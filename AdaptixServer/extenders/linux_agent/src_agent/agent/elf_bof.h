#ifndef ELF_BOF_H
#define ELF_BOF_H

#include "types.h"
#include "crt.h"
#include "msgpack.h"

/// ────────────────────────────────────────────────────────────────────────────
/// ELF64 structures — minimal, zero libc dependency (nostdlib)
/// ────────────────────────────────────────────────────────────────────────────

typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;

/// ELF header
typedef struct {
    unsigned char e_ident[16];
    Elf64_Half    e_type;
    Elf64_Half    e_machine;
    Elf64_Word    e_version;
    Elf64_Addr    e_entry;
    Elf64_Off     e_phoff;
    Elf64_Off     e_shoff;
    Elf64_Word    e_flags;
    Elf64_Half    e_ehsize;
    Elf64_Half    e_phentsize;
    Elf64_Half    e_phnum;
    Elf64_Half    e_shentsize;
    Elf64_Half    e_shnum;
    Elf64_Half    e_shstrndx;
} Elf64_Ehdr;

/// Section header
typedef struct {
    Elf64_Word    sh_name;
    Elf64_Word    sh_type;
    Elf64_Xword   sh_flags;
    Elf64_Addr    sh_addr;
    Elf64_Off     sh_offset;
    Elf64_Xword   sh_size;
    Elf64_Word    sh_link;
    Elf64_Word    sh_info;
    Elf64_Xword   sh_addralign;
    Elf64_Xword   sh_entsize;
} Elf64_Shdr;

/// Symbol table entry
typedef struct {
    Elf64_Word    st_name;
    unsigned char st_info;
    unsigned char st_other;
    Elf64_Half    st_shndx;
    Elf64_Addr    st_value;
    Elf64_Xword   st_size;
} Elf64_Sym;

/// Relocation entry with addend
typedef struct {
    Elf64_Addr    r_offset;
    Elf64_Xword   r_info;
    Elf64_Sxword  r_addend;
} Elf64_Rela;

/// ────────────────────────────────────────────────────────────────────────────
/// ELF macros
/// ────────────────────────────────────────────────────────────────────────────

#define ELF64_R_SYM(i)    ((i) >> 32)
#define ELF64_R_TYPE(i)   ((i) & 0xffffffffL)
#define ELF64_ST_BIND(i)  ((unsigned char)(i) >> 4)
#define ELF64_ST_TYPE(i)  ((i) & 0xf)

/// ELF magic
#define ELFMAG0  0x7f
#define ELFMAG1  'E'
#define ELFMAG2  'L'
#define ELFMAG3  'F'
#define ELFCLASS64 2
#define ELFDATA2LSB 1

/// e_type
#define ET_REL  1

/// e_machine
#define EM_X86_64   62
#define EM_AARCH64  183

/// Section types
#define SHT_NULL      0
#define SHT_PROGBITS  1
#define SHT_SYMTAB    2
#define SHT_STRTAB    3
#define SHT_RELA      4
#define SHT_NOBITS    8

/// Section flags
#define SHF_WRITE     0x1
#define SHF_ALLOC     0x2
#define SHF_EXECINSTR 0x4

/// Symbol binding/type
#define STB_LOCAL   0
#define STB_GLOBAL  1
#define STB_WEAK    2
#define STT_NOTYPE  0
#define STT_FUNC    2

/// Special section indices
#define SHN_UNDEF  0

/// ────────────────────────────────────────────────────────────────────────────
/// Relocation types — x86_64
/// ────────────────────────────────────────────────────────────────────────────

#define R_X86_64_64        1    // S + A (absolute 64-bit)
#define R_X86_64_PC32      2    // S + A - P (32-bit PC-relative)
#define R_X86_64_PLT32     4    // S + A - P (same as PC32 for .o files)
#define R_X86_64_32       10    // S + A (absolute 32-bit, zero-extend)
#define R_X86_64_32S      11    // S + A (absolute 32-bit, sign-extend)

/// ────────────────────────────────────────────────────────────────────────────
/// Relocation types — ARM64 (AArch64)
/// ────────────────────────────────────────────────────────────────────────────

#define R_AARCH64_ABS64              257  // S + A
#define R_AARCH64_CALL26             283  // (S + A - P) >> 2, 26-bit branch
#define R_AARCH64_JUMP26             282  // (S + A - P) >> 2, 26-bit branch
#define R_AARCH64_ADR_PREL_PG_HI21  275  // Page(S+A) - Page(P), bits [32:12]
#define R_AARCH64_ADD_ABS_LO12_NC   277  // (S+A) & 0xFFF, 12-bit
#define R_AARCH64_LDST8_ABS_LO12_NC   278
#define R_AARCH64_LDST16_ABS_LO12_NC  284
#define R_AARCH64_LDST32_ABS_LO12_NC  285
#define R_AARCH64_LDST64_ABS_LO12_NC  286
#define R_AARCH64_LDST128_ABS_LO12_NC 299

/// ────────────────────────────────────────────────────────────────────────────
/// BOF error codes (matches beacon pattern for Go-side ProcessData)
/// ────────────────────────────────────────────────────────────────────────────

#define BOF_ERR_NONE     0
#define BOF_ERR_PARSE    0x101
#define BOF_ERR_SYMBOL   0x102
#define BOF_ERR_ENTRY    0x104
#define BOF_ERR_ALLOC    0x105
#define BOF_ERR_RELOC    0x106

/// Max sections supported
#define BOF_MAX_SECTIONS 32

/// ────────────────────────────────────────────────────────────────────────────
/// Public API
/// ────────────────────────────────────────────────────────────────────────────

/// Execute an ELF BOF in-memory (synchronous).
/// Called from commander.c case COMMAND_EXEC_BOF.
/// Parses msgpack params {content, args, entry_func}, loads ELF, executes, returns output.
int task_exec_bof(uint32_t cmd_id, const uint8_t *data, uint32_t data_len, mp_writer_t *response);

/// Execute an ELF BOF in a background thread (async).
/// Called from commander.c case COMMAND_EXEC_BOF_ASYNC.
/// Spawns a thread that opens its own C2 connection and streams output.
int task_exec_bof_async(uint32_t cmd_id, const uint8_t *data, uint32_t data_len, mp_writer_t *response);

#endif // ELF_BOF_H
