#ifndef SYSCALLS_X64_H
#define SYSCALLS_X64_H

#ifdef ARCH_X86_64

#include <stdint.h>
#include <stddef.h>

/// Linux x86_64 syscall numbers
#define __NR_read           0
#define __NR_write          1
#define __NR_open           2
#define __NR_close          3
#define __NR_stat           4
#define __NR_fstat          5
#define __NR_mmap           9
#define __NR_mprotect       10
#define __NR_munmap         11
#define __NR_ioctl          16
#define __NR_pipe           22
#define __NR_dup2           33
#define __NR_socket         41
#define __NR_connect        42
#define __NR_accept         43
#define __NR_bind           49
#define __NR_listen         50
#define __NR_setsockopt     54
#define __NR_getsockopt     55
#define __NR_clone          56
#define __NR_fork           57
#define __NR_execve         59
#define __NR_exit           60
#define __NR_kill           62
#define __NR_fcntl          72
#define __NR_getcwd         79
#define __NR_chdir          80
#define __NR_rename         82
#define __NR_mkdir          83
#define __NR_rmdir          84
#define __NR_unlink         87
#define __NR_getuid         102
#define __NR_ptrace         101
#define __NR_geteuid        107
#define __NR_getpid         39
#define __NR_getdents64     217
#define __NR_exit_group     231
#define __NR_waitid         247
#define __NR_openat         257
#define __NR_pipe2          293
#define __NR_nanosleep      35
#define __NR_wait4          61
#define __NR_select         23
#define __NR_setsid         112
#define __NR_setpgid        109
#define __NR_pselect6       270
#define __NR_prctl          157
#define __NR_utimensat      280
#define __NR_bpf            321
#define __NR_getrandom      318
#define __NR_memfd_create   319

/// Raw syscall wrappers — inline assembly
/// Convention: rax=nr, rdi=a0, rsi=a1, rdx=a2, r10=a3, r8=a4, r9=a5
/// Clobbered: rcx, r11

static inline long raw_syscall0(long number) {
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(number)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long raw_syscall1(long number, long a0) {
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(number), "D"(a0)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long raw_syscall2(long number, long a0, long a1) {
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(number), "D"(a0), "S"(a1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long raw_syscall3(long number, long a0, long a1, long a2) {
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(number), "D"(a0), "S"(a1), "d"(a2)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long raw_syscall4(long number, long a0, long a1, long a2, long a3) {
    long ret;
    register long r10 __asm__("r10") = a3;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(number), "D"(a0), "S"(a1), "d"(a2), "r"(r10)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long raw_syscall5(long number, long a0, long a1, long a2, long a3, long a4) {
    long ret;
    register long r10 __asm__("r10") = a3;
    register long r8  __asm__("r8")  = a4;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(number), "D"(a0), "S"(a1), "d"(a2), "r"(r10), "r"(r8)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline long raw_syscall6(long number, long a0, long a1, long a2, long a3, long a4, long a5) {
    long ret;
    register long r10 __asm__("r10") = a3;
    register long r8  __asm__("r8")  = a4;
    register long r9  __asm__("r9")  = a5;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(number), "D"(a0), "S"(a1), "d"(a2), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/// Convenience wrappers

static inline int sys_open(const char *path, int flags, int mode) {
    return (int)raw_syscall3(__NR_open, (long)path, flags, mode);
}

static inline int sys_openat(int dirfd, const char *path, int flags, int mode) {
    return (int)raw_syscall4(__NR_openat, dirfd, (long)path, flags, mode);
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

static inline int sys_fork(void) {
    return (int)raw_syscall0(__NR_fork);
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

static inline int sys_mkdir(const char *path, int mode) {
    return (int)raw_syscall2(__NR_mkdir, (long)path, mode);
}

static inline int sys_unlink(const char *path) {
    return (int)raw_syscall1(__NR_unlink, (long)path);
}

static inline int sys_rename(const char *oldpath, const char *newpath) {
    return (int)raw_syscall2(__NR_rename, (long)oldpath, (long)newpath);
}

static inline int sys_rmdir(const char *path) {
    return (int)raw_syscall1(__NR_rmdir, (long)path);
}

static inline int sys_getdents64(int fd, void *dirp, unsigned int count) {
    return (int)raw_syscall3(__NR_getdents64, fd, (long)dirp, count);
}

static inline int sys_dup2(int oldfd, int newfd) {
    return (int)raw_syscall2(__NR_dup2, oldfd, newfd);
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

// ── Stat ──

struct linux_stat {
    unsigned long st_dev;
    unsigned long st_ino;
    unsigned long st_nlink;
    unsigned int  st_mode;
    unsigned int  st_uid;
    unsigned int  st_gid;
    unsigned int  __pad0;
    unsigned long st_rdev;
    long          st_size;
    long          st_blksize;
    long          st_blocks;
    unsigned long st_atime_sec;
    unsigned long st_atime_nsec;
    unsigned long st_mtime_sec;
    unsigned long st_mtime_nsec;
    unsigned long st_ctime_sec;
    unsigned long st_ctime_nsec;
    long          __unused[3];
};

static inline int sys_stat(const char *path, struct linux_stat *buf) {
    return (int)raw_syscall2(__NR_stat, (long)path, (long)buf);
}

static inline int sys_fstat(int fd, struct linux_stat *buf) {
    return (int)raw_syscall2(__NR_fstat, fd, (long)buf);
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
// Minimal pthread replacement for -nostdlib static builds
// Stack: mmap(STACK_SIZE), child runs fn(arg)

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

#endif // ARCH_X86_64
#endif // SYSCALLS_X64_H
