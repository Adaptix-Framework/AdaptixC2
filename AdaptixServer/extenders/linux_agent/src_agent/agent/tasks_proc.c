/// tasks_proc.c -- Process commands for Linux agent
/// All ops via direct syscalls — zero libc dependency.

#include "tasks_proc.h"
#include "crt.h"
#include "types.h"

#ifdef ARCH_X86_64
#include "syscalls_x64.h"
#endif
#ifdef ARCH_AARCH64
#include "syscalls_aarch64.h"
#endif

// ── Linux constants ──

#define O_RDONLY     0
#define SIGKILL      9

// ── Helpers ──

static void write_error(mp_writer_t *w, const char *msg) {
    mp_write_map(w, 1);
    mp_write_kv_str(w, "error", msg);
}

/// Parse /proc/<pid>/stat → extract pid, comm, ppid, tty_nr
/// Format: "pid (comm) state ppid pgrp session tty_nr ..."
static int parse_proc_stat(const char *buf, int *pid, char *comm, size_t comm_size,
                           int *ppid, int *tty_nr) {
    // Parse pid
    const char *p = buf;
    *pid = ax_atoi(p);

    // Find '(' for comm start
    while (*p && *p != '(') p++;
    if (!*p) return -1;
    p++; // skip '('

    // Find matching ')' — comm can contain spaces and parens
    const char *comm_start = p;
    const char *comm_end = p;
    while (*p) {
        if (*p == ')') comm_end = p;
        p++;
    }
    // comm_end now points to the LAST ')' in the string
    size_t clen = (size_t)(comm_end - comm_start);
    if (clen >= comm_size) clen = comm_size - 1;
    ax_memcpy(comm, comm_start, clen);
    comm[clen] = '\0';

    // After ') ' comes: state ppid pgrp session tty_nr ...
    p = comm_end + 1;
    while (*p == ' ') p++;
    // state (single char)
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
    // ppid
    *ppid = ax_atoi(p);
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
    // pgrp — skip
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
    // session — skip
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
    // tty_nr
    *tty_nr = ax_atoi(p);

    return 0;
}

/// Read /proc/<pid>/status and extract Uid: field (first value = real UID)
static int get_proc_uid(int pid, int *uid) {
    char path[64];
    ax_strcpy(path, "/proc/");
    char pidbuf[16];
    ax_itoa(pid, pidbuf, 10);
    ax_strcat(path, pidbuf);
    ax_strcat(path, "/status");

    char buf[4096];
    int fd = sys_open(path, O_RDONLY, 0);
    if (fd < 0) return -1;
    long n = sys_read(fd, buf, sizeof(buf) - 1);
    sys_close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';

    // Find "Uid:\t<real_uid>"
    char *p = ax_strstr(buf, "Uid:");
    if (!p) return -1;
    p += 4; // skip "Uid:"
    while (*p == '\t' || *p == ' ') p++;
    *uid = ax_atoi(p);
    return 0;
}

/// Convert UID to username by parsing /etc/passwd
static void uid_to_name(int uid, char *buf, size_t buf_size) {
    char passwd[8192];
    int fd = sys_open("/etc/passwd", O_RDONLY, 0);
    if (fd < 0) goto fallback;

    long n = sys_read(fd, passwd, sizeof(passwd) - 1);
    sys_close(fd);
    if (n <= 0) goto fallback;
    passwd[n] = '\0';

    char uid_str[16];
    ax_itoa(uid, uid_str, 10);
    size_t uid_len = ax_strlen(uid_str);

    {
        char *line = passwd;
        while (*line) {
            char *eol = ax_strchr(line, '\n');
            if (eol) *eol = '\0';

            // Format: name:x:uid:gid:...
            char *p1 = ax_strchr(line, ':');
            if (!p1) goto next;
            char *p2 = ax_strchr(p1 + 1, ':');
            if (!p2) goto next;
            char *uid_start = p2 + 1;
            char *p3 = ax_strchr(uid_start, ':');
            if (!p3) goto next;

            size_t field_len = (size_t)(p3 - uid_start);
            if (field_len == uid_len && ax_memcmp(uid_start, uid_str, uid_len) == 0) {
                size_t name_len = (size_t)(p1 - line);
                if (name_len >= buf_size) name_len = buf_size - 1;
                ax_memcpy(buf, line, name_len);
                buf[name_len] = '\0';
                return;
            }

        next:
            if (eol) line = eol + 1;
            else break;
        }
    }

fallback:
    ax_itoa(uid, buf, 10);
}

