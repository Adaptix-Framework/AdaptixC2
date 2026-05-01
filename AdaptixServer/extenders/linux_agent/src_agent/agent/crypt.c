#include "crypt.h"
#include "crt.h"

/// ---- AES-128 Core ----

static const uint8_t aes_sbox[256] = {
    0x63,0x7C,0x77,0x7B,0xF2,0x6B,0x6F,0xC5,0x30,0x01,0x67,0x2B,0xFE,0xD7,0xAB,0x76,
    0xCA,0x82,0xC9,0x7D,0xFA,0x59,0x47,0xF0,0xAD,0xD4,0xA2,0xAF,0x9C,0xA4,0x72,0xC0,
    0xB7,0xFD,0x93,0x26,0x36,0x3F,0xF7,0xCC,0x34,0xA5,0xE5,0xF1,0x71,0xD8,0x31,0x15,
    0x04,0xC7,0x23,0xC3,0x18,0x96,0x05,0x9A,0x07,0x12,0x80,0xE2,0xEB,0x27,0xB2,0x75,
    0x09,0x83,0x2C,0x1A,0x1B,0x6E,0x5A,0xA0,0x52,0x3B,0xD6,0xB3,0x29,0xE3,0x2F,0x84,
    0x53,0xD1,0x00,0xED,0x20,0xFC,0xB1,0x5B,0x6A,0xCB,0xBE,0x39,0x4A,0x4C,0x58,0xCF,
    0xD0,0xEF,0xAA,0xFB,0x43,0x4D,0x33,0x85,0x45,0xF9,0x02,0x7F,0x50,0x3C,0x9F,0xA8,
    0x51,0xA3,0x40,0x8F,0x92,0x9D,0x38,0xF5,0xBC,0xB6,0xDA,0x21,0x10,0xFF,0xF3,0xD2,
    0xCD,0x0C,0x13,0xEC,0x5F,0x97,0x44,0x17,0xC4,0xA7,0x7E,0x3D,0x64,0x5D,0x19,0x73,
    0x60,0x81,0x4F,0xDC,0x22,0x2A,0x90,0x88,0x46,0xEE,0xB8,0x14,0xDE,0x5E,0x0B,0xDB,
    0xE0,0x32,0x3A,0x0A,0x49,0x06,0x24,0x5C,0xC2,0xD3,0xAC,0x62,0x91,0x95,0xE4,0x79,
    0xE7,0xC8,0x37,0x6D,0x8D,0xD5,0x4E,0xA9,0x6C,0x56,0xF4,0xEA,0x65,0x7A,0xAE,0x08,
    0xBA,0x78,0x25,0x2E,0x1C,0xA6,0xB4,0xC6,0xE8,0xDD,0x74,0x1F,0x4B,0xBD,0x8B,0x8A,
    0x70,0x3E,0xB5,0x66,0x48,0x03,0xF6,0x0E,0x61,0x35,0x57,0xB9,0x86,0xC1,0x1D,0x9E,
    0xE1,0xF8,0x98,0x11,0x69,0xD9,0x8E,0x94,0x9B,0x1E,0x87,0xE9,0xCE,0x55,0x28,0xDF,
    0x8C,0xA1,0x89,0x0D,0xBF,0xE6,0x42,0x68,0x41,0x99,0x2D,0x0F,0xB0,0x54,0xBB,0x16
};

static const uint8_t aes_rcon[10] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1B, 0x36
};

#define AES128_ROUNDS 10
#define AES128_NK     4
#define AES128_NB     4

