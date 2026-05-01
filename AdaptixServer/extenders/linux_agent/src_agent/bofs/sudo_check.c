/// sudo_check.c — BOF: Parse sudoers configuration + check current user privileges
/// Compile: gcc -c -o sudo_check.o sudo_check.c -include bof_api.h -Os -fPIC
/// Usage: execute bof sudo_check.o

// getdents64 record structure
struct linux_dirent64 {
    unsigned long long d_ino;
    long long          d_off;
    unsigned short     d_reclen;
    unsigned char      d_type;
    char               d_name[];
};

#define DT_REG 8

static void print_interesting_lines(const char *data, int len, const char *source) {
    // Parse line by line, show non-comment, non-empty lines
    const char *line = data;
    int interesting = 0;

    while (line < data + len) {
        const char *eol = line;
        while (eol < data + len && *eol != '\n')
            eol++;

        int line_len = (int)(eol - line);

        // Skip empty lines and comments
        if (line_len > 0) {
            // Skip leading whitespace
            const char *start = line;
            while (start < eol && (*start == ' ' || *start == '\t'))
                start++;

            int content_len = (int)(eol - start);
            if (content_len > 0 && *start != '#') {
                // Check for high-value patterns
                int is_nopasswd = 0;
                int is_all = 0;

                // Manual search for NOPASSWD
                for (const char *p = start; p + 8 <= eol; p++) {
                    if (AxStrncmp(p, "NOPASSWD", 8) == 0) {
                        is_nopasswd = 1;
                        break;
                    }
                }
                // Search for ALL
                for (const char *p = start; p + 3 <= eol; p++) {
                    if (p[0] == 'A' && p[1] == 'L' && p[2] == 'L') {
                        is_all = 1;
                        break;
                    }
                }

                char prefix[16];
                prefix[0] = ' '; prefix[1] = ' ';
                int plen = 2;
                if (is_nopasswd && is_all) {
                    // Critical: NOPASSWD + ALL
                    prefix[0] = '!'; prefix[1] = '!';
                } else if (is_nopasswd || is_all) {
                    prefix[0] = ' '; prefix[1] = '*';
                }
                prefix[plen] = '\0';

                // Print the line with truncation guard
                char linebuf[1024];
                if (content_len > 1020) content_len = 1020;
                AxMemcpy(linebuf, start, content_len);
                linebuf[content_len] = '\0';
                BeaconPrintf(CALLBACK_OUTPUT, "%s %s\n", prefix, linebuf);
                interesting++;
            }
        }
        line = eol + 1;
    }

    if (interesting == 0) {
        BeaconPrintf(CALLBACK_OUTPUT, "  (no active rules in %s)\n", source);
    }
}

