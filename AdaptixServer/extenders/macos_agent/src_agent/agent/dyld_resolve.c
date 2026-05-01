#include "dyld_resolve.h"
#include "crt.h"

#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/dyld.h>

/// Global resolved API table
resolved_apis_t g_apis;

/// DJB2 hash — case-insensitive, seeded
/// Matches Go's djb2a(seed, strings.ToLower(s))
uint32_t djb2_hash(const char* str) {
    if (!str) return 0;
    uint32_t hash = DJB2_SEED;
    int c;
    while ((c = *str++)) {
        if (c >= 'A' && c <= 'Z')
            c += 0x20;  // lowercase
        hash = ((hash << 5) + hash) + (uint32_t)c;
    }
    return hash;
}

/// Extract basename from a path: "/usr/lib/libSystem.B.dylib" → "libSystem.B.dylib"
static const char* path_basename(const char* path) {
    const char* last = path;
    while (*path) {
        if (*path == '/') last = path + 1;
        path++;
    }
    return last;
}

/// Resolve a dylib by DJB2 hash of its basename
void* dyld_resolve_lib(uint32_t name_hash) {
    uint32_t count = _dyld_image_count();
    for (uint32_t i = 0; i < count; i++) {
        const char* name = _dyld_get_image_name(i);
        if (!name) continue;

        const char* base = path_basename(name);
        if (djb2_hash(base) == name_hash) {
            return (void*)_dyld_get_image_header(i);
        }
    }
    return (void*)0;
}

/// Resolve a symbol within a Mach-O image by DJB2 hash
/// Parses the Mach-O header → finds LC_SYMTAB → walks nlist entries
void* dyld_resolve_sym(void* image_header, uint32_t symbol_hash) {
    if (!image_header) return (void*)0;

    const struct mach_header_64* header = (const struct mach_header_64*)image_header;

    // Verify magic
    if (header->magic != MH_MAGIC_64) return (void*)0;

    // Find LC_SYMTAB load command
    const struct load_command* cmd = (const struct load_command*)(header + 1);
    const struct symtab_command* symtab = (const struct symtab_command*)0;
    intptr_t slide = 0;
    intptr_t linkedit_base = 0;
    intptr_t text_base = 0;

    // First pass: find __LINKEDIT and __TEXT segments for slide calculation
    const struct load_command* cmd_iter = cmd;
    for (uint32_t i = 0; i < header->ncmds; i++) {
        if (cmd_iter->cmd == LC_SEGMENT_64) {
            const struct segment_command_64* seg = (const struct segment_command_64*)cmd_iter;
            if (seg->segname[0] == '_' && seg->segname[1] == '_' &&
                seg->segname[2] == 'L' && seg->segname[3] == 'I' &&
                seg->segname[4] == 'N' && seg->segname[5] == 'K') {
                // __LINKEDIT
                linkedit_base = (intptr_t)(seg->vmaddr - seg->fileoff);
            }
            if (seg->segname[0] == '_' && seg->segname[1] == '_' &&
                seg->segname[2] == 'T' && seg->segname[3] == 'E' &&
                seg->segname[4] == 'X' && seg->segname[5] == 'T' &&
                seg->segname[6] == '\0') {
                // __TEXT
                text_base = (intptr_t)seg->vmaddr;
            }
        }
        cmd_iter = (const struct load_command*)((const uint8_t*)cmd_iter + cmd_iter->cmdsize);
    }

    // Calculate ASLR slide
    slide = (intptr_t)header - text_base;

    // Second pass: find LC_SYMTAB
    cmd_iter = cmd;
    for (uint32_t i = 0; i < header->ncmds; i++) {
        if (cmd_iter->cmd == LC_SYMTAB) {
            symtab = (const struct symtab_command*)cmd_iter;
            break;
        }
        cmd_iter = (const struct load_command*)((const uint8_t*)cmd_iter + cmd_iter->cmdsize);
    }

    if (!symtab || symtab->nsyms == 0) return (void*)0;

    // Get string table and symbol table addresses
    const char* strtab = (const char*)(linkedit_base + slide + symtab->stroff);
    const struct nlist_64* symbols = (const struct nlist_64*)(linkedit_base + slide + symtab->symoff);

    // Walk symbol table
    for (uint32_t i = 0; i < symtab->nsyms; i++) {
        const struct nlist_64* sym = &symbols[i];

        // Skip debug, undefined, and non-external symbols
        if ((sym->n_type & N_STAB) != 0) continue;  // debug symbol
        if ((sym->n_type & N_TYPE) == N_UNDF) continue;  // undefined
        if ((sym->n_type & N_EXT) == 0) continue;  // not exported

        uint32_t str_idx = sym->n_un.n_strx;
        const char* sym_name = &strtab[str_idx];

        // Skip leading underscore (Mach-O convention)
        if (sym_name[0] == '_') sym_name++;

        if (djb2_hash(sym_name) == symbol_hash) {
            return (void*)((intptr_t)sym->n_value + slide);
        }
    }

    return (void*)0;
}

