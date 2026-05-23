/// elf_resolve.c -- ELF hash-based API resolver for Linux agent
/// Parses /proc/self/maps → ELF headers → .dynsym/.dynstr → DJB2 match
/// Uses only direct syscalls — zero libc dependency for bootstrapping.

#include "elf_resolve.h"
#include "crt.h"
#include "types.h"

#ifdef ARCH_X86_64
#include "syscalls_x64.h"
#endif
#ifdef ARCH_AARCH64
#include "syscalls_aarch64.h"
#endif

// ── ELF structures (inline — no system headers needed) ──

#define EI_NIDENT   16
#define ELFMAG0     0x7f
#define ELFMAG1     'E'
#define ELFMAG2     'L'
#define ELFMAG3     'F'
#define ELFCLASS64  2
#define PT_LOAD     1
#define PT_DYNAMIC  2
#define DT_NULL     0
#define DT_STRTAB   5
#define DT_SYMTAB   6
#define DT_STRSZ    10
#define DT_GNU_HASH 0x6ffffef5
#define DT_HASH     4

#define STB_GLOBAL  1
#define STB_WEAK    2
#define STT_FUNC    2
#define STT_OBJECT  1
#define SHN_UNDEF   0

#define ELF64_ST_BIND(i) ((i) >> 4)
#define ELF64_ST_TYPE(i) ((i) & 0xf)

typedef struct {
    uint8_t  e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

typedef struct {
    int64_t  d_tag;
    uint64_t d_val;
} Elf64_Dyn;

typedef struct {
    uint32_t st_name;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} Elf64_Sym;

// DT_HASH header
typedef struct {
    uint32_t nbucket;
    uint32_t nchain;
    // followed by: uint32_t bucket[nbucket]; uint32_t chain[nchain];
} Elf_Hash;

// DT_GNU_HASH header
typedef struct {
    uint32_t nbuckets;
    uint32_t symndx;
    uint32_t maskwords;
    uint32_t shift2;
    // followed by: uint64_t bloom[maskwords]; uint32_t buckets[nbuckets]; uint32_t chains[];
} Gnu_Hash;

// ── Parsed library entry ──
typedef struct {
    uintptr_t base;          // lowest mapping address
    uint32_t  name_hash;     // DJB2 hash of basename
} lib_entry_t;

#define MAX_LIBS 32

/// Global resolved API table
resolved_apis_t g_apis;

/// DJB2 hash — case-insensitive, seeded
uint32_t djb2_hash(uint32_t seed, const char *s)
{
    uint32_t h = seed;
    while (*s) {
        char c = *s++;
        if (c >= 'A' && c <= 'Z')
            c += 32;
        h = ((h << 5) + h) + (uint32_t)c;
    }
    return h;
}

// ── Internal helpers ──

#define O_RDONLY 0

/// Extract basename from "/lib/x86_64-linux-gnu/libc.so.6" → "libc.so.6"
static const char *path_basename(const char *path) {
    const char *last = path;
    while (*path) {
        if (*path == '/') last = path + 1;
        path++;
    }
    return last;
}

/// Validate ELF64 magic at a given memory address
static int is_valid_elf64(const void *addr) {
    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)addr;
    return (ehdr->e_ident[0] == ELFMAG0 &&
            ehdr->e_ident[1] == ELFMAG1 &&
            ehdr->e_ident[2] == ELFMAG2 &&
            ehdr->e_ident[3] == ELFMAG3 &&
            ehdr->e_ident[4] == ELFCLASS64);
}