void go(char *args, int args_len) {
    BeaconPrintf(CALLBACK_OUTPUT, "[*] Sudoers configuration audit\n");
    BeaconPrintf(CALLBACK_OUTPUT, "    Legend: !! = critical (NOPASSWD+ALL), * = notable\n\n");

    // 1. Read /etc/sudoers
    char *sudoers = (char *)0;
    int sudoers_len = AxReadFileToBuffer("/etc/sudoers", &sudoers, 524288);
    if (sudoers_len > 0 && sudoers) {
        BeaconPrintf(CALLBACK_OUTPUT, "=== /etc/sudoers ===\n");
        print_interesting_lines(sudoers, sudoers_len, "/etc/sudoers");
        AxFree(sudoers);
    } else {
        BeaconPrintf(CALLBACK_OUTPUT, "[!] Cannot read /etc/sudoers (need root or sudo group)\n");
    }

    // 2. Scan /etc/sudoers.d/
    int dirfd = AxOpenDir("/etc/sudoers.d");
    if (dirfd >= 0) {
        char dirbuf[4096];
        while (1) {
            int nread = AxReadDir(dirfd, dirbuf, sizeof(dirbuf));
            if (nread <= 0) break;

            int pos = 0;
            while (pos < nread) {
                struct linux_dirent64 *entry = (struct linux_dirent64 *)(dirbuf + pos);
                char *name = entry->d_name;

                // Skip . and .. and hidden files
                if (name[0] != '.') {
                    char filepath[512];
                    AxSnprintf(filepath, sizeof(filepath), "/etc/sudoers.d/%s", name);

                    char *fdata = (char *)0;
                    int flen = AxReadFileToBuffer(filepath, &fdata, 524288);
                    if (flen > 0 && fdata) {
                        BeaconPrintf(CALLBACK_OUTPUT, "\n=== %s ===\n", filepath);
                        print_interesting_lines(fdata, flen, filepath);
                        AxFree(fdata);
                    }
                }
                pos += entry->d_reclen;
            }
        }
        AxCloseFile(dirfd);
    }

    // 3. Check current user's groups (from /proc/self/status)
    char *status = (char *)0;
    int status_len = AxReadFileToBuffer("/proc/self/status", &status, 65536);
    if (status_len > 0 && status) {
        // Find "Groups:" line
        char *groups_line = AxStrstr(status, "Groups:");
        if (groups_line) {
            char *eol = groups_line;
            while (eol < status + status_len && *eol != '\n')
                eol++;
            int gline_len = (int)(eol - groups_line);
            if (gline_len > 0) {
                char gbuf[512];
                if (gline_len > 511) gline_len = 511;
                AxMemcpy(gbuf, groups_line, gline_len);
                gbuf[gline_len] = '\0';
                BeaconPrintf(CALLBACK_OUTPUT, "\n=== Current process ===\n");
                BeaconPrintf(CALLBACK_OUTPUT, "  PID: %d  |  UID: %d  |  EUID: %d\n",
                             AxGetPid(), AxGetUid(), AxGetEuid());
                BeaconPrintf(CALLBACK_OUTPUT, "  %s\n", gbuf);
            }
        }
        AxFree(status);
    }

    // 4. Check if user is in sudo/wheel group via /etc/group
    char *group_file = (char *)0;
    int group_len = AxReadFileToBuffer("/etc/group", &group_file, 524288);
    if (group_len > 0 && group_file) {
        // Get current username from /proc/self/status (Name: field) or UID
        int my_uid = AxGetUid();

        // Find username from /etc/passwd by UID
        char *passwd = (char *)0;
        int passwd_len = AxReadFileToBuffer("/etc/passwd", &passwd, 524288);
        char my_username[256];
        my_username[0] = '\0';

        if (passwd_len > 0 && passwd) {
            char uid_str[16];
            AxSnprintf(uid_str, sizeof(uid_str), "%d", my_uid);
            int uid_str_len = AxStrlen(uid_str);

            char *line = passwd;
            while (line < passwd + passwd_len) {
                char *eol = line;
                while (eol < passwd + passwd_len && *eol != '\n')
                    eol++;

                // username:x:uid:...
                char *c1 = AxStrchr(line, ':');
                if (c1 && c1 < eol) {
                    char *c2 = AxStrchr(c1 + 1, ':');
                    if (c2 && c2 < eol) {
                        char *uid_start = c2 + 1;
                        char *c3 = AxStrchr(uid_start, ':');
                        if (c3 && c3 < eol) {
                            int u_len = (int)(c3 - uid_start);
                            if (u_len == uid_str_len && AxStrncmp(uid_start, uid_str, u_len) == 0) {
                                int name_len = (int)(c1 - line);
                                if (name_len > 255) name_len = 255;
                                AxMemcpy(my_username, line, name_len);
                                my_username[name_len] = '\0';
                                break;
                            }
                        }
                    }
                }
                line = eol + 1;
            }
            AxFree(passwd);
        }

        if (my_username[0] != '\0') {
            BeaconPrintf(CALLBACK_OUTPUT, "  Username: %s\n", my_username);

            // Check privileged groups
            const char *priv_groups[] = {"sudo", "wheel", "admin", "root", "docker", "lxd", "disk", (const char *)0};
            BeaconPrintf(CALLBACK_OUTPUT, "\n=== Privileged group membership ===\n");

            char *line = group_file;
            while (line < group_file + group_len) {
                char *eol = line;
                while (eol < group_file + group_len && *eol != '\n')
                    eol++;

                // group:x:gid:member1,member2,...
                char *c1 = AxStrchr(line, ':');
                if (c1 && c1 < eol) {
                    int gname_len = (int)(c1 - line);

                    // Check if this is a privileged group
                    for (int i = 0; priv_groups[i]; i++) {
                        int pg_len = AxStrlen(priv_groups[i]);
                        if (gname_len == pg_len && AxStrncmp(line, priv_groups[i], gname_len) == 0) {
                            // Check if our user is a member
                            // Find last colon (members field)
                            char *last_colon = eol - 1;
                            while (last_colon > c1 && *last_colon != ':')
                                last_colon--;
                            if (*last_colon == ':') {
                                char *members = last_colon + 1;
                                int members_len = (int)(eol - members);

                                // Search for username in comma-separated list
                                int uname_len = AxStrlen(my_username);
                                char *search = members;
                                while (search + uname_len <= eol) {
                                    if (AxStrncmp(search, my_username, uname_len) == 0) {
                                        char after = (search + uname_len < eol) ? *(search + uname_len) : '\0';
                                        if (after == ',' || after == '\0' || after == '\n') {
                                            char gname_buf[64];
                                            if (gname_len > 63) gname_len = 63;
                                            AxMemcpy(gname_buf, line, gname_len);
                                            gname_buf[gname_len] = '\0';
                                            BeaconPrintf(CALLBACK_OUTPUT, "  [!] Member of '%s' group\n", gname_buf);
                                            break;
                                        }
                                    }
                                    // Advance to next comma or end
                                    while (search < eol && *search != ',')
                                        search++;
                                    if (search < eol) search++;
                                }
                            }
                        }
                    }
                }
                line = eol + 1;
            }
        }
        AxFree(group_file);
    }
}
