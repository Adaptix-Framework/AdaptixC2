/// host_recon.c — BOF: Host reconnaissance (system info, users, groups, login history, crontabs)
/// Compile: gcc -c -o host_recon.o host_recon.c -include bof_api.h -Os -fPIC
/// Usage: execute bof host_recon.o

struct linux_dirent64 {
    unsigned long long d_ino;
    long long          d_off;
    unsigned short     d_reclen;
    unsigned char      d_type;
    char               d_name[];
};

static void dump_file(const char *title, const char *path, int max_size) {
    char *data = (char *)0;
    int len = AxReadFileToBuffer(path, &data, max_size);
    if (len > 0 && data) {
        BeaconPrintf(CALLBACK_OUTPUT, "\n=== %s ===\n", title);
        BeaconOutput(CALLBACK_OUTPUT, data, len);
        if (data[len - 1] != '\n')
            BeaconOutput(CALLBACK_OUTPUT, "\n", 1);
        AxFree(data);
    }
}

static void system_info(void) {
    BeaconPrintf(CALLBACK_OUTPUT, "=== System Information ===\n");

    // Hostname
    char *hn = (char *)0;
    int hn_len = AxReadFileToBuffer("/proc/sys/kernel/hostname", &hn, 256);
    if (hn_len > 0 && hn) {
        // Trim trailing newline
        if (hn[hn_len - 1] == '\n') hn[hn_len - 1] = '\0';
        BeaconPrintf(CALLBACK_OUTPUT, "  Hostname:   %s\n", hn);
        AxFree(hn);
    }

    // Domain
    char *dm = (char *)0;
    int dm_len = AxReadFileToBuffer("/proc/sys/kernel/domainname", &dm, 256);
    if (dm_len > 0 && dm) {
        if (dm[dm_len - 1] == '\n') dm[dm_len - 1] = '\0';
        BeaconPrintf(CALLBACK_OUTPUT, "  Domain:     %s\n", dm);
        AxFree(dm);
    }

    // Kernel
    char *ver = (char *)0;
    int ver_len = AxReadFileToBuffer("/proc/version", &ver, 512);
    if (ver_len > 0 && ver) {
        if (ver[ver_len - 1] == '\n') ver[ver_len - 1] = '\0';
        BeaconPrintf(CALLBACK_OUTPUT, "  Kernel:     %s\n", ver);
        AxFree(ver);
    }

    // OS release
    char *os = (char *)0;
    int os_len = AxReadFileToBuffer("/etc/os-release", &os, 4096);
    if (os_len > 0 && os) {
        // Extract PRETTY_NAME
        char *pn = AxStrstr(os, "PRETTY_NAME=");
        if (pn) {
            pn += 12;
            if (*pn == '"') pn++;
            char *end = AxStrchr(pn, '"');
            if (!end) end = AxStrchr(pn, '\n');
            if (end) {
                char osname[256];
                int nlen = (int)(end - pn);
                if (nlen > 255) nlen = 255;
                AxMemcpy(osname, pn, nlen);
                osname[nlen] = '\0';
                BeaconPrintf(CALLBACK_OUTPUT, "  OS:         %s\n", osname);
            }
        }
        AxFree(os);
    }

    // Uptime
    char *up = (char *)0;
    int up_len = AxReadFileToBuffer("/proc/uptime", &up, 128);
    if (up_len > 0 && up) {
        // First field is total seconds
        long seconds = 0;
        char *p = up;
        while (*p >= '0' && *p <= '9') {
            seconds = seconds * 10 + (*p - '0');
            p++;
        }
        long days = seconds / 86400;
        long hours = (seconds % 86400) / 3600;
        long mins = (seconds % 3600) / 60;
        BeaconPrintf(CALLBACK_OUTPUT, "  Uptime:     %ldd %ldh %ldm\n", days, hours, mins);
        AxFree(up);
    }

    // CPU info (first processor)
    char *cpu = (char *)0;
    int cpu_len = AxReadFileToBuffer("/proc/cpuinfo", &cpu, 8192);
    if (cpu_len > 0 && cpu) {
        char *model = AxStrstr(cpu, "model name");
        if (model) {
            char *colon = AxStrchr(model, ':');
            if (colon) {
                colon++;
                while (*colon == ' ' || *colon == '\t') colon++;
                char *eol = AxStrchr(colon, '\n');
                if (eol) {
                    char cpuname[256];
                    int clen = (int)(eol - colon);
                    if (clen > 255) clen = 255;
                    AxMemcpy(cpuname, colon, clen);
                    cpuname[clen] = '\0';
                    BeaconPrintf(CALLBACK_OUTPUT, "  CPU:        %s\n", cpuname);
                }
            }
        }
        // Count processors
        int ncpu = 0;
        char *search = cpu;
        while ((search = AxStrstr(search, "processor")) != (char *)0) {
            ncpu++;
            search += 9;
        }
        BeaconPrintf(CALLBACK_OUTPUT, "  CPU cores:  %d\n", ncpu);
        AxFree(cpu);
    }

    // Memory
    char *mem = (char *)0;
    int mem_len = AxReadFileToBuffer("/proc/meminfo", &mem, 4096);
    if (mem_len > 0 && mem) {
        char *mt = AxStrstr(mem, "MemTotal:");
        char *mf = AxStrstr(mem, "MemAvailable:");
        if (mt) {
            mt += 9;
            while (*mt == ' ') mt++;
            long total = 0;
            while (*mt >= '0' && *mt <= '9') { total = total * 10 + (*mt - '0'); mt++; }
            BeaconPrintf(CALLBACK_OUTPUT, "  RAM total:  %ld MB\n", total / 1024);
        }
        if (mf) {
            mf += 13;
            while (*mf == ' ') mf++;
            long avail = 0;
            while (*mf >= '0' && *mf <= '9') { avail = avail * 10 + (*mf - '0'); mf++; }
            BeaconPrintf(CALLBACK_OUTPUT, "  RAM avail:  %ld MB\n", avail / 1024);
        }
        AxFree(mem);
    }

    // Architecture
    BeaconPrintf(CALLBACK_OUTPUT, "  PID:        %d\n", AxGetPid());
    BeaconPrintf(CALLBACK_OUTPUT, "  UID:        %d  EUID: %d\n", AxGetUid(), AxGetEuid());
}

