#include "Crypt.h"
#include "utils.h"

static const unsigned char g_sbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

static const unsigned char g_rcon[10] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

static unsigned char gf_mul(unsigned char a, unsigned char b) {
    unsigned char p = 0;
    for (int i = 0; i < 8; i++) {
        if (b & 1)
            p ^= a;
        unsigned char hi = a & 0x80;
        a <<= 1;
        if (hi)
            a ^= 0x1b;
        b >>= 1;
    }
    return p;
}

static void aes256_key_expand(const unsigned char* key, unsigned char* roundKeys) {
    unsigned char temp[4];
    int i;

    for (i = 0; i < 32; i++)
        roundKeys[i] = key[i];

    int nk = 8;
    int nb = 4;
    int nr = 14;

    for (i = nk; i < nb * (nr + 1); i++) {
        temp[0] = roundKeys[(i-1)*4 + 0];
        temp[1] = roundKeys[(i-1)*4 + 1];
        temp[2] = roundKeys[(i-1)*4 + 2];
        temp[3] = roundKeys[(i-1)*4 + 3];

        if (i % nk == 0) {
            unsigned char t = temp[0];
            temp[0] = temp[1];
            temp[1] = temp[2];
            temp[2] = temp[3];
            temp[3] = t;
            temp[0] = g_sbox[temp[0]];
            temp[1] = g_sbox[temp[1]];
            temp[2] = g_sbox[temp[2]];
            temp[3] = g_sbox[temp[3]];
            temp[0] ^= g_rcon[i/nk - 1];
        }
        else if (i % nk == 4) {
            temp[0] = g_sbox[temp[0]];
            temp[1] = g_sbox[temp[1]];
            temp[2] = g_sbox[temp[2]];
            temp[3] = g_sbox[temp[3]];
        }

        roundKeys[i*4 + 0] = roundKeys[(i-nk)*4 + 0] ^ temp[0];
        roundKeys[i*4 + 1] = roundKeys[(i-nk)*4 + 1] ^ temp[1];
        roundKeys[i*4 + 2] = roundKeys[(i-nk)*4 + 2] ^ temp[2];
        roundKeys[i*4 + 3] = roundKeys[(i-nk)*4 + 3] ^ temp[3];
    }
}

static void aes_add_round_key(unsigned char* state, const unsigned char* rk) {
    for (int i = 0; i < 16; i++)
        state[i] ^= rk[i];
}

static void aes_sub_bytes(unsigned char* state) {
    for (int i = 0; i < 16; i++)
        state[i] = g_sbox[state[i]];
}

static void aes_shift_rows(unsigned char* state) {
    unsigned char t;
    t = state[1]; state[1] = state[5]; state[5] = state[9]; state[9] = state[13]; state[13] = t;
    t = state[2]; state[2] = state[10]; state[10] = t;
    t = state[6]; state[6] = state[14]; state[14] = t;
    t = state[15]; state[15] = state[11]; state[11] = state[7]; state[7] = state[3]; state[3] = t;
}

static void aes_mix_columns(unsigned char* state) {
    for (int c = 0; c < 4; c++) {
        int i = c * 4;
        unsigned char s0 = state[i], s1 = state[i+1], s2 = state[i+2], s3 = state[i+3];
        state[i+0] = gf_mul(s0, 2) ^ gf_mul(s1, 3) ^ s2 ^ s3;
        state[i+1] = s0 ^ gf_mul(s1, 2) ^ gf_mul(s2, 3) ^ s3;
        state[i+2] = s0 ^ s1 ^ gf_mul(s2, 2) ^ gf_mul(s3, 3);
        state[i+3] = gf_mul(s0, 3) ^ s1 ^ s2 ^ gf_mul(s3, 2);
    }
}

