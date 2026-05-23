#ifndef SYSCALLS_AARCH64_H
#define SYSCALLS_AARCH64_H

#ifdef ARCH_AARCH64

#include <stdint.h>
#include <stddef.h>

/// Linux ARM64 syscall numbers (different from macOS ARM64!)
/// macOS uses x16 + svc #0x80 — Linux uses x8 + svc #0
#define __NR_ioctl           29
#define __NR_fcntl           25
#define __NR_openat          56
#define __NR_close           57
#define __NR_read            63
#define __NR_write           64
#define __NR_fstatat         79
#define __NR_fstat           80
#define __NR_exit            93
#define __NR_exit_group      94
#define __NR_kill            129
#define __NR_getpid          172
#define __NR_getuid          174
#define __NR_geteuid         175
#define __NR_ptrace          117
#define __NR_clone           220
#define __NR_execve          221
#define __NR_mmap            222
#define __NR_mprotect        226
#define __NR_munmap          215
#define __NR_socket          198
#define __NR_connect         203
#define __NR_accept          202
#define __NR_bind            200
#define __NR_listen          201
#define __NR_setsockopt      208
#define __NR_getsockopt      209
#define __NR_getcwd          17
#define __NR_chdir           49
#define __NR_mkdirat         34
#define __NR_unlinkat        35
#define __NR_renameat        38
#define __NR_getdents64      61
#define __NR_dup3            24
#define __NR_pipe2           59
#define __NR_prctl           167
#define __NR_utimensat       88
#define __NR_bpf             280
#define __NR_getrandom       278
#define __NR_memfd_create    279
#define __NR_waitid          95
#define __NR_wait4           260
#define __NR_nanosleep       101
#define __NR_setsid          157
#define __NR_setpgid         154
#define __NR_pselect6        72

/// Raw syscall wrappers — inline assembly
/// Convention: x8=nr, x0-x5=args, svc #0

static inline long raw_syscall0(long number) {
    register long x8 __asm__("x8") = number;
    register long x0 __asm__("x0");
    __asm__ volatile(
        "svc #0"
        : "=r"(x0)
        : "r"(x8)
        : "memory", "cc"
    );
    return x0;
}

static inline long raw_syscall1(long number, long a0) {
    register long x8 __asm__("x8") = number;
    register long x0 __asm__("x0") = a0;
    __asm__ volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x8)
        : "memory", "cc"
    );
    return x0;
}

static inline long raw_syscall2(long number, long a0, long a1) {
    register long x8 __asm__("x8") = number;
    register long x0 __asm__("x0") = a0;
    register long x1 __asm__("x1") = a1;
    __asm__ volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1)
        : "memory", "cc"
    );
    return x0;
}

static inline long raw_syscall3(long number, long a0, long a1, long a2) {
    register long x8 __asm__("x8") = number;
    register long x0 __asm__("x0") = a0;
    register long x1 __asm__("x1") = a1;
    register long x2 __asm__("x2") = a2;
    __asm__ volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2)
        : "memory", "cc"
    );
    return x0;
}

static inline long raw_syscall4(long number, long a0, long a1, long a2, long a3) {
    register long x8 __asm__("x8") = number;
    register long x0 __asm__("x0") = a0;
    register long x1 __asm__("x1") = a1;
    register long x2 __asm__("x2") = a2;
    register long x3 __asm__("x3") = a3;
    __asm__ volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2), "r"(x3)
        : "memory", "cc"
    );
    return x0;
}

static inline long raw_syscall5(long number, long a0, long a1, long a2, long a3, long a4) {
    register long x8 __asm__("x8") = number;
    register long x0 __asm__("x0") = a0;
    register long x1 __asm__("x1") = a1;
    register long x2 __asm__("x2") = a2;
    register long x3 __asm__("x3") = a3;
    register long x4 __asm__("x4") = a4;
    __asm__ volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4)
        : "memory", "cc"
    );
    return x0;
}

static inline long raw_syscall6(long number, long a0, long a1, long a2, long a3, long a4, long a5) {
    register long x8 __asm__("x8") = number;
    register long x0 __asm__("x0") = a0;
    register long x1 __asm__("x1") = a1;
    register long x2 __asm__("x2") = a2;
    register long x3 __asm__("x3") = a3;
    register long x4 __asm__("x4") = a4;
    register long x5 __asm__("x5") = a5;
    __asm__ volatile(
        "svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)
        : "memory", "cc"
    );
    return x0;
}

/// Convenience wrappers

// Note: ARM64 Linux has no open() — use openat(AT_FDCWD, ...)
#define AT_FDCWD -100

static inline int sys_openat(int dirfd, const char *path, int flags, int mode) {
    return (int)raw_syscall4(__NR_openat, dirfd, (long)path, flags, mode);
}

static inline int sys_open(const char *path, int flags, int mode) {
    return sys_openat(AT_FDCWD, path, flags, mode);
}