static void enum_users(void) {
    char *passwd = (char *)0;
    int len = AxReadFileToBuffer("/etc/passwd", &passwd, 524288);
    if (len <= 0 || !passwd) return;

    BeaconPrintf(CALLBACK_OUTPUT, "\n=== Users with shell access ===\n");
    BeaconPrintf(CALLBACK_OUTPUT, "  %-20s %-6s %-6s %-30s %s\n",
                 "USERNAME", "UID", "GID", "HOME", "SHELL");

    char *line = passwd;
    while (line < passwd + len) {
        char *eol = line;
        while (eol < passwd + len && *eol != '\n') eol++;

        // Parse: user:x:uid:gid:gecos:home:shell
        char *fields[7];
        int nfields = 0;
        char *fstart = line;
        for (char *p = line; p <= eol && nfields < 7; p++) {
            if (p == eol || *p == ':') {
                fields[nfields++] = fstart;
                if (p < eol) *p = '\0';  // Temporary null termination
                fstart = p + 1;
            }
        }

        if (nfields >= 7) {
            // Skip nologin/false
            int skip = 0;
            int shell_len = AxStrlen(fields[6]);
            if (shell_len >= 7 && AxStrncmp(fields[6] + shell_len - 7, "nologin", 7) == 0) skip = 1;
            if (shell_len >= 5 && AxStrncmp(fields[6] + shell_len - 5, "false", 5) == 0) skip = 1;
            if (AxStrcmp(fields[6], "/bin/sync") == 0) skip = 1;

            if (!skip) {
                BeaconPrintf(CALLBACK_OUTPUT, "  %-20s %-6s %-6s %-30s %s\n",
                             fields[0], fields[2], fields[3], fields[5], fields[6]);
            }
        }

        // Restore colons (not strictly needed since we own the buffer)
        line = eol + 1;
    }
    AxFree(passwd);
}

