#ifndef MSGPACK_H
#define MSGPACK_H

#include "types.h"
#include "crt.h"

/// ---- Writer (encoder) ----

typedef struct {
    buffer_t buf;
} mp_writer_t;

int  mp_writer_init(mp_writer_t *w, size_t cap);
void mp_writer_free(mp_writer_t *w);

int mp_write_map(mp_writer_t *w, uint32_t count);
int mp_write_array(mp_writer_t *w, uint32_t count);
int mp_write_nil(mp_writer_t *w);
int mp_write_bool(mp_writer_t *w, bool val);
int mp_write_uint(mp_writer_t *w, uint64_t val);
int mp_write_int(mp_writer_t *w, int64_t val);
int mp_write_str(mp_writer_t *w, const char *str, uint32_t len);
int mp_write_bin(mp_writer_t *w, const uint8_t *data, uint32_t len);

int mp_write_kv_str(mp_writer_t *w, const char *key, const char *val);
int mp_write_kv_bin(mp_writer_t *w, const char *key, const uint8_t *data, uint32_t len);
int mp_write_kv_uint(mp_writer_t *w, const char *key, uint64_t val);
int mp_write_kv_int(mp_writer_t *w, const char *key, int64_t val);
int mp_write_kv_bool(mp_writer_t *w, const char *key, bool val);

/// ---- Reader (decoder) ----

typedef struct {
    const uint8_t *data;
    size_t         len;
    size_t         pos;
} mp_reader_t;

void    mp_reader_init(mp_reader_t *r, const uint8_t *data, size_t len);
uint8_t mp_peek_type(mp_reader_t *r);
int     mp_skip(mp_reader_t *r);

int mp_read_map(mp_reader_t *r, uint32_t *count);
int mp_read_array(mp_reader_t *r, uint32_t *count);
int mp_read_nil(mp_reader_t *r);
int mp_read_bool(mp_reader_t *r, bool *val);
int mp_read_uint(mp_reader_t *r, uint64_t *val);
int mp_read_int(mp_reader_t *r, int64_t *val);
int mp_read_str(mp_reader_t *r, const char **str, uint32_t *len);
int mp_read_bin(mp_reader_t *r, const uint8_t **data, uint32_t *len);

int mp_find_key_str(mp_reader_t *r, uint32_t map_count, const char *key);

#endif // MSGPACK_H