static inline int sys_close(int fd) {
    return (int)raw_syscall1(__NR_close, fd);
}

static inline long sys_read(int fd, void *buf, size_t count) {
    return raw_syscall3(__NR_read, fd, (long)buf, count);
}

static inline long sys_write(int fd, const void *buf, size_t count) {
    return raw_syscall3(__NR_write, fd, (long)buf, count);
}

static inline int sys_getpid(void) {
    return (int)raw_syscall0(__NR_getpid);
}

static inline int sys_getuid(void) {
    return (int)raw_syscall0(__NR_getuid);
}

static inline int sys_geteuid(void) {
    return (int)raw_syscall0(__NR_geteuid);
}

static inline int sys_kill(int pid, int sig) {
    return (int)raw_syscall2(__NR_kill, pid, sig);
}

static inline long sys_ptrace(long request, long pid, void *addr, void *data) {
    return raw_syscall4(__NR_ptrace, request, pid, (long)addr, (long)data);
}

static inline void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, long offset) {
    return (void*)raw_syscall6(__NR_mmap, (long)addr, length, prot, flags, fd, offset);
}

static inline int sys_munmap(void *addr, size_t length) {
    return (int)raw_syscall2(__NR_munmap, (long)addr, length);
}

static inline int sys_mprotect(void *addr, size_t length, int prot) {
    return (int)raw_syscall3(__NR_mprotect, (long)addr, length, prot);
}

static inline int sys_execve(const char *pathname, char *const argv[], char *const envp[]) {
    return (int)raw_syscall3(__NR_execve, (long)pathname, (long)argv, (long)envp);
}

static inline int sys_getcwd(char *buf, size_t size) {
    return (int)raw_syscall2(__NR_getcwd, (long)buf, size);
}

static inline int sys_chdir(const char *path) {
    return (int)raw_syscall1(__NR_chdir, (long)path);
}

static inline int sys_getdents64(int fd, void *dirp, unsigned int count) {
    return (int)raw_syscall3(__NR_getdents64, fd, (long)dirp, count);
}

static inline int sys_dup3(int oldfd, int newfd, int flags) {
    return (int)raw_syscall3(__NR_dup3, oldfd, newfd, flags);
}

// dup2 emulated via dup3(oldfd, newfd, 0)
static inline int sys_dup2(int oldfd, int newfd) {
    return sys_dup3(oldfd, newfd, 0);
}

static inline int sys_pipe2(int pipefd[2], int flags) {
    return (int)raw_syscall2(__NR_pipe2, (long)pipefd, flags);
}

static inline int sys_ioctl(int fd, unsigned long request, unsigned long arg) {
    return (int)raw_syscall3(__NR_ioctl, fd, request, arg);
}

static inline int sys_fcntl(int fd, int cmd, long arg) {
    return (int)raw_syscall3(__NR_fcntl, fd, cmd, arg);
}

static inline long sys_getrandom(void *buf, size_t buflen, unsigned int flags) {
    return raw_syscall3(__NR_getrandom, (long)buf, buflen, flags);
}

static inline int sys_memfd_create(const char *name, unsigned int flags) {
    return (int)raw_syscall2(__NR_memfd_create, (long)name, flags);
}

static inline int sys_prctl(int option, unsigned long a2, unsigned long a3, unsigned long a4, unsigned long a5) {
    return (int)raw_syscall5(__NR_prctl, option, a2, a3, a4, a5);
}

static inline long sys_bpf(int cmd, void *attr, unsigned int size) {
    return raw_syscall3(__NR_bpf, cmd, (long)attr, size);
}

static inline void sys_exit_group(int status) {
    raw_syscall1(__NR_exit_group, status);
}

// ── Stat (via fstatat since ARM64 has no stat syscall) ──

struct linux_stat {
    unsigned long st_dev;
    unsigned long st_ino;
    unsigned int  st_mode;
    unsigned int  st_nlink;
    unsigned int  st_uid;
    unsigned int  st_gid;
    unsigned long st_rdev;
    unsigned long __pad1;
    long          st_size;
    int           st_blksize;
    int           __pad2;
    long          st_blocks;
    unsigned long st_atime_sec;
    unsigned long st_atime_nsec;
    unsigned long st_mtime_sec;
    unsigned long st_mtime_nsec;
    unsigned long st_ctime_sec;
    unsigned long st_ctime_nsec;
    unsigned int  __unused4;
    unsigned int  __unused5;
};

static inline int sys_fstatat(int dirfd, const char *path, struct linux_stat *buf, int flags) {
    return (int)raw_syscall4(__NR_fstatat, dirfd, (long)path, (long)buf, flags);
}

static inline int sys_stat(const char *path, struct linux_stat *buf) {
    return sys_fstatat(AT_FDCWD, path, buf, 0);
}

static inline int sys_fstat(int fd, struct linux_stat *buf) {
    return (int)raw_syscall2(__NR_fstat, fd, (long)buf);
}

