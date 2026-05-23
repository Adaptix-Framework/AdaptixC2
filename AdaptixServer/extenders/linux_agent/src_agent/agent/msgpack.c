#include "msgpack.h"

/// ---- Writer ----

int mp_writer_init(mp_writer_t* w, size_t cap) {
    buf_init(&w->buf, (int)cap);
    return 0;
}

void mp_writer_free(mp_writer_t* w) {
    buf_free(&w->buf);
}

static int write_byte(mp_writer_t* w, uint8_t b) {
    buf_append(&w->buf, &b, 1);
    return 0;
}

static int write_bytes(mp_writer_t* w, const void* data, size_t len) {
    buf_append(&w->buf, data, (int)len);
    return 0;
}

static int write_u16_be(mp_writer_t* w, uint16_t val) {
    uint8_t b[2] = { (uint8_t)(val >> 8), (uint8_t)val };
    return write_bytes(w, b, 2);
}

static int write_u32_be(mp_writer_t* w, uint32_t val) {
    uint8_t b[4] = {
        (uint8_t)(val >> 24), (uint8_t)(val >> 16),
        (uint8_t)(val >> 8),  (uint8_t)val
    };
    return write_bytes(w, b, 4);
}

int mp_write_map(mp_writer_t* w, uint32_t count) {
    if (count <= 15) {
        return write_byte(w, 0x80 | (uint8_t)count);
    } else if (count <= 0xFFFF) {
        if (write_byte(w, 0xDE)) return -1;
        return write_u16_be(w, (uint16_t)count);
    } else {
        if (write_byte(w, 0xDF)) return -1;
        return write_u32_be(w, count);
    }
}

int mp_write_array(mp_writer_t* w, uint32_t count) {
    if (count <= 15) {
        return write_byte(w, 0x90 | (uint8_t)count);
    } else if (count <= 0xFFFF) {
        if (write_byte(w, 0xDC)) return -1;
        return write_u16_be(w, (uint16_t)count);
    } else {
        if (write_byte(w, 0xDD)) return -1;
        return write_u32_be(w, count);
    }
}

int mp_write_nil(mp_writer_t* w) {
    return write_byte(w, 0xC0);
}

int mp_write_bool(mp_writer_t* w, bool val) {
    return write_byte(w, val ? 0xC3 : 0xC2);
}

int mp_write_uint(mp_writer_t* w, uint64_t val) {
    if (val <= 0x7F) {
        return write_byte(w, (uint8_t)val);
    } else if (val <= 0xFF) {
        if (write_byte(w, 0xCC)) return -1;
        return write_byte(w, (uint8_t)val);
    } else if (val <= 0xFFFF) {
        if (write_byte(w, 0xCD)) return -1;
        return write_u16_be(w, (uint16_t)val);
    } else if (val <= 0xFFFFFFFF) {
        if (write_byte(w, 0xCE)) return -1;
        return write_u32_be(w, (uint32_t)val);
    } else {
        if (write_byte(w, 0xCF)) return -1;
        uint8_t b[8] = {
            (uint8_t)(val >> 56), (uint8_t)(val >> 48),
            (uint8_t)(val >> 40), (uint8_t)(val >> 32),
            (uint8_t)(val >> 24), (uint8_t)(val >> 16),
            (uint8_t)(val >> 8),  (uint8_t)val
        };
        return write_bytes(w, b, 8);
    }
}

int mp_write_int(mp_writer_t* w, int64_t val) {
    if (val >= 0) {
        return mp_write_uint(w, (uint64_t)val);
    }
    if (val >= -32) {
        return write_byte(w, (uint8_t)(val & 0xFF));
    } else if (val >= -128) {
        if (write_byte(w, 0xD0)) return -1;
        return write_byte(w, (uint8_t)(val & 0xFF));
    } else if (val >= -32768) {
        if (write_byte(w, 0xD1)) return -1;
        return write_u16_be(w, (uint16_t)(val & 0xFFFF));
    } else if (val >= -2147483648LL) {
        if (write_byte(w, 0xD2)) return -1;
        return write_u32_be(w, (uint32_t)(val & 0xFFFFFFFF));
    } else {
        if (write_byte(w, 0xD3)) return -1;
        uint64_t uval = (uint64_t)val;
        uint8_t b[8] = {
            (uint8_t)(uval >> 56), (uint8_t)(uval >> 48),
            (uint8_t)(uval >> 40), (uint8_t)(uval >> 32),
            (uint8_t)(uval >> 24), (uint8_t)(uval >> 16),
            (uint8_t)(uval >> 8),  (uint8_t)uval
        };
        return write_bytes(w, b, 8);
    }
}