/// Convert tty_nr to tty name string
static void tty_to_name(int tty_nr, char *buf, size_t buf_size) {
    if (tty_nr == 0) {
        ax_strncpy(buf, "?", buf_size - 1);
        buf[buf_size - 1] = '\0';
        return;
    }
    int major = (tty_nr >> 8) & 0xff;
    int minor = tty_nr & 0xff;

    if (major == 136) {
        // pts/<minor>
        ax_strcpy(buf, "pts/");
        char num[16];
        ax_itoa(minor, num, 10);
        ax_strcat(buf, num);
    } else if (major == 4 && minor < 64) {
        // tty<minor>
        ax_strcpy(buf, "tty");
        char num[16];
        ax_itoa(minor, num, 10);
        ax_strcat(buf, num);
    } else {
        ax_strcpy(buf, "tty");
        char num[16];
        ax_itoa(major, num, 10);
        ax_strcat(buf, num);
        ax_strcat(buf, "/");
        ax_itoa(minor, num, 10);
        ax_strcat(buf, num);
    }
}

// ── getdents64 for /proc scanning ──

struct linux_dirent64 {
    uint64_t d_ino;
    int64_t  d_off;
    uint16_t d_reclen;
    uint8_t  d_type;
    char     d_name[];
};

#define DT_DIR 4

// ──── Command handlers ────

int task_ps(mp_writer_t *w)
{
    // Scan /proc for numeric directories → each is a PID
    int dirfd = sys_open("/proc", O_RDONLY, 0);
    if (dirfd < 0) {
        mp_write_map(w, 3);
        mp_write_kv_bool(w, "result", 0);
        mp_write_kv_str(w, "status", "cannot open /proc");
        mp_write_kv_bin(w, "processes", (const uint8_t *)"", 0);
        return 0;
    }

    // First pass: count PIDs
    char dirbuf[4096];
    uint32_t count = 0;
    for (;;) {
        int nread = sys_getdents64(dirfd, dirbuf, sizeof(dirbuf));
        if (nread <= 0) break;
        int pos = 0;
        while (pos < nread) {
            struct linux_dirent64 *d = (struct linux_dirent64 *)(dirbuf + pos);
            if (d->d_type == DT_DIR && d->d_name[0] >= '0' && d->d_name[0] <= '9') {
                count++;
            }
            pos += d->d_reclen;
        }
    }
    sys_close(dirfd);

    // Second pass: collect process info
    dirfd = sys_open("/proc", O_RDONLY, 0);
    if (dirfd < 0) {
        mp_write_map(w, 3);
        mp_write_kv_bool(w, "result", 0);
        mp_write_kv_str(w, "status", "cannot reopen /proc");
        mp_write_kv_bin(w, "processes", (const uint8_t *)"", 0);
        return 0;
    }

    mp_writer_t procs;
    mp_writer_init(&procs, 4096);
    mp_write_array(&procs, count);

    uint32_t written = 0;
    for (;;) {
        int nread = sys_getdents64(dirfd, dirbuf, sizeof(dirbuf));
        if (nread <= 0) break;
        int pos = 0;
        while (pos < nread) {
            struct linux_dirent64 *d = (struct linux_dirent64 *)(dirbuf + pos);
            if (d->d_type == DT_DIR && d->d_name[0] >= '0' && d->d_name[0] <= '9' && written < count) {
                // Read /proc/<pid>/stat
                char stat_path[64];
                ax_strcpy(stat_path, "/proc/");
                ax_strcat(stat_path, d->d_name);
                ax_strcat(stat_path, "/stat");

                char stat_buf[1024];
                int sfd = sys_open(stat_path, O_RDONLY, 0);
                if (sfd >= 0) {
                    long n = sys_read(sfd, stat_buf, sizeof(stat_buf) - 1);
                    sys_close(sfd);
                    if (n > 0) {
                        stat_buf[n] = '\0';

                        int pid = 0, ppid = 0, tty_nr = 0;
                        char comm[256] = {0};

                        if (parse_proc_stat(stat_buf, &pid, comm, sizeof(comm), &ppid, &tty_nr) == 0) {
                            // Get UID
                            int proc_uid = 0;
                            get_proc_uid(pid, &proc_uid);

                            char user[64];
                            uid_to_name(proc_uid, user, sizeof(user));

                            char tty[32];
                            tty_to_name(tty_nr, tty, sizeof(tty));

                            // Write PsInfo map: {pid, ppid, tty, context, process}
                            mp_write_map(&procs, 5);
                            mp_write_kv_int(&procs, "pid", pid);
                            mp_write_kv_int(&procs, "ppid", ppid);
                            mp_write_kv_str(&procs, "tty", tty);
                            mp_write_kv_str(&procs, "context", user);
                            mp_write_kv_str(&procs, "process", comm);

                            written++;
                        }
                    }
                }
            }
            pos += d->d_reclen;
        }
    }
    sys_close(dirfd);

    // If we wrote fewer than count (processes died between passes), pad with empty maps
    while (written < count) {
        mp_write_map(&procs, 5);
        mp_write_kv_int(&procs, "pid", 0);
        mp_write_kv_int(&procs, "ppid", 0);
        mp_write_kv_str(&procs, "tty", "?");
        mp_write_kv_str(&procs, "context", "");
        mp_write_kv_str(&procs, "process", "[dead]");
        written++;
    }

    // AnsPs: {result: bool, status: string, processes: []byte}
    mp_write_map(w, 3);
    mp_write_kv_bool(w, "result", 1);
    mp_write_kv_str(w, "status", "");
    mp_write_kv_bin(w, "processes", procs.buf.data, (uint32_t)procs.buf.len);

    mp_writer_free(&procs);
    return 0;
}