// ── Filesystem (via *at syscalls since ARM64 has no legacy versions) ──

static inline int sys_mkdir(const char *path, int mode) {
    return (int)raw_syscall3(__NR_mkdirat, AT_FDCWD, (long)path, mode);
}

static inline int sys_unlink(const char *path) {
    return (int)raw_syscall3(__NR_unlinkat, AT_FDCWD, (long)path, 0);
}

#define AT_REMOVEDIR 0x200

static inline int sys_rmdir(const char *path) {
    return (int)raw_syscall3(__NR_unlinkat, AT_FDCWD, (long)path, AT_REMOVEDIR);
}

static inline int sys_rename(const char *oldpath, const char *newpath) {
    return (int)raw_syscall4(__NR_renameat, AT_FDCWD, (long)oldpath, AT_FDCWD, (long)newpath);
}

// ── Fork (via clone on ARM64) ──

#define SIGCHLD 17

static inline int sys_fork(void) {
    return (int)raw_syscall5(__NR_clone, SIGCHLD, 0, 0, 0, 0);
}

// ── Network syscalls ──

static inline int sys_socket(int domain, int type, int protocol) {
    return (int)raw_syscall3(__NR_socket, domain, type, protocol);
}

static inline int sys_connect(int fd, const void *addr, unsigned int addrlen) {
    return (int)raw_syscall3(__NR_connect, fd, (long)addr, addrlen);
}

static inline int sys_bind(int fd, const void *addr, unsigned int addrlen) {
    return (int)raw_syscall3(__NR_bind, fd, (long)addr, addrlen);
}

static inline int sys_listen(int fd, int backlog) {
    return (int)raw_syscall2(__NR_listen, fd, backlog);
}

static inline int sys_accept(int fd, void *addr, unsigned int *addrlen) {
    return (int)raw_syscall3(__NR_accept, fd, (long)addr, (long)addrlen);
}

static inline int sys_setsockopt(int fd, int level, int optname, const void *optval, unsigned int optlen) {
    return (int)raw_syscall5(__NR_setsockopt, fd, level, optname, (long)optval, optlen);
}

static inline int sys_getsockopt(int fd, int level, int optname, void *optval, unsigned int *optlen) {
    return (int)raw_syscall5(__NR_getsockopt, fd, level, optname, (long)optval, (long)optlen);
}

// ── Process wait ──

static inline int sys_wait4(int pid, int *wstatus, int options, void *rusage) {
    return (int)raw_syscall4(__NR_wait4, pid, (long)wstatus, options, (long)rusage);
}

// ── Sleep (nanosleep) ──

struct linux_timespec {
    long tv_sec;
    long tv_nsec;
};

static inline int sys_nanosleep(const struct linux_timespec *req, struct linux_timespec *rem) {
    return (int)raw_syscall2(__NR_nanosleep, (long)req, (long)rem);
}

static inline void sys_sleep(unsigned int seconds) {
    struct linux_timespec ts = { .tv_sec = seconds, .tv_nsec = 0 };
    sys_nanosleep(&ts, (struct linux_timespec*)0);
}

static inline void sys_usleep(unsigned int usec) {
    struct linux_timespec ts = { .tv_sec = usec / 1000000, .tv_nsec = (usec % 1000000) * 1000L };
    sys_nanosleep(&ts, (struct linux_timespec*)0);
}

// ── Timestamp modification ──

static inline int sys_utimensat(int dirfd, const char *path, const struct linux_timespec times[2], int flags) {
    return (int)raw_syscall4(__NR_utimensat, dirfd, (long)path, (long)times, flags);
}

// ── Process session/group ──

static inline int sys_setsid(void) {
    return (int)raw_syscall0(__NR_setsid);
}

static inline int sys_setpgid(int pid, int pgid) {
    return (int)raw_syscall2(__NR_setpgid, pid, pgid);
}

// ── Select (pselect6) ──

static inline int sys_pselect6(int nfds, void *readfds, void *writefds,
                                void *exceptfds, const struct linux_timespec *timeout,
                                const void *sigmask) {
    return (int)raw_syscall6(__NR_pselect6, nfds, (long)readfds, (long)writefds,
                              (long)exceptfds, (long)timeout, (long)sigmask);
}

// ── Threading via clone() ──

#define CLONE_VM       0x00000100
#define CLONE_FS       0x00000200
#define CLONE_FILES    0x00000400
#define CLONE_SIGHAND  0x00000800
#define CLONE_THREAD   0x00010000
#define CLONE_SYSVSEM  0x00040000
#define CLONE_SETTLS   0x00080000
#define CLONE_PARENT_SETTID  0x00100000
#define CLONE_CHILD_CLEARTID 0x00200000

#define THREAD_CLONE_FLAGS (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | \
                            CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS | \
                            CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID)

#endif // ARCH_AARCH64
#endif // SYSCALLS_AARCH64_H