int mp_write_str(mp_writer_t* w, const char* str, uint32_t len) {
    if (len <= 31) {
        if (write_byte(w, 0xA0 | (uint8_t)len)) return -1;
    } else if (len <= 0xFF) {
        if (write_byte(w, 0xD9)) return -1;
        if (write_byte(w, (uint8_t)len)) return -1;
    } else if (len <= 0xFFFF) {
        if (write_byte(w, 0xDA)) return -1;
        if (write_u16_be(w, (uint16_t)len)) return -1;
    } else {
        if (write_byte(w, 0xDB)) return -1;
        if (write_u32_be(w, len)) return -1;
    }
    if (len > 0) {
        return write_bytes(w, str, len);
    }
    return 0;
}

int mp_write_bin(mp_writer_t* w, const uint8_t* data, uint32_t len) {
    if (len <= 0xFF) {
        if (write_byte(w, 0xC4)) return -1;
        if (write_byte(w, (uint8_t)len)) return -1;
    } else if (len <= 0xFFFF) {
        if (write_byte(w, 0xC5)) return -1;
        if (write_u16_be(w, (uint16_t)len)) return -1;
    } else {
        if (write_byte(w, 0xC6)) return -1;
        if (write_u32_be(w, len)) return -1;
    }
    if (len > 0) {
        return write_bytes(w, data, len);
    }
    return 0;
}

int mp_write_kv_str(mp_writer_t* w, const char* key, const char* val) {
    uint32_t klen = (uint32_t)ax_strlen(key);
    uint32_t vlen = val ? (uint32_t)ax_strlen(val) : 0;
    if (mp_write_str(w, key, klen)) return -1;
    return mp_write_str(w, val ? val : "", vlen);
}

int mp_write_kv_bin(mp_writer_t* w, const char* key, const uint8_t* data, uint32_t len) {
    uint32_t klen = (uint32_t)ax_strlen(key);
    if (mp_write_str(w, key, klen)) return -1;
    return mp_write_bin(w, data, len);
}

int mp_write_kv_uint(mp_writer_t* w, const char* key, uint64_t val) {
    uint32_t klen = (uint32_t)ax_strlen(key);
    if (mp_write_str(w, key, klen)) return -1;
    return mp_write_uint(w, val);
}

int mp_write_kv_int(mp_writer_t* w, const char* key, int64_t val) {
    uint32_t klen = (uint32_t)ax_strlen(key);
    if (mp_write_str(w, key, klen)) return -1;
    return mp_write_int(w, val);
}

int mp_write_kv_bool(mp_writer_t* w, const char* key, bool val) {
    uint32_t klen = (uint32_t)ax_strlen(key);
    if (mp_write_str(w, key, klen)) return -1;
    return mp_write_bool(w, val);
}

/// ---- Reader ----

void mp_reader_init(mp_reader_t* r, const uint8_t* data, size_t len) {
    r->data = data;
    r->len = len;
    r->pos = 0;
}

static int read_byte(mp_reader_t* r, uint8_t* b) {
    if (r->pos >= r->len) return -1;
    *b = r->data[r->pos++];
    return 0;
}

static int read_bytes(mp_reader_t* r, const uint8_t** out, size_t len) {
    if (r->pos + len > r->len) return -1;
    *out = r->data + r->pos;
    r->pos += len;
    return 0;
}

static uint16_t read_u16_be(const uint8_t* p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

static uint32_t read_u32_be(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | p[3];
}

static uint64_t read_u64_be(const uint8_t* p) {
    return ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) |
           ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32) |
           ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) |
           ((uint64_t)p[6] << 8)  | p[7];
}

uint8_t mp_peek_type(mp_reader_t* r) {
    if (r->pos >= r->len) return 0;
    return r->data[r->pos];
}

int mp_read_map(mp_reader_t* r, uint32_t* count) {
    uint8_t b;
    if (read_byte(r, &b)) return -1;
    if ((b & 0xF0) == 0x80) {
        *count = b & 0x0F;
        return 0;
    } else if (b == 0xDE) {
        const uint8_t* p;
        if (read_bytes(r, &p, 2)) return -1;
        *count = read_u16_be(p);
        return 0;
    } else if (b == 0xDF) {
        const uint8_t* p;
        if (read_bytes(r, &p, 4)) return -1;
        *count = read_u32_be(p);
        return 0;
    }
    return -1;
}