/// Parse /proc/self/maps to find loaded shared libraries.
/// Format: "addr1-addr2 perms offset dev inode path\n"
/// We only care about r-xp (executable) mappings with a library path.
static int parse_proc_maps(lib_entry_t *libs, int max_libs) {
    int fd = sys_open("/proc/self/maps", O_RDONLY, 0);
    if (fd < 0) return 0;

    // Read maps in chunks — typical maps file is 2-20 KB
    char buf[8192];
    int lib_count = 0;
    int buf_used = 0;

    for (;;) {
        long n = sys_read(fd, buf + buf_used, (size_t)(sizeof(buf) - 1 - buf_used));
        if (n <= 0) break;
        buf_used += (int)n;
        buf[buf_used] = '\0';

        // Process complete lines
        char *line_start = buf;
        char *newline;
        while ((newline = ax_strchr(line_start, '\n')) != NULL) {
            *newline = '\0';

            // Parse: "7f1234560000-7f1234570000 r-xp 00001000 08:01 12345 /lib/x86_64-linux-gnu/libc.so.6"
            //         ^addr_start               ^perms                       ^path

            // Find permissions field (after first space)
            char *p = ax_strchr(line_start, ' ');
            if (p && p[1] == 'r' && p[4] == 'p') {
                // Find the path (last field, starts with '/')
                // Skip: addr, perms, offset, dev, inode → path
                char *path = NULL;
                int space_count = 0;
                for (char *s = line_start; *s; s++) {
                    if (*s == '/') {
                        // Check this is actually a path (after inode field)
                        // Count at least 4 spaces before this
                        int sc = 0;
                        for (char *t = line_start; t < s; t++) {
                            if (*t == ' ') sc++;
                        }
                        if (sc >= 4) {
                            path = s;
                            break;
                        }
                    }
                }
                (void)space_count;

                if (path && ax_strstr(path, ".so")) {
                    // Parse base address (hex before '-')
                    uintptr_t base_addr = 0;
                    for (char *h = line_start; *h && *h != '-'; h++) {
                        int digit;
                        if (*h >= '0' && *h <= '9') digit = *h - '0';
                        else if (*h >= 'a' && *h <= 'f') digit = *h - 'a' + 10;
                        else if (*h >= 'A' && *h <= 'F') digit = *h - 'A' + 10;
                        else break;
                        base_addr = base_addr * 16 + digit;
                    }

                    const char *bn = path_basename(path);
                    uint32_t h = djb2_hash(DJB2_SEED, bn);

                    // Check if we already have this lib (maps has multiple segments per lib)
                    int found = 0;
                    for (int i = 0; i < lib_count; i++) {
                        if (libs[i].name_hash == h) {
                            // Keep lowest base address
                            if (base_addr < libs[i].base)
                                libs[i].base = base_addr;
                            found = 1;
                            break;
                        }
                    }

                    if (!found && lib_count < max_libs) {
                        libs[lib_count].base = base_addr;
                        libs[lib_count].name_hash = h;
                        lib_count++;
                    }
                }
            }

            line_start = newline + 1;
        }

        // Move remaining partial line to beginning
        int remaining = buf_used - (int)(line_start - buf);
        if (remaining > 0)
            ax_memmove(buf, line_start, remaining);
        buf_used = remaining;
    }

    sys_close(fd);
    return lib_count;
}

/// Resolve a loaded shared library by DJB2 hash of its basename
void *elf_resolve_lib(uint32_t name_hash)
{
    lib_entry_t libs[MAX_LIBS];
    int count = parse_proc_maps(libs, MAX_LIBS);

    for (int i = 0; i < count; i++) {
        if (libs[i].name_hash == name_hash) {
            // Validate ELF magic at base
            if (is_valid_elf64((void *)libs[i].base))
                return (void *)libs[i].base;
        }
    }
    return NULL;
}