int task_kill(const uint8_t *data, uint32_t data_len, mp_writer_t *w)
{
    // Parse {pid: int}
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t map_count;
    if (mp_read_map(&r, &map_count) != 0) {
        write_error(w, "invalid params");
        return 0;
    }

    int pid = 0;
    for (uint32_t i = 0; i < map_count; i++) {
        const char *key; uint32_t klen;
        if (mp_read_str(&r, &key, &klen) != 0) break;
        if (klen == 3 && ax_memcmp(key, "pid", 3) == 0) {
            uint64_t v; mp_read_uint(&r, &v); pid = (int)v;
        } else {
            mp_skip(&r);
        }
    }

    if (pid <= 0) {
        write_error(w, "invalid pid");
        return 0;
    }

    if (sys_kill(pid, SIGKILL) != 0) {
        write_error(w, "kill failed");
        return 0;
    }

    mp_write_nil(w);
    return 0;
}

int task_shell(const uint8_t *data, uint32_t data_len, mp_writer_t *w)
{
    // Go sends: {program: "/bin/sh", args: ["-c", "whoami"]}
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t map_count;
    if (mp_read_map(&r, &map_count) != 0) {
        write_error(w, "invalid params");
        return 0;
    }

    char program_buf[512] = {0};
    // argv slots: [program, arg0, arg1, ..., NULL] — max 32 args
    #define SHELL_MAX_ARGS 32
    char *argv_ptrs[SHELL_MAX_ARGS + 2]; // +1 for program, +1 for NULL
    int argc = 0;
    char args_storage[4096] = {0};
    size_t args_off = 0;

    for (uint32_t i = 0; i < map_count; i++) {
        const char *key; uint32_t klen;
        if (mp_read_str(&r, &key, &klen) != 0) break;

        if (klen == 7 && ax_memcmp(key, "program", 7) == 0) {
            const char *val; uint32_t vlen;
            if (mp_read_str(&r, &val, &vlen) != 0) break;
            if (vlen >= sizeof(program_buf)) vlen = sizeof(program_buf) - 1;
            ax_memcpy(program_buf, val, vlen);
            program_buf[vlen] = '\0';
        } else if (klen == 4 && ax_memcmp(key, "args", 4) == 0) {
            // args is an array of strings: ["-c", "whoami"]
            uint32_t arr_count;
            if (mp_read_array(&r, &arr_count) != 0) {
                mp_skip(&r);
                continue;
            }
            for (uint32_t j = 0; j < arr_count && argc < SHELL_MAX_ARGS; j++) {
                const char *val; uint32_t vlen;
                if (mp_read_str(&r, &val, &vlen) != 0) break;
                if (args_off + vlen + 1 > sizeof(args_storage)) break;
                ax_memcpy(args_storage + args_off, val, vlen);
                args_storage[args_off + vlen] = '\0';
                argv_ptrs[1 + argc] = args_storage + args_off;
                argc++;
                args_off += vlen + 1;
            }
        } else {
            mp_skip(&r);
        }
    }

    if (program_buf[0] == '\0') {
        ax_strcpy(program_buf, "/bin/sh");
    }

    // Build final argv: [program, args..., NULL]
    argv_ptrs[0] = program_buf;
    argv_ptrs[1 + argc] = (char *)0;

    // Set up pipe for stdout+stderr capture
    int pipefd[2];
    if (sys_pipe2(pipefd, 0) != 0) {
        write_error(w, "pipe2 failed");
        return 0;
    }

    int pid = sys_fork();
    if (pid < 0) {
        sys_close(pipefd[0]);
        sys_close(pipefd[1]);
        write_error(w, "fork failed");
        return 0;
    }

    if (pid == 0) {
        // Child process
        sys_close(pipefd[0]);
        sys_dup2(pipefd[1], 1);
        sys_dup2(pipefd[1], 2);
        sys_close(pipefd[1]);

        char *envp[] = { (char *)0 };
        sys_execve(program_buf, argv_ptrs, envp);
        sys_exit_group(127);
    }

    // Parent
    sys_close(pipefd[1]); // close write end

    // Read output from child
    size_t out_cap = 8192;
    size_t out_len = 0;
    uint8_t *output = (uint8_t *)ax_malloc(out_cap);

    for (;;) {
        if (out_len + 4096 > out_cap) {
            out_cap *= 2;
            output = (uint8_t *)ax_realloc(output, out_cap);
        }
        long n = sys_read(pipefd[0], output + out_len, 4096);
        if (n <= 0) break;
        out_len += (size_t)n;
    }
    sys_close(pipefd[0]);

    // Wait for child
    int status = 0;
    sys_wait4(pid, &status, 0, (void *)0);

    // Null-terminate for string output
    if (out_len + 1 > out_cap) {
        output = (uint8_t *)ax_realloc(output, out_len + 1);
    }
    output[out_len] = '\0';

    // AnsShell: {output: string}
    mp_write_map(w, 1);
    mp_write_kv_str(w, "output", (const char *)output);

    ax_free(output);
    return 0;
}

int task_getuid(mp_writer_t *w)
{
    int uid  = sys_getuid();
    int euid = sys_geteuid();

    char uid_name[64], euid_name[64];
    uid_to_name(uid, uid_name, sizeof(uid_name));
    uid_to_name(euid, euid_name, sizeof(euid_name));

    // Build "uid=<N>(<name>) euid=<N>(<name>)" string
    char result[256];
    ax_strcpy(result, "uid=");
    char num[16];
    ax_itoa(uid, num, 10);
    ax_strcat(result, num);
    ax_strcat(result, "(");
    ax_strcat(result, uid_name);
    ax_strcat(result, ") euid=");
    ax_itoa(euid, num, 10);
    ax_strcat(result, num);
    ax_strcat(result, "(");
    ax_strcat(result, euid_name);
    ax_strcat(result, ")");

    // AnsShell: {output: string}
    mp_write_map(w, 1);
    mp_write_kv_str(w, "output", result);
    return 0;
}