static void aes256_encrypt_block(const unsigned char* roundKeys, const unsigned char* in, unsigned char* out) {
    unsigned char state[16];
    memcpy(state, in, 16);

    aes_add_round_key(state, roundKeys);

    for (int r = 1; r < 14; r++) {
        aes_sub_bytes(state);
        aes_shift_rows(state);
        aes_mix_columns(state);
        aes_add_round_key(state, roundKeys + r * 16);
    }

    aes_sub_bytes(state);
    aes_shift_rows(state);
    aes_add_round_key(state, roundKeys + 14 * 16);

    memcpy(out, state, 16);
}

// ========== GCM ==========

static void gcm_inc32(unsigned char* ctr) {
    for (int i = 15; i >= 12; i--) {
        if (++ctr[i])
            break;
    }
}

static void ghash_mul(unsigned char* x, const unsigned char* h) {
    unsigned char z[16] = {0};
    unsigned char v[16];
    memcpy(v, h, 16);

    for (int i = 0; i < 128; i++) {
        if (x[i / 8] & (1 << (7 - (i % 8)))) {
            for (int j = 0; j < 16; j++)
                z[j] ^= v[j];
        }
        unsigned char carry = v[15] & 1;
        for (int j = 15; j > 0; j--)
            v[j] = (v[j] >> 1) | (v[j-1] << 7);
        v[0] >>= 1;
        if (carry)
            v[0] ^= 0xe1;
    }
    memcpy(x, z, 16);
}

static void ghash(const unsigned char* h, const unsigned char* aad, int aadLen,
                   const unsigned char* ct, int ctLen, unsigned char* out) {
    unsigned char x[16] = {0};
    int i, j;

    for (i = 0; i < aadLen; i += 16) {
        int blockLen = aadLen - i;
        if (blockLen > 16) blockLen = 16;
        for (j = 0; j < blockLen; j++)
            x[j] ^= aad[i + j];
        ghash_mul(x, h);
    }

    for (i = 0; i < ctLen; i += 16) {
        int blockLen = ctLen - i;
        if (blockLen > 16) blockLen = 16;
        for (j = 0; j < blockLen; j++)
            x[j] ^= ct[i + j];
        ghash_mul(x, h);
    }

    unsigned char lenBlock[16] = {0};
    unsigned long long aadBits = (unsigned long long)aadLen * 8;
    unsigned long long ctBits  = (unsigned long long)ctLen * 8;
    lenBlock[0]  = (unsigned char)(aadBits >> 56);
    lenBlock[1]  = (unsigned char)(aadBits >> 48);
    lenBlock[2]  = (unsigned char)(aadBits >> 40);
    lenBlock[3]  = (unsigned char)(aadBits >> 32);
    lenBlock[4]  = (unsigned char)(aadBits >> 24);
    lenBlock[5]  = (unsigned char)(aadBits >> 16);
    lenBlock[6]  = (unsigned char)(aadBits >> 8);
    lenBlock[7]  = (unsigned char)(aadBits);
    lenBlock[8]  = (unsigned char)(ctBits >> 56);
    lenBlock[9]  = (unsigned char)(ctBits >> 48);
    lenBlock[10] = (unsigned char)(ctBits >> 40);
    lenBlock[11] = (unsigned char)(ctBits >> 32);
    lenBlock[12] = (unsigned char)(ctBits >> 24);
    lenBlock[13] = (unsigned char)(ctBits >> 16);
    lenBlock[14] = (unsigned char)(ctBits >> 8);
    lenBlock[15] = (unsigned char)(ctBits);

    for (j = 0; j < 16; j++)
        x[j] ^= lenBlock[j];
    ghash_mul(x, h);

    memcpy(out, x, 16);
}