static void enum_groups(void) {
    char *groups = (char *)0;
    int len = AxReadFileToBuffer("/etc/group", &groups, 524288);
    if (len <= 0 || !groups) return;

    BeaconPrintf(CALLBACK_OUTPUT, "\n=== Groups with members ===\n");

    char *line = groups;
    while (line < groups + len) {
        char *eol = line;
        while (eol < groups + len && *eol != '\n') eol++;

        // group:x:gid:member1,member2
        // Find last colon
        char *last_colon = eol;
        int colons = 0;
        for (char *p = line; p < eol; p++) {
            if (*p == ':') { last_colon = p; colons++; }
        }

        if (colons >= 3 && last_colon + 1 < eol) {
            // Has members — show this group
            char gline[512];
            int glen = (int)(eol - line);
            if (glen > 511) glen = 511;
            AxMemcpy(gline, line, glen);
            gline[glen] = '\0';
            BeaconPrintf(CALLBACK_OUTPUT, "  %s\n", gline);
        }

        line = eol + 1;
    }
    AxFree(groups);
}

static void enum_crontabs(void) {
    BeaconPrintf(CALLBACK_OUTPUT, "\n=== Crontabs ===\n");

    // System crontab
    char *crontab = (char *)0;
    int ct_len = AxReadFileToBuffer("/etc/crontab", &crontab, 65536);
    if (ct_len > 0 && crontab) {
        BeaconPrintf(CALLBACK_OUTPUT, "  [/etc/crontab]\n");
        // Show non-comment lines
        char *line = crontab;
        while (line < crontab + ct_len) {
            char *eol = line;
            while (eol < crontab + ct_len && *eol != '\n') eol++;
            int llen = (int)(eol - line);
            if (llen > 0 && line[0] != '#') {
                char *s = line;
                while (s < eol && (*s == ' ' || *s == '\t')) s++;
                if (s < eol) {
                    char lbuf[512];
                    int ll = (int)(eol - line);
                    if (ll > 511) ll = 511;
                    AxMemcpy(lbuf, line, ll);
                    lbuf[ll] = '\0';
                    BeaconPrintf(CALLBACK_OUTPUT, "    %s\n", lbuf);
                }
            }
            line = eol + 1;
        }
        AxFree(crontab);
    }

    // /etc/cron.d/
    int dirfd = AxOpenDir("/etc/cron.d");
    if (dirfd >= 0) {
        char dirbuf[4096];
        while (1) {
            int nread = AxReadDir(dirfd, dirbuf, sizeof(dirbuf));
            if (nread <= 0) break;
            int pos = 0;
            while (pos < nread) {
                struct linux_dirent64 *entry = (struct linux_dirent64 *)(dirbuf + pos);
                if (entry->d_name[0] != '.') {
                    char fpath[256];
                    AxSnprintf(fpath, sizeof(fpath), "/etc/cron.d/%s", entry->d_name);
                    BeaconPrintf(CALLBACK_OUTPUT, "  [%s]\n", fpath);
                }
                pos += entry->d_reclen;
            }
        }
        AxCloseFile(dirfd);
    }
}

void go(char *args, int args_len) {
    BeaconPrintf(CALLBACK_OUTPUT, "[*] Host reconnaissance\n\n");

    system_info();
    enum_users();
    enum_groups();
    enum_crontabs();

    // Login shells
    dump_file("Available shells", "/etc/shells", 4096);

    // Timezone
    dump_file("Timezone", "/etc/timezone", 128);

    // Machine ID
    dump_file("Machine ID", "/etc/machine-id", 128);

    BeaconPrintf(CALLBACK_OUTPUT, "\n[*] Host recon complete\n");
}