/// Resolve a symbol within an ELF64 library by DJB2 hash.
/// Walks PT_DYNAMIC → DT_SYMTAB + DT_STRTAB, then iterates symbols.
void *elf_resolve_sym(void *lib_base, uint32_t symbol_hash)
{
    if (!lib_base) return NULL;

    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)lib_base;
    if (!is_valid_elf64(ehdr)) return NULL;

    uintptr_t base = (uintptr_t)lib_base;

    // Find PT_DYNAMIC and PT_LOAD[0] for bias calculation
    const Elf64_Phdr *phdr = (const Elf64_Phdr *)(base + ehdr->e_phoff);
    const Elf64_Dyn *dyn = NULL;
    uintptr_t load_bias = 0;
    int found_load = 0;

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_DYNAMIC) {
            dyn = (const Elf64_Dyn *)(base + phdr[i].p_offset);
        }
        if (phdr[i].p_type == PT_LOAD && !found_load) {
            // Load bias = actual base - expected vaddr of first PT_LOAD
            load_bias = base - phdr[i].p_vaddr;
            found_load = 1;
        }
    }

    if (!dyn) return NULL;

    // Extract DT_SYMTAB, DT_STRTAB, DT_HASH/DT_GNU_HASH from PT_DYNAMIC
    const Elf64_Sym *symtab = NULL;
    const char *strtab = NULL;
    const Elf_Hash *elf_hash = NULL;
    const Gnu_Hash *gnu_hash = NULL;
    uint64_t strsz = 0;

    for (const Elf64_Dyn *d = dyn; d->d_tag != DT_NULL; d++) {
        switch (d->d_tag) {
        case DT_SYMTAB:   symtab   = (const Elf64_Sym *)(load_bias + d->d_val); break;
        case DT_STRTAB:   strtab   = (const char *)(load_bias + d->d_val);      break;
        case DT_STRSZ:    strsz    = d->d_val;                                  break;
        case DT_HASH:     elf_hash = (const Elf_Hash *)(load_bias + d->d_val);   break;
        case DT_GNU_HASH: gnu_hash = (const Gnu_Hash *)(load_bias + d->d_val);   break;
        }
    }

    if (!symtab || !strtab) return NULL;

    // Determine symbol count.
    // If DT_HASH is available, nchain == total symbol count.
    // If only DT_GNU_HASH, we need to walk the chain to find max index.
    uint32_t nsyms = 0;

    if (elf_hash) {
        nsyms = elf_hash->nchain;
    } else if (gnu_hash) {
        // Walk GNU hash chains to find the highest symbol index
        const uint64_t *bloom = (const uint64_t *)(gnu_hash + 1);
        const uint32_t *buckets = (const uint32_t *)(bloom + gnu_hash->maskwords);
        const uint32_t *chains = buckets + gnu_hash->nbuckets;

        // Find highest occupied bucket
        uint32_t max_idx = 0;
        for (uint32_t i = 0; i < gnu_hash->nbuckets; i++) {
            if (buckets[i] > max_idx)
                max_idx = buckets[i];
        }

        if (max_idx >= gnu_hash->symndx) {
            // Walk chain from max bucket entry to find last symbol
            uint32_t idx = max_idx;
            while (!(chains[idx - gnu_hash->symndx] & 1))
                idx++;
            nsyms = idx + 1;
        } else {
            nsyms = gnu_hash->symndx;
        }
    }

    if (nsyms == 0) return NULL;

    // Linear walk over .dynsym — hash each exported symbol name
    for (uint32_t i = 0; i < nsyms; i++) {
        const Elf64_Sym *sym = &symtab[i];

        // Skip undefined symbols
        if (sym->st_shndx == SHN_UNDEF) continue;

        // Skip non-global/non-weak
        uint8_t bind = ELF64_ST_BIND(sym->st_info);
        if (bind != STB_GLOBAL && bind != STB_WEAK) continue;

        // Skip non-function/non-object
        uint8_t type = ELF64_ST_TYPE(sym->st_info);
        if (type != STT_FUNC && type != STT_OBJECT) continue;

        // Get symbol name from strtab
        uint32_t name_off = sym->st_name;
        if (name_off == 0 || name_off >= strsz) continue;
        const char *name = strtab + name_off;
        if (*name == '\0') continue;

        if (djb2_hash(DJB2_SEED, name) == symbol_hash) {
            return (void *)(load_bias + sym->st_value);
        }
    }

    return NULL;
}