int mp_read_array(mp_reader_t* r, uint32_t* count) {
    uint8_t b;
    if (read_byte(r, &b)) return -1;
    if ((b & 0xF0) == 0x90) {
        *count = b & 0x0F;
        return 0;
    } else if (b == 0xDC) {
        const uint8_t* p;
        if (read_bytes(r, &p, 2)) return -1;
        *count = read_u16_be(p);
        return 0;
    } else if (b == 0xDD) {
        const uint8_t* p;
        if (read_bytes(r, &p, 4)) return -1;
        *count = read_u32_be(p);
        return 0;
    }
    return -1;
}

int mp_read_nil(mp_reader_t* r) {
    uint8_t b;
    if (read_byte(r, &b)) return -1;
    return (b == 0xC0) ? 0 : -1;
}

int mp_read_bool(mp_reader_t* r, bool* val) {
    uint8_t b;
    if (read_byte(r, &b)) return -1;
    if (b == 0xC3) { *val = true; return 0; }
    if (b == 0xC2) { *val = false; return 0; }
    return -1;
}

int mp_read_uint(mp_reader_t* r, uint64_t* val) {
    uint8_t b;
    if (read_byte(r, &b)) return -1;
    if (b <= 0x7F) {
        *val = b;
        return 0;
    }
    const uint8_t* p;
    switch (b) {
        case 0xCC:
            if (read_byte(r, &b)) return -1;
            *val = b;
            return 0;
        case 0xCD:
            if (read_bytes(r, &p, 2)) return -1;
            *val = read_u16_be(p);
            return 0;
        case 0xCE:
            if (read_bytes(r, &p, 4)) return -1;
            *val = read_u32_be(p);
            return 0;
        case 0xCF:
            if (read_bytes(r, &p, 8)) return -1;
            *val = read_u64_be(p);
            return 0;
        default:
            return -1;
    }
}

int mp_read_int(mp_reader_t* r, int64_t* val) {
    uint8_t b = mp_peek_type(r);
    if (b <= 0x7F || b == 0xCC || b == 0xCD || b == 0xCE || b == 0xCF) {
        uint64_t uval;
        if (mp_read_uint(r, &uval)) return -1;
        *val = (int64_t)uval;
        return 0;
    }
    if ((b & 0xE0) == 0xE0) {
        read_byte(r, &b);
        *val = (int8_t)b;
        return 0;
    }
    read_byte(r, &b);
    const uint8_t* p;
    switch (b) {
        case 0xD0:
            if (read_byte(r, &b)) return -1;
            *val = (int8_t)b;
            return 0;
        case 0xD1:
            if (read_bytes(r, &p, 2)) return -1;
            *val = (int16_t)read_u16_be(p);
            return 0;
        case 0xD2:
            if (read_bytes(r, &p, 4)) return -1;
            *val = (int32_t)read_u32_be(p);
            return 0;
        case 0xD3:
            if (read_bytes(r, &p, 8)) return -1;
            *val = (int64_t)read_u64_be(p);
            return 0;
        default:
            return -1;
    }
}

int mp_read_str(mp_reader_t* r, const char** str, uint32_t* len) {
    uint8_t b;
    if (read_byte(r, &b)) return -1;
    if ((b & 0xE0) == 0xA0) {
        *len = b & 0x1F;
    } else if (b == 0xD9) {
        uint8_t l;
        if (read_byte(r, &l)) return -1;
        *len = l;
    } else if (b == 0xDA) {
        const uint8_t* p;
        if (read_bytes(r, &p, 2)) return -1;
        *len = read_u16_be(p);
    } else if (b == 0xDB) {
        const uint8_t* p;
        if (read_bytes(r, &p, 4)) return -1;
        *len = read_u32_be(p);
    } else {
        return -1;
    }
    const uint8_t* p;
    if (*len > 0) {
        if (read_bytes(r, &p, *len)) return -1;
        *str = (const char*)p;
    } else {
        *str = "";
    }
    return 0;
}

