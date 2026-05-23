/// ld_preload_check.c — BOF: Check for LD_PRELOAD hooks, /etc/ld.so.preload, rogue shared libs
/// Compile: gcc -c -o ld_preload_check.o ld_preload_check.c -include bof_api.h -Os -fPIC
/// Usage: execute bof ld_preload_check.o

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
    while (*s >= '0' && *s <= '9') { val = val * 10 + (*s - '0'); s++; }
    return val;
}

static void check_ld_preload_env(void) {
    BeaconPrintf(CALLBACK_OUTPUT, "\n=== LD_PRELOAD environment variable ===\n");

    // Check our own process
    char val[1024];
    if (AxGetEnv("LD_PRELOAD", val, sizeof(val)) > 0) {
        BeaconPrintf(CALLBACK_OUTPUT, "  [!!] Current process: LD_PRELOAD=%s\n", val);
    } else {
        BeaconPrintf(CALLBACK_OUTPUT, "  [-] Not set in current process\n");
    }

    // Scan all processes for LD_PRELOAD
    BeaconPrintf(CALLBACK_OUTPUT, "\n=== Scanning all processes for LD_PRELOAD ===\n");
    int found = 0;

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
                    AxSnprintf(path, sizeof(path), "/proc/%d/environ", pid);

                    char *env = (char *)0;
                    int env_len = AxReadFileToBuffer(path, &env, 65536);
                    if (env_len > 0 && env) {
                        // Search for LD_PRELOAD= in null-separated environ
                        int epos = 0;
                        while (epos < env_len) {
                            char *entry_str = env + epos;
                            int elen = 0;
                            while (epos + elen < env_len && entry_str[elen] != '\0') elen++;

                            if (elen > 11 && AxStrncmp(entry_str, "LD_PRELOAD=", 11) == 0) {
                                // Get process name
                                char pname[128];
                                pname[0] = '\0';
                                char spath[128];
                                AxSnprintf(spath, sizeof(spath), "/proc/%d/comm", pid);
                                char *comm = (char *)0;
                                int clen = AxReadFileToBuffer(spath, &comm, 128);
                                if (clen > 0 && comm) {
                                    if (comm[clen - 1] == '\n') comm[clen - 1] = '\0';
                                    AxStrcpy(pname, comm);
                                    AxFree(comm);
                                }

                                char preload_val[512];
                                int vlen = elen - 11;
                                if (vlen > 511) vlen = 511;
                                AxMemcpy(preload_val, entry_str + 11, vlen);
                                preload_val[vlen] = '\0';

                                BeaconPrintf(CALLBACK_OUTPUT,
                                             "  [!!] PID %-6d (%s): LD_PRELOAD=%s\n",
                                             pid, pname, preload_val);
                                found++;
                                break;
                            }
                            epos += elen + 1;
                        }
                        AxFree(env);
                    }
                }
            }
            pos += entry->d_reclen;
        }
    }
    AxCloseFile(dirfd);

    if (found == 0) {
        BeaconPrintf(CALLBACK_OUTPUT, "  [-] No processes with LD_PRELOAD\n");
    }
}

static void check_ld_so_preload(void) {
    BeaconPrintf(CALLBACK_OUTPUT, "\n=== /etc/ld.so.preload ===\n");

    char *data = (char *)0;
    int len = AxReadFileToBuffer("/etc/ld.so.preload", &data, 8192);
    if (len > 0 && data) {
        BeaconPrintf(CALLBACK_OUTPUT, "  [!!] /etc/ld.so.preload EXISTS — global preload active!\n");

        char *line = data;
        while (line < data + len) {
            char *eol = line;
            while (eol < data + len && *eol != '\n') eol++;
            int llen = (int)(eol - line);

            if (llen > 0 && line[0] != '#') {
                char lbuf[512];
                if (llen > 511) llen = 511;
                AxMemcpy(lbuf, line, llen);
                lbuf[llen] = '\0';

                // Check if library exists
                unsigned int mode = 0;
                long fsize = 0;
                int exists = (AxFileStat(lbuf, &mode, &fsize, (unsigned int *)0, (unsigned int *)0) == 0);

                BeaconPrintf(CALLBACK_OUTPUT, "  [PRELOAD] %s (%s, %ld bytes)\n",
                             lbuf, exists ? "exists" : "MISSING", fsize);
            }
            line = eol + 1;
        }
        AxFree(data);
    } else {
        BeaconPrintf(CALLBACK_OUTPUT, "  [-] /etc/ld.so.preload does not exist (normal)\n");
    }
}

