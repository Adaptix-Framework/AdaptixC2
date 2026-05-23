#include "jobs.h"
#include "crt.h"
#include "crypt.h"
#include "msgpack.h"
#include "dyld_resolve.h"

#ifdef DEBUG_TRACE
#include "syscalls_arm64.h"
static void _jobs_dbg(const char* msg) {
    size_t len = 0;
    const char* p = msg;
    while (*p++) len++;
    sys_write(2, msg, len);
    sys_write(2, "\n", 1);
}
static void _jobs_dbg_int(const char* prefix, int64_t val) {
    size_t plen = 0;
    const char* p = prefix;
    while (*p++) plen++;
    sys_write(2, prefix, plen);
    char nbuf[24];
    int ni = 0;
    uint64_t uv = val < 0 ? (uint64_t)(-val) : (uint64_t)val;
    if (val < 0) sys_write(2, "-", 1);
    do { nbuf[ni++] = '0' + (uv % 10); uv /= 10; } while (uv > 0);
    while (ni > 0) { char c = nbuf[--ni]; sys_write(2, &c, 1); }
    sys_write(2, "\n", 1);
}
static void _jobs_dbg_hex(const char* prefix, const uint8_t* data, size_t len) {
    size_t plen = 0;
    const char* p = prefix;
    while (*p++) plen++;
    sys_write(2, prefix, plen);
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
#else
#define _jobs_dbg(msg)               ((void)0)
#define _jobs_dbg_int(prefix, val)   ((void)0)
#define _jobs_dbg_hex(prefix, d, l)  ((void)0)
#endif

/// Global job context
job_context_t g_job_ctx;

void jobs_init(job_context_t* ctx) {
    ax_memset(ctx, 0, sizeof(job_context_t));
    R_pthread_mutex_init(&ctx->jobs_mutex, (void*)0);
    R_pthread_mutex_init(&ctx->tunnels_mutex, (void*)0);
    R_pthread_mutex_init(&ctx->terminals_mutex, (void*)0);
}

void jobs_update_connection(job_context_t* ctx, const char* address,
                           int banner_size, const uint8_t* enc_key,
                           uint32_t profile_type) {
    ax_strncpy(ctx->address, address, sizeof(ctx->address) - 1);
    ctx->banner_size = banner_size;
    ax_memcpy(ctx->enc_key, enc_key, 16);
    ctx->profile_type = profile_type;
}

int jobs_open_connection(job_context_t* ctx, connector_t* conn) {
    if (conn_open(conn, ctx->address) != 0)
        return -1;

    // Discard banner
    if (ctx->banner_size > 0) {
        if (conn_discard(conn, (size_t)ctx->banner_size) != 0) {
            conn_close(conn);
            return -1;
        }
    }
    return 0;
}

int jobs_alloc(job_context_t* ctx) {
    R_pthread_mutex_lock(&ctx->jobs_mutex);
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!ctx->jobs[i].active) {
            ax_memset(&ctx->jobs[i], 0, sizeof(job_entry_t));
            ctx->jobs[i].conn.fd = -1;
            R_pthread_mutex_unlock(&ctx->jobs_mutex);
            return i;
        }
    }
    R_pthread_mutex_unlock(&ctx->jobs_mutex);
    return -1;
}

int jobs_find(job_context_t* ctx, const char* job_id) {
    R_pthread_mutex_lock(&ctx->jobs_mutex);
    for (int i = 0; i < MAX_JOBS; i++) {
        if (ctx->jobs[i].active && ax_strcmp(ctx->jobs[i].job_id, job_id) == 0) {
            R_pthread_mutex_unlock(&ctx->jobs_mutex);
            return i;
        }
    }
    R_pthread_mutex_unlock(&ctx->jobs_mutex);
    return -1;
}

void jobs_remove(job_context_t* ctx, int idx) {
    R_pthread_mutex_lock(&ctx->jobs_mutex);
    if (idx >= 0 && idx < MAX_JOBS) {
        ctx->jobs[idx].active = 0;
        ctx->jobs[idx].job_id[0] = '\0';
    }
    R_pthread_mutex_unlock(&ctx->jobs_mutex);
}