int mp_read_bin(mp_reader_t* r, const uint8_t** data, uint32_t* len) {
    uint8_t b;
    if (read_byte(r, &b)) return -1;
    if (b == 0xC4) {
        uint8_t l;
        if (read_byte(r, &l)) return -1;
        *len = l;
    } else if (b == 0xC5) {
        const uint8_t* p;
        if (read_bytes(r, &p, 2)) return -1;
        *len = read_u16_be(p);
    } else if (b == 0xC6) {
        const uint8_t* p;
        if (read_bytes(r, &p, 4)) return -1;
        *len = read_u32_be(p);
    } else {
        return -1;
    }
    if (*len > 0) {
        if (read_bytes(r, data, *len)) return -1;
    } else {
        *data = (const uint8_t*)0;
    }
    return 0;
}

int mp_skip(mp_reader_t* r) {
    uint8_t b;
    if (read_byte(r, &b)) return -1;
    if (b <= 0x7F) return 0;
    if ((b & 0xE0) == 0xE0) return 0;
    if ((b & 0xF0) == 0x80) {
        uint32_t count = b & 0x0F;
        for (uint32_t i = 0; i < count * 2; i++)
            if (mp_skip(r)) return -1;
        return 0;
    }
    if ((b & 0xF0) == 0x90) {
        uint32_t count = b & 0x0F;
        for (uint32_t i = 0; i < count; i++)
            if (mp_skip(r)) return -1;
        return 0;
    }
    if ((b & 0xE0) == 0xA0) {
        uint32_t len = b & 0x1F;
        r->pos += len;
        return (r->pos <= r->len) ? 0 : -1;
    }
    const uint8_t* p;
    switch (b) {
        case 0xC0: case 0xC2: case 0xC3: return 0;
        case 0xCC: r->pos += 1; break;
        case 0xCD: r->pos += 2; break;
        case 0xCE: r->pos += 4; break;
        case 0xCF: r->pos += 8; break;
        case 0xD0: r->pos += 1; break;
        case 0xD1: r->pos += 2; break;
        case 0xD2: r->pos += 4; break;
        case 0xD3: r->pos += 8; break;
        case 0xCA: r->pos += 4; break;
        case 0xCB: r->pos += 8; break;
        case 0xC4:
            if (read_byte(r, &b)) return -1;
            r->pos += b;
            break;
        case 0xC5:
            if (read_bytes(r, &p, 2)) return -1;
            r->pos += read_u16_be(p);
            break;
        case 0xC6:
            if (read_bytes(r, &p, 4)) return -1;
            r->pos += read_u32_be(p);
            break;
        case 0xD9:
            if (read_byte(r, &b)) return -1;
            r->pos += b;
            break;
        case 0xDA:
            if (read_bytes(r, &p, 2)) return -1;
            r->pos += read_u16_be(p);
            break;
        case 0xDB:
            if (read_bytes(r, &p, 4)) return -1;
            r->pos += read_u32_be(p);
            break;
        case 0xDC: {
            if (read_bytes(r, &p, 2)) return -1;
            uint32_t count = read_u16_be(p);
            for (uint32_t i = 0; i < count; i++)
                if (mp_skip(r)) return -1;
            return 0;
        }
        case 0xDD: {
            if (read_bytes(r, &p, 4)) return -1;
            uint32_t count = read_u32_be(p);
            for (uint32_t i = 0; i < count; i++)
                if (mp_skip(r)) return -1;
            return 0;
        }
        case 0xDE: {
            if (read_bytes(r, &p, 2)) return -1;
            uint32_t count = read_u16_be(p);
            for (uint32_t i = 0; i < count * 2; i++)
                if (mp_skip(r)) return -1;
            return 0;
        }
        case 0xDF: {
            if (read_bytes(r, &p, 4)) return -1;
            uint32_t count = read_u32_be(p);
            for (uint32_t i = 0; i < count * 2; i++)
                if (mp_skip(r)) return -1;
            return 0;
        }
        default:
            return -1;
    }
    return (r->pos <= r->len) ? 0 : -1;
}

int mp_find_key_str(mp_reader_t* r, uint32_t map_count, const char* key) {
    size_t key_len = ax_strlen(key);
    for (uint32_t i = 0; i < map_count; i++) {
        const char* k;
        uint32_t klen;
        if (mp_read_str(r, &k, &klen)) return -1;
        if (klen == key_len && ax_memcmp(k, key, klen) == 0) {
            return 0;
        }
        if (mp_skip(r)) return -1;
    }
    return -1;
}