/// Initialize resolver — resolve critical APIs from libSystem
int dyld_resolver_init(void) {
    ax_memset(&g_apis, 0, sizeof(g_apis));

    // libSystem.B.dylib contains all BSD/POSIX functions on macOS
    // It's always loaded (it's the macOS equivalent of libc)

    // We need to find libSystem by hash
    // Try multiple names since dyld may list it differently
    void* libsystem = (void*)0;
    uint32_t count = _dyld_image_count();

    for (uint32_t i = 0; i < count; i++) {
        const char* name = _dyld_get_image_name(i);
        if (!name) continue;
        const char* base = path_basename(name);

        // Match "libSystem.B.dylib" or "libsystem_*" components
        // But we primarily want libSystem.B.dylib which re-exports everything
        uint32_t h = djb2_hash(base);

        // Check against our expected hash
        // Note: the hash value depends on DJB2_SEED, so we compute at runtime
        uint32_t expected = djb2_hash("libSystem.B.dylib");
        if (h == expected) {
            libsystem = (void*)_dyld_get_image_header(i);
            break;
        }
    }

    // If not found by exact name, try to find libc or libsystem_c
    if (!libsystem) {
        for (uint32_t i = 0; i < count; i++) {
            const char* name = _dyld_get_image_name(i);
            if (!name) continue;
            const char* base = path_basename(name);
            if (djb2_hash(base) == djb2_hash("libsystem_c.dylib")) {
                libsystem = (void*)_dyld_get_image_header(i);
                break;
            }
        }
    }

    if (!libsystem) return -1;

    // libSystem.B.dylib re-exports from sub-libraries
    // Symbols may be in libsystem_c, libsystem_kernel, libsystem_pthread, etc.
    // We need to search multiple images

    // Collect all libsystem_* images
    void* images[32];
    int image_count = 0;

    for (uint32_t i = 0; i < count && image_count < 32; i++) {
        const char* name = _dyld_get_image_name(i);
        if (!name) continue;

        // Check if path contains "libsystem_" or "libSystem"
        int is_system = 0;
        const char* p = name;
        while (*p) {
            if (p[0] == 'l' && p[1] == 'i' && p[2] == 'b') {
                if ((p[3] == 's' || p[3] == 'S') &&
                    (p[4] == 'y' || p[4] == 'Y') &&
                    (p[5] == 's' || p[5] == 'S')) {
                    is_system = 1;
                    break;
                }
            }
            p++;
        }
        if (is_system) {
            images[image_count++] = (void*)_dyld_get_image_header(i);
        }
    }

    // Also add libpthread if present
    for (uint32_t i = 0; i < count && image_count < 32; i++) {
        const char* name = _dyld_get_image_name(i);
        if (!name) continue;
        const char* base = path_basename(name);
        if (base[0] == 'l' && base[1] == 'i' && base[2] == 'b' &&
            base[3] == 'p' && base[4] == 't' && base[5] == 'h') {
            images[image_count++] = (void*)_dyld_get_image_header(i);
            break;
        }
    }

    // Helper: resolve across all system images
    #define RESOLVE(field, name_str) do { \
        uint32_t _h = djb2_hash(name_str); \
        for (int _i = 0; _i < image_count && !g_apis.field; _i++) { \
            g_apis.field = dyld_resolve_sym(images[_i], _h); \
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
    RESOLVE(fn_copyfile,       "copyfile");
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
    RESOLVE(fn_execl,          "execl");
    RESOLVE(fn_execlp,         "execlp");
    RESOLVE(fn_waitpid,        "waitpid");
    RESOLVE(fn_getpid,         "getpid");
    RESOLVE(fn_getuid,         "getuid");
    RESOLVE(fn_geteuid,        "geteuid");
    RESOLVE(fn_kill,           "kill");
    RESOLVE(fn_killpg,         "killpg");
    RESOLVE(fn_setsid,         "setsid");
    RESOLVE(fn_setpgid,        "setpgid");
    RESOLVE(fn_exit,           "_exit");

    // ── Network ──
    RESOLVE(fn_socket,         "socket");
    RESOLVE(fn_connect,        "connect");
    RESOLVE(fn_getaddrinfo,    "getaddrinfo");
    RESOLVE(fn_freeaddrinfo,   "freeaddrinfo");
    RESOLVE(fn_gethostname,    "gethostname");
    RESOLVE(fn_getsockopt,     "getsockopt");
    RESOLVE(fn_setsockopt,     "setsockopt");
    RESOLVE(fn_select,         "select");

    // ── System ──
    RESOLVE(fn_sysctl,         "sysctl");
    RESOLVE(fn_sysctlbyname,   "sysctlbyname");
    RESOLVE(fn_getenv,         "getenv");
    RESOLVE(fn_setenv,         "setenv");
    RESOLVE(fn_sleep,          "sleep");
    RESOLVE(fn_usleep,         "usleep");

    // ── Pipes & PTY ──
    RESOLVE(fn_pipe,           "pipe");
    RESOLVE(fn_dup2,           "dup2");
    RESOLVE(fn_fcntl,          "fcntl");
    RESOLVE(fn_posix_openpt,   "posix_openpt");
    RESOLVE(fn_grantpt,        "grantpt");
    RESOLVE(fn_unlockpt,       "unlockpt");
    RESOLVE(fn_ptsname,        "ptsname");
    RESOLVE(fn_ioctl,          "ioctl");

    // ── Threading ──
    RESOLVE(fn_pthread_create,       "pthread_create");
    RESOLVE(fn_pthread_detach,       "pthread_detach");
    RESOLVE(fn_pthread_mutex_init,   "pthread_mutex_init");
    RESOLVE(fn_pthread_mutex_lock,   "pthread_mutex_lock");
    RESOLVE(fn_pthread_mutex_unlock, "pthread_mutex_unlock");

    // ── Crypto/Random ──
    RESOLVE(fn_arc4random_buf, "arc4random_buf");

    // ── String/Misc ──
    RESOLVE(fn_dlopen,         "dlopen");
    RESOLVE(fn_dlsym,          "dlsym");
    RESOLVE(fn_dlclose,        "dlclose");

    // ── macOS-specific ──
    RESOLVE(fn_getpwuid,       "getpwuid");
    RESOLVE(fn_getgrgid,       "getgrgid");
    RESOLVE(fn_getifaddrs,     "getifaddrs");
    RESOLVE(fn_freeifaddrs,    "freeifaddrs");
    RESOLVE(fn_inet_ntop,      "inet_ntop");
    RESOLVE(fn_localtime,      "localtime");
    RESOLVE(fn_strftime,       "strftime");

    #undef RESOLVE

    return 0;
}