static void aes128_key_expand(const uint8_t* key, uint8_t* rk) {
    ax_memcpy(rk, key, 16);
    for (int i = AES128_NK; i < AES128_NB * (AES128_ROUNDS + 1); i++) {
        uint8_t temp[4];
        temp[0] = rk[(i-1)*4 + 0];
        temp[1] = rk[(i-1)*4 + 1];
        temp[2] = rk[(i-1)*4 + 2];
        temp[3] = rk[(i-1)*4 + 3];
        if (i % AES128_NK == 0) {
            uint8_t t = temp[0];
            temp[0] = temp[1]; temp[1] = temp[2];
            temp[2] = temp[3]; temp[3] = t;
            temp[0] = aes_sbox[temp[0]]; temp[1] = aes_sbox[temp[1]];
            temp[2] = aes_sbox[temp[2]]; temp[3] = aes_sbox[temp[3]];
            temp[0] ^= aes_rcon[i/AES128_NK - 1];
        }
        rk[i*4 + 0] = rk[(i-AES128_NK)*4 + 0] ^ temp[0];
        rk[i*4 + 1] = rk[(i-AES128_NK)*4 + 1] ^ temp[1];
        rk[i*4 + 2] = rk[(i-AES128_NK)*4 + 2] ^ temp[2];
        rk[i*4 + 3] = rk[(i-AES128_NK)*4 + 3] ^ temp[3];
    }
}

static uint8_t gf_mul(uint8_t a, uint8_t b) {
    uint8_t result = 0;
    while (b) {
        if (b & 1) result ^= a;
        uint8_t hi = a & 0x80;
        a <<= 1;
        if (hi) a ^= 0x1B;
        b >>= 1;
    }
    return result;
}

static void sub_bytes(uint8_t* state) {
    for (int i = 0; i < 16; i++)
        state[i] = aes_sbox[state[i]];
}

static void shift_rows(uint8_t* s) {
    uint8_t t;
    t = s[1]; s[1] = s[5]; s[5] = s[9]; s[9] = s[13]; s[13] = t;
    t = s[2]; s[2] = s[10]; s[10] = t; t = s[6]; s[6] = s[14]; s[14] = t;
    t = s[15]; s[15] = s[11]; s[11] = s[7]; s[7] = s[3]; s[3] = t;
}

static void mix_columns(uint8_t* s) {
    for (int c = 0; c < 4; c++) {
        int i = c * 4;
        uint8_t a0 = s[i], a1 = s[i+1], a2 = s[i+2], a3 = s[i+3];
        s[i]   = gf_mul(a0,2) ^ gf_mul(a1,3) ^ a2 ^ a3;
        s[i+1] = a0 ^ gf_mul(a1,2) ^ gf_mul(a2,3) ^ a3;
        s[i+2] = a0 ^ a1 ^ gf_mul(a2,2) ^ gf_mul(a3,3);
        s[i+3] = gf_mul(a0,3) ^ a1 ^ a2 ^ gf_mul(a3,2);
    }
}

static void add_round_key(uint8_t* state, const uint8_t* rk, int round) {
    for (int i = 0; i < 16; i++)
        state[i] ^= rk[round * 16 + i];
}

static void aes128_encrypt_block(const uint8_t* in, uint8_t* out, const uint8_t* rk) {
    uint8_t state[16];
    ax_memcpy(state, in, 16);
    add_round_key(state, rk, 0);
    for (int round = 1; round < AES128_ROUNDS; round++) {
        sub_bytes(state);
        shift_rows(state);
        mix_columns(state);
        add_round_key(state, rk, round);
    }
    sub_bytes(state);
    shift_rows(state);
    add_round_key(state, rk, AES128_ROUNDS);
    ax_memcpy(out, state, 16);
}

/// ---- GCM Mode ----

static void ghash_mul(uint8_t* x, const uint8_t* h) {
    uint8_t z[16] = {0};
    uint8_t v[16];
    ax_memcpy(v, h, 16);
    for (int i = 0; i < 128; i++) {
        if (x[i / 8] & (0x80 >> (i % 8))) {
            for (int j = 0; j < 16; j++) z[j] ^= v[j];
        }
        uint8_t carry = v[15] & 1;
        for (int j = 15; j > 0; j--)
            v[j] = (v[j] >> 1) | (v[j-1] << 7);
        v[0] >>= 1;
        if (carry) v[0] ^= 0xE1;
    }
    ax_memcpy(x, z, 16);
}

static void inc32(uint8_t* counter) {
    for (int i = 15; i >= 12; i--) {
        if (++counter[i]) break;
    }
}

