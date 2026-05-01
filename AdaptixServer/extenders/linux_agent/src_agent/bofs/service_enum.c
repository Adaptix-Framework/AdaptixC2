/// service_enum.c — BOF: Enumerate running services, systemd units, init scripts
/// Compile: gcc -c -o service_enum.o service_enum.c -include bof_api.h -Os -fPIC
/// Usage: execute bof service_enum.o

struct linux_dirent64 {
    unsigned long long d_ino;
    long long          d_off;
    unsigned short     d_reclen;
    unsigned char      d_type;
    char               d_name[];
};

#define DT_REG  8
#define DT_LNK  10
#define DT_DIR  4

static int is_digit(char c) { return c >= '0' && c <= '9'; }
static int str_to_int(const char *s) {
    int val = 0;
    while (*s >= '0' && *s <= '9') { val = val * 10 + (*s - '0'); s++; }
    return val;
}

static void enum_listening_services(void) {
    BeaconPrintf(CALLBACK_OUTPUT, "\n=== Listening services (from /proc/net/tcp) ===\n");

    char *tcp = (char *)0;
    int tcp_len = AxReadFileToBuffer("/proc/net/tcp", &tcp, 524288);
    if (tcp_len <= 0 || !tcp) return;

    BeaconPrintf(CALLBACK_OUTPUT, "  %-8s %-22s\n", "PROTO", "LISTEN ADDRESS");

    char *line = tcp;
    int line_num = 0;
    while (line < tcp + tcp_len) {
        char *eol = line;
        while (eol < tcp + tcp_len && *eol != '\n') eol++;

        line_num++;
        if (line_num == 1) { line = eol + 1; continue; }

        // Find state field (0A = LISTEN)
        // Quick scan for "0A" as state
        char *p = line;
        // Skip to after remote_addr:port (3 colon-delimited fields)
        int colons = 0;
        while (p < eol && colons < 3) {
            if (*p == ':') colons++;
            p++;
        }
        // Skip spaces
        while (p < eol && *p == ' ') p++;
        // Now at state field — skip port hex
        // Actually simpler: look for " 0A " pattern in line
        char *state = AxStrstr(line, " 0A ");
        if (state) {
            // This is a LISTEN socket — extract local address
            char *colon1 = AxStrchr(line, ':');
            if (colon1) {
                char *local = colon1 + 2;
                char *lcolon = AxStrchr(local, ':');
                if (lcolon && lcolon < state) {
                    // Parse IP and port
                    char ip_hex[16];
                    int ip_len = (int)(lcolon - local);
                    if (ip_len > 15) ip_len = 15;
                    AxMemcpy(ip_hex, local, ip_len);
                    ip_hex[ip_len] = '\0';

                    // Parse hex port
                    int port = 0;
                    for (int i = 1; i <= 4 && lcolon[i]; i++) {
                        int nibble = 0;
                        char c = lcolon[i];
                        if (c >= '0' && c <= '9') nibble = c - '0';
                        else if (c >= 'A' && c <= 'F') nibble = c - 'A' + 10;
                        else if (c >= 'a' && c <= 'f') nibble = c - 'a' + 10;
                        port = (port << 4) | nibble;
                    }

                    // Parse hex IP (little-endian)
                    unsigned int ip = 0;
                    for (int i = 0; i < 8 && ip_hex[i]; i++) {
                        int nibble = 0;
                        char c = ip_hex[i];
                        if (c >= '0' && c <= '9') nibble = c - '0';
                        else if (c >= 'A' && c <= 'F') nibble = c - 'A' + 10;
                        else if (c >= 'a' && c <= 'f') nibble = c - 'a' + 10;
                        ip = (ip << 4) | nibble;
                    }

                    BeaconPrintf(CALLBACK_OUTPUT, "  %-8s %d.%d.%d.%d:%d\n",
                                 "tcp",
                                 ip & 0xFF, (ip >> 8) & 0xFF,
                                 (ip >> 16) & 0xFF, (ip >> 24) & 0xFF,
                                 port);
                }
            }
        }
        line = eol + 1;
    }
    AxFree(tcp);

    // UDP listeners (all UDP sockets are effectively "listening")
    char *udp = (char *)0;
    int udp_len = AxReadFileToBuffer("/proc/net/udp", &udp, 524288);
    if (udp_len > 0 && udp) {
        char *line2 = udp;
        int ln = 0;
        while (line2 < udp + udp_len) {
            char *eol2 = line2;
            while (eol2 < udp + udp_len && *eol2 != '\n') eol2++;
            ln++;
            if (ln > 1) {
                char *colon1 = AxStrchr(line2, ':');
                if (colon1) {
                    char *local = colon1 + 2;
                    char *lcolon = AxStrchr(local, ':');
                    if (lcolon) {
                        int port = 0;
                        for (int i = 1; i <= 4 && lcolon[i]; i++) {
                            int nibble = 0;
                            char c = lcolon[i];
                            if (c >= '0' && c <= '9') nibble = c - '0';
                            else if (c >= 'A' && c <= 'F') nibble = c - 'A' + 10;
                            else if (c >= 'a' && c <= 'f') nibble = c - 'a' + 10;
                            port = (port << 4) | nibble;
                        }
                        if (port > 0) {
                            unsigned int ip = 0;
                            char ip_hex[16];
                            int ip_len = (int)(lcolon - local);
                            if (ip_len > 15) ip_len = 15;
                            AxMemcpy(ip_hex, local, ip_len);
                            ip_hex[ip_len] = '\0';
                            for (int i = 0; i < 8 && ip_hex[i]; i++) {
                                int nibble = 0;
                                char c = ip_hex[i];
                                if (c >= '0' && c <= '9') nibble = c - '0';
                                else if (c >= 'A' && c <= 'F') nibble = c - 'A' + 10;
                                else if (c >= 'a' && c <= 'f') nibble = c - 'a' + 10;
                                ip = (ip << 4) | nibble;
                            }
                            BeaconPrintf(CALLBACK_OUTPUT, "  %-8s %d.%d.%d.%d:%d\n",
                                         "udp",
                                         ip & 0xFF, (ip >> 8) & 0xFF,
                                         (ip >> 16) & 0xFF, (ip >> 24) & 0xFF,
                                         port);
                        }
                    }
                }
            }
            line2 = eol2 + 1;
        }
        AxFree(udp);
    }
}