static void check_ld_library_path(void) {
    BeaconPrintf(CALLBACK_OUTPUT, "\n=== LD_LIBRARY_PATH ===\n");

    char val[2048];
    if (AxGetEnv("LD_LIBRARY_PATH", val, sizeof(val)) > 0) {
        BeaconPrintf(CALLBACK_OUTPUT, "  [!] LD_LIBRARY_PATH=%s\n", val);
        BeaconPrintf(CALLBACK_OUTPUT, "      (non-standard library search paths — potential hijack vector)\n");
    } else {
        BeaconPrintf(CALLBACK_OUTPUT, "  [-] Not set\n");
    }
}

static void check_ld_so_conf(void) {
    BeaconPrintf(CALLBACK_OUTPUT, "\n=== /etc/ld.so.conf + /etc/ld.so.conf.d/ ===\n");

    char *conf = (char *)0;
    int conf_len = AxReadFileToBuffer("/etc/ld.so.conf", &conf, 8192);
    if (conf_len > 0 && conf) {
        BeaconPrintf(CALLBACK_OUTPUT, "  /etc/ld.so.conf:\n");
        char *line = conf;
        while (line < conf + conf_len) {
            char *eol = line;
            while (eol < conf + conf_len && *eol != '\n') eol++;
            int llen = (int)(eol - line);
            if (llen > 0 && line[0] != '#') {
                char lbuf[256];
                if (llen > 255) llen = 255;
                AxMemcpy(lbuf, line, llen);
                lbuf[llen] = '\0';
                BeaconPrintf(CALLBACK_OUTPUT, "    %s\n", lbuf);
            }
            line = eol + 1;
        }
        AxFree(conf);
    }

    // Scan /etc/ld.so.conf.d/
    int dirfd = AxOpenDir("/etc/ld.so.conf.d");
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
                    AxSnprintf(fpath, sizeof(fpath), "/etc/ld.so.conf.d/%s", entry->d_name);
                    char *fdata = (char *)0;
                    int flen = AxReadFileToBuffer(fpath, &fdata, 4096);
                    if (flen > 0 && fdata) {
                        BeaconPrintf(CALLBACK_OUTPUT, "  %s:\n", fpath);
                        char *fl = fdata;
                        while (fl < fdata + flen) {
                            char *feol = fl;
                            while (feol < fdata + flen && *feol != '\n') feol++;
                            int fllen = (int)(feol - fl);
                            if (fllen > 0 && fl[0] != '#') {
                                char flbuf[256];
                                if (fllen > 255) fllen = 255;
                                AxMemcpy(flbuf, fl, fllen);
                                flbuf[fllen] = '\0';
                                BeaconPrintf(CALLBACK_OUTPUT, "    %s\n", flbuf);
                            }
                            fl = feol + 1;
                        }
                        AxFree(fdata);
                    }
                }
                pos += entry->d_reclen;
            }
        }
        AxCloseFile(dirfd);
    }
}

static void check_rpath_abuse(void) {
    BeaconPrintf(CALLBACK_OUTPUT, "\n=== Writable library directories ===\n");

    // Check if any standard lib dirs are writable
    const char *lib_dirs[] = {
        "/lib", "/lib64", "/usr/lib", "/usr/lib64",
        "/usr/local/lib", "/usr/local/lib64",
        (const char *)0
    };

    int writable = 0;
    for (int i = 0; lib_dirs[i]; i++) {
        unsigned int mode = 0;
        if (AxFileStat(lib_dirs[i], &mode, (long *)0, (unsigned int *)0, (unsigned int *)0) == 0) {
            // Check world-writable (or our uid writable)
            if (mode & 002) {  // world-writable
                BeaconPrintf(CALLBACK_OUTPUT, "  [!!] %s is world-writable!\n", lib_dirs[i]);
                writable++;
            }
        }
    }

    if (writable == 0) {
        BeaconPrintf(CALLBACK_OUTPUT, "  [-] Standard library directories are properly protected\n");
    }
}

void go(char *args, int args_len) {
    BeaconPrintf(CALLBACK_OUTPUT, "[*] LD_PRELOAD / shared library hook analysis\n");

    check_ld_preload_env();
    check_ld_so_preload();
    check_ld_library_path();
    check_ld_so_conf();
    check_rpath_abuse();

    BeaconPrintf(CALLBACK_OUTPUT, "\n[*] LD analysis complete\n");
}
