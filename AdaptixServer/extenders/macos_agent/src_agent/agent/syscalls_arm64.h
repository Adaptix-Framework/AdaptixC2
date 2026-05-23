#ifndef SYSCALLS_ARM64_H
#define SYSCALLS_ARM64_H

#include <stdint.h>
#include <stddef.h>

/// ARM64 macOS direct syscalls via SVC #0x80
/// BSD syscall numbers are 0x2000000 | bsd_number
/// Mach traps are negative numbers (not used here)
///
/// Bypasses userland hooks on libSystem functions

#define SYS_CLASS_UNIX  0x2000000

// BSD syscall numbers (macOS ARM64)
#define SYS_exit        (SYS_CLASS_UNIX | 1)
#define SYS_fork        (SYS_CLASS_UNIX | 2)
#define SYS_read        (SYS_CLASS_UNIX | 3)
#define SYS_write       (SYS_CLASS_UNIX | 4)
#define SYS_open        (SYS_CLASS_UNIX | 5)
#define SYS_close       (SYS_CLASS_UNIX | 6)
#define SYS_kill        (SYS_CLASS_UNIX | 37)
#define SYS_getpid      (SYS_CLASS_UNIX | 20)
#define SYS_getuid      (SYS_CLASS_UNIX | 24)
#define SYS_ptrace      (SYS_CLASS_UNIX | 26)
#define SYS_socket      (SYS_CLASS_UNIX | 97)
#define SYS_connect     (SYS_CLASS_UNIX | 98)
#define SYS_mmap        (SYS_CLASS_UNIX | 197)
#define SYS_munmap      (SYS_CLASS_UNIX | 73)
#define SYS_mprotect    (SYS_CLASS_UNIX | 74)
#define SYS_sysctl      (SYS_CLASS_UNIX | 202)
#define SYS_sysctlbyname (SYS_CLASS_UNIX | 274)
#define SYS_stat64      (SYS_CLASS_UNIX | 338)
#define SYS_fstat64     (SYS_CLASS_UNIX | 339)
#define SYS_getdirentries64 (SYS_CLASS_UNIX | 344)
#define SYS_csops       (SYS_CLASS_UNIX | 169)

// ptrace requests
#define PT_DENY_ATTACH  31
#define PT_TRACE_ME     0

/// Raw syscall wrappers — ARM64 ABI
/// x16 = syscall number, x0-x5 = args, SVC #0x80
/// Returns x0 (result), carry flag set on error

static inline long raw_syscall0(long number) {
    register long x16 __asm__("x16") = number;
    register long x0  __asm__("x0");
    __asm__ volatile(
        "svc #0x80\n"
        : "=r"(x0)
        : "r"(x16)
        : "memory", "cc"
    );
    return x0;
}

static inline long raw_syscall1(long number, long a0) {
    register long x16 __asm__("x16") = number;
    register long x0  __asm__("x0")  = a0;
    __asm__ volatile(
        "svc #0x80\n"
        : "+r"(x0)
        : "r"(x16)
        : "memory", "cc"
    );
    return x0;
}

static inline long raw_syscall2(long number, long a0, long a1) {
    register long x16 __asm__("x16") = number;
    register long x0  __asm__("x0")  = a0;
    register long x1  __asm__("x1")  = a1;
    __asm__ volatile(
        "svc #0x80\n"
        : "+r"(x0)
        : "r"(x16), "r"(x1)
        : "memory", "cc"
    );
    return x0;
}

static inline long raw_syscall3(long number, long a0, long a1, long a2) {
    register long x16 __asm__("x16") = number;
    register long x0  __asm__("x0")  = a0;
    register long x1  __asm__("x1")  = a1;
    register long x2  __asm__("x2")  = a2;
    __asm__ volatile(
        "svc #0x80\n"
        : "+r"(x0)
        : "r"(x16), "r"(x1), "r"(x2)
        : "memory", "cc"
    );
    return x0;
}

static inline long raw_syscall4(long number, long a0, long a1, long a2, long a3) {
    register long x16 __asm__("x16") = number;
    register long x0  __asm__("x0")  = a0;
    register long x1  __asm__("x1")  = a1;
    register long x2  __asm__("x2")  = a2;
    register long x3  __asm__("x3")  = a3;
    __asm__ volatile(
        "svc #0x80\n"
        : "+r"(x0)
        : "r"(x16), "r"(x1), "r"(x2), "r"(x3)
        : "memory", "cc"
    );
    return x0;
}

static inline long raw_syscall6(long number, long a0, long a1, long a2,
                                 long a3, long a4, long a5) {
    register long x16 __asm__("x16") = number;
    register long x0  __asm__("x0")  = a0;
    register long x1  __asm__("x1")  = a1;
    register long x2  __asm__("x2")  = a2;
    register long x3  __asm__("x3")  = a3;
    register long x4  __asm__("x4")  = a4;
    register long x5  __asm__("x5")  = a5;
    __asm__ volatile(
        "svc #0x80\n"
        : "+r"(x0)
        : "r"(x16), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)
        : "memory", "cc"
    );
    return x0;
}

/// Convenience wrappers for common syscalls

static inline int sys_open(const char* path, int flags, int mode) {
    return (int)raw_syscall3(SYS_open, (long)path, (long)flags, (long)mode);
}

static inline int sys_close(int fd) {
    return (int)raw_syscall1(SYS_close, (long)fd);
}

static inline long sys_read(int fd, void* buf, size_t count) {
    return raw_syscall3(SYS_read, (long)fd, (long)buf, (long)count);
}

static inline long sys_write(int fd, const void* buf, size_t count) {
    return raw_syscall3(SYS_write, (long)fd, (long)buf, (long)count);
}

static inline int sys_getpid(void) {
    return (int)raw_syscall0(SYS_getpid);
}

static inline int sys_getuid(void) {
    return (int)raw_syscall0(SYS_getuid);
}

static inline int sys_kill(int pid, int sig) {
    return (int)raw_syscall2(SYS_kill, (long)pid, (long)sig);
}

static inline int sys_ptrace(int request, int pid, void* addr, int data) {
    return (int)raw_syscall4(SYS_ptrace, (long)request, (long)pid, (long)addr, (long)data);
}

static inline void* sys_mmap(void* addr, size_t len, int prot, int flags, int fd, long offset) {
    return (void*)raw_syscall6(SYS_mmap, (long)addr, (long)len, (long)prot,
                                (long)flags, (long)fd, offset);
}

static inline int sys_munmap(void* addr, size_t len) {
    return (int)raw_syscall2(SYS_munmap, (long)addr, (long)len);
}

static inline int sys_mprotect(void* addr, size_t len, int prot) {
    return (int)raw_syscall3(SYS_mprotect, (long)addr, (long)len, (long)prot);
}

static inline int sys_sysctl(int* name, unsigned int namelen, void* oldp,
                              size_t* oldlenp, void* newp, size_t newlen) {
    return (int)raw_syscall6(SYS_sysctl, (long)name, (long)namelen, (long)oldp,
                              (long)oldlenp, (long)newp, (long)newlen);
}

static inline int sys_fork(void) {
    return (int)raw_syscall0(SYS_fork);
}

static inline int sys_csops(int pid, unsigned int ops, void* useraddr, size_t usersize) {
    return (int)raw_syscall4(SYS_csops, (long)pid, (long)ops, (long)useraddr, (long)usersize);
}

#endif // SYSCALLS_ARM64_H