static void enum_systemd_units(void) {
    BeaconPrintf(CALLBACK_OUTPUT, "\n=== Systemd service units ===\n");

    const char *unit_dirs[] = {
        "/etc/systemd/system",
        "/usr/lib/systemd/system",
        "/lib/systemd/system",
        (const char *)0
    };

    for (int d = 0; unit_dirs[d]; d++) {
        int dirfd = AxOpenDir(unit_dirs[d]);
        if (dirfd < 0) continue;

        BeaconPrintf(CALLBACK_OUTPUT, "  [%s]\n", unit_dirs[d]);

        char dirbuf[8192];
        int count = 0;
        while (1) {
            int nread = AxReadDir(dirfd, dirbuf, sizeof(dirbuf));
            if (nread <= 0) break;

            int pos = 0;
            while (pos < nread) {
                struct linux_dirent64 *entry = (struct linux_dirent64 *)(dirbuf + pos);
                char *name = entry->d_name;

                // Only show .service files
                int nlen = AxStrlen(name);
                if (nlen > 8 && AxStrcmp(name + nlen - 8, ".service") == 0) {
                    BeaconPrintf(CALLBACK_OUTPUT, "    %s\n", name);
                    count++;
                }
                pos += entry->d_reclen;
            }
        }
        AxCloseFile(dirfd);

        if (count == 0) {
            BeaconPrintf(CALLBACK_OUTPUT, "    (no .service files)\n");
        }
    }
}

static void enum_init_scripts(void) {
    BeaconPrintf(CALLBACK_OUTPUT, "\n=== Init scripts ===\n");

    int dirfd = AxOpenDir("/etc/init.d");
    if (dirfd < 0) {
        BeaconPrintf(CALLBACK_OUTPUT, "  /etc/init.d not found\n");
        return;
    }

    char dirbuf[4096];
    while (1) {
        int nread = AxReadDir(dirfd, dirbuf, sizeof(dirbuf));
        if (nread <= 0) break;

        int pos = 0;
        while (pos < nread) {
            struct linux_dirent64 *entry = (struct linux_dirent64 *)(dirbuf + pos);
            if (entry->d_name[0] != '.') {
                BeaconPrintf(CALLBACK_OUTPUT, "    %s\n", entry->d_name);
            }
            pos += entry->d_reclen;
        }
    }
    AxCloseFile(dirfd);
}

static void enum_running_daemons(void) {
    BeaconPrintf(CALLBACK_OUTPUT, "\n=== Running daemons (root processes) ===\n");

    int dirfd = AxOpenDir("/proc");
    if (dirfd < 0) return;

    char dirbuf[8192];
    while (1) {
        int nread = AxReadDir(dirfd, dirbuf, sizeof(dirbuf));
        if (nread <= 0) break;

        int pos = 0;
        while (pos < nread) {
            struct linux_dirent64 *entry = (struct linux_dirent64 *)(dirbuf + pos);

            if (entry->d_type == DT_DIR && is_digit(entry->d_name[0])) {
                int pid = str_to_int(entry->d_name);
                if (pid > 0) {
                    char path[128];
                    AxSnprintf(path, sizeof(path), "/proc/%d/status", pid);
                    char *status = (char *)0;
                    int slen = AxReadFileToBuffer(path, &status, 4096);
                    if (slen > 0 && status) {
                        // Check if UID is 0
                        char *uid_line = AxStrstr(status, "Uid:\t");
                        if (uid_line) {
                            int uid = str_to_int(uid_line + 5);
                            if (uid == 0) {
                                // Get name
                                char *name_line = AxStrstr(status, "Name:\t");
                                if (name_line) {
                                    name_line += 6;
                                    char *eol = AxStrchr(name_line, '\n');
                                    if (eol) {
                                        char pname[128];
                                        int plen = (int)(eol - name_line);
                                        if (plen > 127) plen = 127;
                                        AxMemcpy(pname, name_line, plen);
                                        pname[plen] = '\0';

                                        // Get PPID
                                        char *ppid_line = AxStrstr(status, "PPid:\t");
                                        int ppid = 0;
                                        if (ppid_line) ppid = str_to_int(ppid_line + 6);

                                        // Only show if PPID is 1 (daemon) or 0 (kernel)
                                        if (ppid <= 1) {
                                            BeaconPrintf(CALLBACK_OUTPUT, "  PID %-6d  %s\n", pid, pname);
                                        }
                                    }
                                }
                            }
                        }
                        AxFree(status);
                    }
                }
            }
            pos += entry->d_reclen;
        }
    }
    AxCloseFile(dirfd);
}

void go(char *args, int args_len) {
    BeaconPrintf(CALLBACK_OUTPUT, "[*] Service enumeration\n");

    enum_listening_services();
    enum_running_daemons();
    enum_systemd_units();
    enum_init_scripts();

    BeaconPrintf(CALLBACK_OUTPUT, "\n[*] Service enumeration complete\n");
}
