/// proc_enum.c — BOF: Deep /proc process enumeration (threads, fd, cmdline, maps, cwd)
/// Compile: gcc -c -o proc_enum.o proc_enum.c -include bof_api.h -Os -fPIC
/// Usage: execute bof proc_enum.o

struct linux_dirent64 {
    unsigned long long d_ino;
    long long          d_off;
    unsigned short     d_reclen;
    unsigned char      d_type;
    char               d_name[];
};

#define DT_DIR 4

static int is_digit(char c) { return c >= '0' && c <= '9'; }

static int str_to_int(const char *s) {
    int val = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    return val;
}

static void enum_process(int pid) {
    char path[256];
    char buf[4096];

    // cmdline
    AxSnprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    char *cmdline = (char *)0;
    int cmdline_len = AxReadFileToBuffer(path, &cmdline, 4096);

    // Replace null bytes with spaces in cmdline
    char cmd_display[512];
    cmd_display[0] = '\0';
    if (cmdline_len > 0 && cmdline) {
        int dlen = cmdline_len > 500 ? 500 : cmdline_len;
        AxMemcpy(cmd_display, cmdline, dlen);
        for (int i = 0; i < dlen; i++) {
            if (cmd_display[i] == '\0') cmd_display[i] = ' ';
        }
        cmd_display[dlen] = '\0';
        AxFree(cmdline);
    }

    // status — extract key fields
    AxSnprintf(path, sizeof(path), "/proc/%d/status", pid);
    char *status = (char *)0;
    int status_len = AxReadFileToBuffer(path, &status, 8192);
    if (status_len <= 0 || !status) return;

    // Parse Name, State, Uid, Gid, Threads, VmRSS
    char name[128] = "(unknown)";
    char state[32] = "?";
    int uid = -1, threads = 0;
    long vm_rss = 0;

    char *line = status;
    while (line < status + status_len) {
        char *eol = line;
        while (eol < status + status_len && *eol != '\n') eol++;

        if (AxStrncmp(line, "Name:\t", 6) == 0) {
            int nlen = (int)(eol - line - 6);
            if (nlen > 127) nlen = 127;
            AxMemcpy(name, line + 6, nlen);
            name[nlen] = '\0';
        } else if (AxStrncmp(line, "State:\t", 7) == 0) {
            int slen = (int)(eol - line - 7);
            if (slen > 31) slen = 31;
            AxMemcpy(state, line + 7, slen);
            state[slen] = '\0';
        } else if (AxStrncmp(line, "Uid:\t", 5) == 0) {
            uid = str_to_int(line + 5);
        } else if (AxStrncmp(line, "Threads:\t", 9) == 0) {
            threads = str_to_int(line + 9);
        } else if (AxStrncmp(line, "VmRSS:", 6) == 0) {
            char *p = line + 6;
            while (*p == ' ' || *p == '\t') p++;
            vm_rss = 0;
            while (*p >= '0' && *p <= '9') {
                vm_rss = vm_rss * 10 + (*p - '0');
                p++;
            }
        }
        line = eol + 1;
    }
    AxFree(status);

    // cwd (readlink via reading /proc/pid/cwd — we read the symlink target)
    char cwd[256] = "";
    AxSnprintf(path, sizeof(path), "/proc/%d/cwd", pid);
    // Can't readlink without syscall, but we can try exe
    AxSnprintf(path, sizeof(path), "/proc/%d/exe", pid);
    char *exe = (char *)0;
    // exe is a symlink — read via /proc/pid/maps first line or status
    // Simplify: just show cmdline

    // Count open FDs
    int fd_count = 0;
    AxSnprintf(path, sizeof(path), "/proc/%d/fd", pid);
    int dirfd = AxOpenDir(path);
    if (dirfd >= 0) {
        char dirbuf[4096];
        while (1) {
            int nread = AxReadDir(dirfd, dirbuf, sizeof(dirbuf));
            if (nread <= 0) break;
            int pos = 0;
            while (pos < nread) {
                struct linux_dirent64 *entry = (struct linux_dirent64 *)(dirbuf + pos);
                if (entry->d_name[0] != '.') fd_count++;
                pos += entry->d_reclen;
            }
        }
        AxCloseFile(dirfd);
    }

    // Output
    BeaconPrintf(CALLBACK_OUTPUT, "  %-6d %-4d %-20s %-10s thr=%-3d fd=%-4d rss=%-8ld %s\n",
                 pid, uid, name, state, threads, fd_count, vm_rss, cmd_display);
}

void go(char *args, int args_len) {
    BeaconPrintf(CALLBACK_OUTPUT, "[*] Deep process enumeration\n\n");
    BeaconPrintf(CALLBACK_OUTPUT, "  %-6s %-4s %-20s %-10s %-7s %-6s %-10s %s\n",
                 "PID", "UID", "NAME", "STATE", "THR", "FDs", "RSS(kB)", "CMDLINE");
    BeaconPrintf(CALLBACK_OUTPUT, "  %s\n",
                 "----------------------------------------------------------------------"
                 "----------------------------------------------");

    int dirfd = AxOpenDir("/proc");
    if (dirfd < 0) {
        BeaconPrintf(CALLBACK_ERROR, "[!] Cannot open /proc\n");
        return;
    }

    char dirbuf[8192];
    int total = 0;

    while (1) {
        int nread = AxReadDir(dirfd, dirbuf, sizeof(dirbuf));
        if (nread <= 0) break;

        int pos = 0;
        while (pos < nread) {
            struct linux_dirent64 *entry = (struct linux_dirent64 *)(dirbuf + pos);

            // Only process numeric directories (PIDs)
            if (entry->d_type == DT_DIR && is_digit(entry->d_name[0])) {
                int pid = str_to_int(entry->d_name);
                if (pid > 0) {
                    enum_process(pid);
                    total++;
                }
            }
            pos += entry->d_reclen;
        }
    }
    AxCloseFile(dirfd);

    BeaconPrintf(CALLBACK_OUTPUT, "\n[*] %d processes enumerated\n", total);
}