/// Initialize the resolver — resolve all APIs from loaded libraries
int elf_resolver_init(void)
{
    ax_memset(&g_apis, 0, sizeof(g_apis));

    // Parse /proc/self/maps to find all loaded libraries
    lib_entry_t libs[MAX_LIBS];
    int lib_count = parse_proc_maps(libs, MAX_LIBS);
    if (lib_count == 0) return -1;

    // Collect valid ELF image bases
    void *images[MAX_LIBS];
    int image_count = 0;

    for (int i = 0; i < lib_count; i++) {
        void *base = (void *)libs[i].base;
        if (is_valid_elf64(base)) {
            images[image_count++] = base;
        }
    }

    if (image_count == 0) return -1;

    // Resolve macro: try all images until found
    #define RESOLVE(field, name_str) do { \
        uint32_t _h = djb2_hash(DJB2_SEED, name_str); \
        for (int _i = 0; _i < image_count && !g_apis.field; _i++) { \
            g_apis.field = elf_resolve_sym(images[_i], _h); \
        } \
    } while(0)

    // ── File I/O ──
    RESOLVE(fn_open,           "open");
    RESOLVE(fn_close,          "close");
    RESOLVE(fn_read,           "read");
    RESOLVE(fn_write,          "write");
    RESOLVE(fn_stat,           "stat");
    RESOLVE(fn_fstat,          "fstat");
    RESOLVE(fn_unlink,         "unlink");
    RESOLVE(fn_rename,         "rename");
    RESOLVE(fn_mkdir,          "mkdir");
    RESOLVE(fn_opendir,        "opendir");
    RESOLVE(fn_readdir,        "readdir");
    RESOLVE(fn_closedir,       "closedir");
    RESOLVE(fn_getcwd,         "getcwd");
    RESOLVE(fn_chdir,          "chdir");
    RESOLVE(fn_rmdir,          "rmdir");
    RESOLVE(fn_rewinddir,      "rewinddir");

    // ── Memory ──
    RESOLVE(fn_mmap,           "mmap");
    RESOLVE(fn_munmap,         "munmap");
    RESOLVE(fn_mprotect,       "mprotect");

    // ── Process ──
    RESOLVE(fn_fork,           "fork");
    RESOLVE(fn_execve,         "execve");
    RESOLVE(fn_execvp,         "execvp");
    RESOLVE(fn_waitpid,        "waitpid");
    RESOLVE(fn_getpid,         "getpid");
    RESOLVE(fn_getuid,         "getuid");
    RESOLVE(fn_geteuid,        "geteuid");
    RESOLVE(fn_kill,           "kill");
    RESOLVE(fn_setsid,         "setsid");
    RESOLVE(fn_setpgid,        "setpgid");
    RESOLVE(fn_exit,           "_exit");
    RESOLVE(fn_prctl,          "prctl");

    // ── Network ──
    RESOLVE(fn_socket,         "socket");
    RESOLVE(fn_connect,        "connect");
    RESOLVE(fn_getaddrinfo,    "getaddrinfo");
    RESOLVE(fn_freeaddrinfo,   "freeaddrinfo");
    RESOLVE(fn_gethostname,    "gethostname");
    RESOLVE(fn_setsockopt,     "setsockopt");
    RESOLVE(fn_getsockopt,     "getsockopt");
    RESOLVE(fn_select,         "select");
    RESOLVE(fn_send,           "send");
    RESOLVE(fn_recv,           "recv");
    RESOLVE(fn_bind,           "bind");
    RESOLVE(fn_listen,         "listen");
    RESOLVE(fn_accept,         "accept");

    // ── Threading ──
    // On glibc 2.34+, pthread is merged into libc.so.6
    RESOLVE(fn_pthread_create,       "pthread_create");
    RESOLVE(fn_pthread_detach,       "pthread_detach");
    RESOLVE(fn_pthread_mutex_init,   "pthread_mutex_init");
    RESOLVE(fn_pthread_mutex_lock,   "pthread_mutex_lock");
    RESOLVE(fn_pthread_mutex_unlock, "pthread_mutex_unlock");

    // ── Pipes & PTY ──
    RESOLVE(fn_pipe,           "pipe");
    RESOLVE(fn_dup2,           "dup2");
    RESOLVE(fn_fcntl,          "fcntl");
    RESOLVE(fn_posix_openpt,   "posix_openpt");
    RESOLVE(fn_grantpt,        "grantpt");
    RESOLVE(fn_unlockpt,       "unlockpt");
    RESOLVE(fn_ptsname,        "ptsname");
    RESOLVE(fn_ioctl,          "ioctl");

    // ── System ──
    RESOLVE(fn_getenv,         "getenv");
    RESOLVE(fn_setenv,         "setenv");
    RESOLVE(fn_sleep,          "sleep");
    RESOLVE(fn_usleep,         "usleep");
    RESOLVE(fn_snprintf,       "snprintf");
    RESOLVE(fn_strtol,         "strtol");

    // ── User/Group ──
    RESOLVE(fn_getpwuid,       "getpwuid");
    RESOLVE(fn_getgrgid,       "getgrgid");
    RESOLVE(fn_getifaddrs,     "getifaddrs");
    RESOLVE(fn_freeifaddrs,    "freeifaddrs");
    RESOLVE(fn_inet_ntop,      "inet_ntop");
    RESOLVE(fn_localtime,      "localtime");
    RESOLVE(fn_strftime,       "strftime");

    // ── Dynamic ──
    RESOLVE(fn_dlopen,         "dlopen");
    RESOLVE(fn_dlsym,          "dlsym");
    RESOLVE(fn_dlclose,        "dlclose");

    #undef RESOLVE

    return 0;
}