static void aes_ctr(const uint8_t* rk, uint8_t* counter,
                    const uint8_t* in, uint8_t* out, size_t len) {
    uint8_t keystream[16];
    size_t offset = 0;
    while (offset < len) {
        aes128_encrypt_block(counter, keystream, rk);
        inc32(counter);
        size_t chunk = len - offset;
        if (chunk > 16) chunk = 16;
        for (size_t i = 0; i < chunk; i++)
            out[offset + i] = in[offset + i] ^ keystream[i];
        offset += chunk;
    }
}

/// ---- Public API ----

uint8_t* aes128_gcm_encrypt(const uint8_t* plaintext, size_t plaintext_len,
                             const uint8_t* key, size_t* out_len) {
    uint8_t rk[176];
    aes128_key_expand(key, rk);

    uint8_t h[16] = {0};
    aes128_encrypt_block(h, h, rk);

    uint8_t nonce[GCM_NONCE_SIZE];
    ax_random_bytes(nonce, GCM_NONCE_SIZE);

    uint8_t j0[16] = {0};
    ax_memcpy(j0, nonce, GCM_NONCE_SIZE);
    j0[15] = 1;

    uint8_t counter[16];
    ax_memcpy(counter, j0, 16);
    inc32(counter);

    *out_len = GCM_NONCE_SIZE + plaintext_len + GCM_TAG_SIZE;
    uint8_t* output = (uint8_t*)ax_malloc(*out_len);
    if (!output) return (uint8_t*)0;

    ax_memcpy(output, nonce, GCM_NONCE_SIZE);

    uint8_t* ct = output + GCM_NONCE_SIZE;
    if (plaintext_len > 0) {
        aes_ctr(rk, counter, plaintext, ct, plaintext_len);
    }

    uint8_t ghash_out[16] = {0};
    size_t ct_blocks = plaintext_len / 16;
    for (size_t i = 0; i < ct_blocks; i++) {
        for (int j = 0; j < 16; j++)
            ghash_out[j] ^= ct[i * 16 + j];
        ghash_mul(ghash_out, h);
    }
    size_t ct_rem = plaintext_len % 16;
    if (ct_rem > 0) {
        for (size_t j = 0; j < ct_rem; j++)
            ghash_out[j] ^= ct[ct_blocks * 16 + j];
        ghash_mul(ghash_out, h);
    }

    uint8_t len_block[16] = {0};
    uint64_t ct_bits = (uint64_t)plaintext_len * 8;
    len_block[8]  = (uint8_t)(ct_bits >> 56);
    len_block[9]  = (uint8_t)(ct_bits >> 48);
    len_block[10] = (uint8_t)(ct_bits >> 40);
    len_block[11] = (uint8_t)(ct_bits >> 32);
    len_block[12] = (uint8_t)(ct_bits >> 24);
    len_block[13] = (uint8_t)(ct_bits >> 16);
    len_block[14] = (uint8_t)(ct_bits >> 8);
    len_block[15] = (uint8_t)(ct_bits);
    for (int j = 0; j < 16; j++)
        ghash_out[j] ^= len_block[j];
    ghash_mul(ghash_out, h);

    uint8_t tag[16];
    aes128_encrypt_block(j0, tag, rk);
    for (int j = 0; j < 16; j++)
        tag[j] ^= ghash_out[j];

    ax_memcpy(output + GCM_NONCE_SIZE + plaintext_len, tag, GCM_TAG_SIZE);

    ax_memset(rk, 0, sizeof(rk));
    ax_memset(h, 0, sizeof(h));

    return output;
}