static int aes256_gcm_crypt(
    const unsigned char* key,
    const unsigned char* iv, int ivLen,
    const unsigned char* aad, int aadLen,
    const unsigned char* in, int inLen,
    unsigned char* out,
    unsigned char* tag, int tagLen,
    int mode)
{
    unsigned char roundKeys[240];
    aes256_key_expand(key, roundKeys);

    unsigned char h[16] = {0};
    aes256_encrypt_block(roundKeys, h, h);

    unsigned char j0[16] = {0};
    if (ivLen == 12) {
        memcpy(j0, iv, 12);
        j0[15] = 1;
    } else {
        ghash(h, NULL, 0, iv, ivLen, j0);
    }

    unsigned char ctr[16];
    memcpy(ctr, j0, 16);
    gcm_inc32(ctr);

    unsigned char keystreamBlock[16];
    for (int i = 0; i < inLen; i += 16) {
        aes256_encrypt_block(roundKeys, ctr, keystreamBlock);
        gcm_inc32(ctr);
        int blockLen = inLen - i;
        if (blockLen > 16) blockLen = 16;
        for (int j = 0; j < blockLen; j++)
            out[i + j] = in[i + j] ^ keystreamBlock[j];
    }

    const unsigned char* ctData = (mode == 0) ? out : in;
    unsigned char ghashResult[16];
    ghash(h, aad, aadLen, ctData, inLen, ghashResult);

    unsigned char encJ0[16];
    aes256_encrypt_block(roundKeys, j0, encJ0);

    unsigned char computedTag[16];
    for (int i = 0; i < 16; i++)
        computedTag[i] = ghashResult[i] ^ encJ0[i];

    if (mode == 0) {
        memcpy(tag, computedTag, tagLen);
        return 0;
    } else {
        unsigned char diff = 0;
        for (int i = 0; i < tagLen; i++)
            diff |= tag[i] ^ computedTag[i];
        return diff ? -1 : 0;
    }
}

// ========== Public API ==========

unsigned char* EncryptAES256GCM(unsigned char* data, int dataLen, unsigned char* key, int* outLen) {
    int totalLen = AES_GCM_IV_SIZE + dataLen + AES_GCM_TAG_SIZE;
    unsigned char* output = (unsigned char*)MemAllocLocal(totalLen);
    if (!output)
        return NULL;

    unsigned char* iv = output;
    for (int i = 0; i < AES_GCM_IV_SIZE; i++)
        iv[i] = (unsigned char)(GenerateRandom32() & 0xFF);

    unsigned char* ct  = output + AES_GCM_IV_SIZE;
    unsigned char* tag = output + AES_GCM_IV_SIZE + dataLen;

    aes256_gcm_crypt(key, iv, AES_GCM_IV_SIZE, NULL, 0, data, dataLen, ct, tag, AES_GCM_TAG_SIZE, 0);

    *outLen = totalLen;
    return output;
}

int DecryptAES256GCM(unsigned char* data, int dataLen, unsigned char* key, int* outLen) {
    if (dataLen < AES_GCM_OVERHEAD)
        return -1;

    unsigned char* iv  = data;
    int ctLen = dataLen - AES_GCM_OVERHEAD;
    unsigned char* ct  = data + AES_GCM_IV_SIZE;
    unsigned char* tag = data + AES_GCM_IV_SIZE + ctLen;

    unsigned char* plain = (unsigned char*)MemAllocLocal(ctLen);
    if (!plain)
        return -1;

    int result = aes256_gcm_crypt(key, iv, AES_GCM_IV_SIZE, NULL, 0, ct, ctLen, plain, tag, AES_GCM_TAG_SIZE, 1);

    if (result == 0) {
        memcpy(data, plain, ctLen);
        *outLen = ctLen;
    }

    MemFreeLocal((LPVOID*)&plain, ctLen);
    return result;
}

void CryptAES256Stream(unsigned char* data, int dataLen, unsigned char* key) {
    unsigned char roundKeys[240];
    aes256_key_expand(key, roundKeys);

    unsigned char ctr[16] = {0};
    ctr[15] = 1;

    unsigned char keystreamBlock[16];
    for (int i = 0; i < dataLen; i += 16) {
        aes256_encrypt_block(roundKeys, ctr, keystreamBlock);
        gcm_inc32(ctr);
        int blockLen = dataLen - i;
        if (blockLen > 16) blockLen = 16;
        for (int j = 0; j < blockLen; j++)
            data[i + j] ^= keystreamBlock[j];
    }
}