int tunnels_find(job_context_t* ctx, int channel_id) {
    R_pthread_mutex_lock(&ctx->tunnels_mutex);
    for (int i = 0; i < MAX_TUNNELS; i++) {
        if (ctx->tunnels[i].active && ctx->tunnels[i].channel_id == channel_id) {
            R_pthread_mutex_unlock(&ctx->tunnels_mutex);
            return i;
        }
    }
    R_pthread_mutex_unlock(&ctx->tunnels_mutex);
    return -1;
}

int terminals_find(job_context_t* ctx, int term_id) {
    R_pthread_mutex_lock(&ctx->terminals_mutex);
    for (int i = 0; i < MAX_TERMINALS; i++) {
        if (ctx->terminals[i].active && ctx->terminals[i].term_id == term_id) {
            R_pthread_mutex_unlock(&ctx->terminals_mutex);
            return i;
        }
    }
    R_pthread_mutex_unlock(&ctx->terminals_mutex);
    return -1;
}

int jobs_send_init(job_context_t* ctx, connector_t* conn,
                   int pack_type, const uint8_t* pack_data, uint32_t pack_len) {
    _jobs_dbg_int("[JOBS] send_init pack_type=", pack_type);
    _jobs_dbg_int("[JOBS] send_init pack_len=", pack_len);

    // Outer: StartMsg{id: pack_type, data: pack_data}
    mp_writer_t outer;
    mp_writer_init(&outer, 256);
    mp_write_map(&outer, 2);
    mp_write_kv_int(&outer, "id", pack_type);
    mp_write_kv_bin(&outer, "data", pack_data, pack_len);

    _jobs_dbg_int("[JOBS] StartMsg msgpack size=", (int64_t)outer.buf.len);
    _jobs_dbg_hex("[JOBS] StartMsg first 32=", outer.buf.data, outer.buf.len < 32 ? outer.buf.len : 32);

    // Encrypt with profile enc_key
    size_t enc_len;
    uint8_t* encrypted = aes128_gcm_encrypt(outer.buf.data, outer.buf.len,
                                             ctx->enc_key, &enc_len);
    mp_writer_free(&outer);
    if (!encrypted) {
        _jobs_dbg("[JOBS] GCM encrypt failed!");
        return -1;
    }

    _jobs_dbg_int("[JOBS] GCM encrypted size=", (int64_t)enc_len);
    _jobs_dbg_hex("[JOBS] GCM encrypted first 32=", encrypted, enc_len < 32 ? enc_len : 32);

    int ret = conn_send_msg(conn, encrypted, enc_len);
    _jobs_dbg_int("[JOBS] conn_send_msg ret=", ret);
    ax_free(encrypted, enc_len);
    return ret;
}

int jobs_send_message(job_context_t* ctx, connector_t* conn,
                      uint32_t command_id, const char* job_id,
                      const uint8_t* data, uint32_t data_len) {
    // Build Job struct: {command_id, job_id, data}
    mp_writer_t job_w;
    mp_writer_init(&job_w, 256 + data_len);
    mp_write_map(&job_w, 3);
    mp_write_kv_uint(&job_w, "command_id", command_id);
    mp_write_kv_str(&job_w, "job_id", job_id);
    mp_write_kv_bin(&job_w, "data", data, data_len);

    // Build Message{type: 2, object: [packed_job]}
    mp_writer_t msg_w;
    mp_writer_init(&msg_w, 256 + job_w.buf.len);
    mp_write_map(&msg_w, 2);
    mp_write_kv_int(&msg_w, "type", 2);
    mp_write_str(&msg_w, "object", 6);
    mp_write_array(&msg_w, 1);
    mp_write_bin(&msg_w, job_w.buf.data, (uint32_t)job_w.buf.len);
    mp_writer_free(&job_w);

    // Encrypt with session key
    size_t enc_len;
    uint8_t* encrypted = aes128_gcm_encrypt(msg_w.buf.data, msg_w.buf.len,
                                             ctx->session_key, &enc_len);
    mp_writer_free(&msg_w);
    if (!encrypted) return -1;

    int ret = conn_send_msg(conn, encrypted, enc_len);
    ax_free(encrypted, enc_len);
    return ret;
}