uint8_t* aes128_gcm_decrypt(const uint8_t* data, size_t data_len,
                             const uint8_t* key, size_t* out_len) {
    if (data_len < GCM_NONCE_SIZE + GCM_TAG_SIZE)
        return (uint8_t*)0;

    size_t ct_len = data_len - GCM_NONCE_SIZE - GCM_TAG_SIZE;
    const uint8_t* nonce = data;
    const uint8_t* ct = data + GCM_NONCE_SIZE;
    const uint8_t* tag = data + GCM_NONCE_SIZE + ct_len;

    uint8_t rk[176];
    aes128_key_expand(key, rk);

    uint8_t h[16] = {0};
    aes128_encrypt_block(h, h, rk);

    uint8_t j0[16] = {0};
    ax_memcpy(j0, nonce, GCM_NONCE_SIZE);
    j0[15] = 1;

    uint8_t ghash_out[16] = {0};
    size_t ct_blocks = ct_len / 16;
    for (size_t i = 0; i < ct_blocks; i++) {
        for (int j = 0; j < 16; j++)
            ghash_out[j] ^= ct[i * 16 + j];
        ghash_mul(ghash_out, h);
    }
    size_t ct_rem = ct_len % 16;
    if (ct_rem > 0) {
        for (size_t j = 0; j < ct_rem; j++)
            ghash_out[j] ^= ct[ct_blocks * 16 + j];
        ghash_mul(ghash_out, h);
    }

    uint8_t len_block[16] = {0};
    uint64_t ct_bits = (uint64_t)ct_len * 8;
    len_block[8]  = (uint8_t)(ct_bits >> 56);
    len_block[9]  = (uint8_t)(ct_bits >> 48);
    len_block[10] = (uint8_t)(ct_bits >> 40);
    len_block[11] = (uint8_t)(ct_bits >> 32);
    len_block[12] = (uint8_t)(ct_bits >> 24);
    len_block[13] = (uint8_t)(ct_bits >> 16);
    len_block[14] = (uint8_t)(ct_bits >> 8);
    len_block[15] = (uint8_t)(ct_bits);
    for (int j = 0; j < 16; j++)
        ghash_out[j] ^= len_block[j];
    ghash_mul(ghash_out, h);

    uint8_t computed_tag[16];
    aes128_encrypt_block(j0, computed_tag, rk);
    for (int j = 0; j < 16; j++)
        computed_tag[j] ^= ghash_out[j];

    // Constant-time tag comparison
    uint8_t diff = 0;
    for (int j = 0; j < GCM_TAG_SIZE; j++)
        diff |= computed_tag[j] ^ tag[j];

    if (diff != 0) {
        ax_memset(rk, 0, sizeof(rk));
        ax_memset(h, 0, sizeof(h));
        return (uint8_t*)0;
    }

    *out_len = ct_len;
    uint8_t* plaintext = (uint8_t*)ax_malloc(ct_len > 0 ? ct_len : 1);
    if (!plaintext) {
        ax_memset(rk, 0, sizeof(rk));
        return (uint8_t*)0;
    }

    uint8_t counter[16];
    ax_memcpy(counter, j0, 16);
    inc32(counter);

    if (ct_len > 0) {
        aes_ctr(rk, counter, ct, plaintext, ct_len);
    }

    ax_memset(rk, 0, sizeof(rk));
    ax_memset(h, 0, sizeof(h));

    return plaintext;
}

/// ---- Public AES-CTR wrappers (for tunnel/terminal) ----

void aes128_expand_key(const uint8_t* key, uint8_t* round_keys) {
    aes128_key_expand(key, round_keys);
}

void aes128_ctr_init(aes128_ctr_ctx_t* ctx, const uint8_t* key, const uint8_t* iv) {
    aes128_key_expand(key, ctx->round_keys);
    for (int i = 0; i < 16; i++) ctx->counter[i] = iv[i];
    ctx->ks_offset = 16;
    for (int i = 0; i < 16; i++) ctx->keystream[i] = 0;
}

void aes128_ctr_process(aes128_ctr_ctx_t* ctx,
                        const uint8_t* in, uint8_t* out, size_t len) {
    size_t pos = 0;
    while (pos < len && ctx->ks_offset < 16) {
        out[pos] = in[pos] ^ ctx->keystream[ctx->ks_offset];
        ctx->ks_offset++;
        pos++;
    }
    while (pos + 16 <= len) {
        aes128_encrypt_block(ctx->counter, ctx->keystream, ctx->round_keys);
        inc32(ctx->counter);
        for (int i = 0; i < 16; i++)
            out[pos + i] = in[pos + i] ^ ctx->keystream[i];
        pos += 16;
    }
    if (pos < len) {
        aes128_encrypt_block(ctx->counter, ctx->keystream, ctx->round_keys);
        inc32(ctx->counter);
        ctx->ks_offset = 0;
        while (pos < len) {
            out[pos] = in[pos] ^ ctx->keystream[ctx->ks_offset];
            ctx->ks_offset++;
            pos++;
        }
    }
}
