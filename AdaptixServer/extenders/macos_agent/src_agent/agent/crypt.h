#ifndef CRYPT_H
#define CRYPT_H

#include <stdint.h>
#include <stddef.h>

/// AES-128-GCM encryption/decryption
/// Format: [nonce 12 bytes][ciphertext][tag 16 bytes]
/// Key: 16 bytes (AES-128)
/// Matches Go's crypto/aes + cipher.NewGCM with 16-byte key

#define AES_KEY_SIZE    16
#define AES_BLOCK_SIZE  16
#define GCM_NONCE_SIZE  12
#define GCM_TAG_SIZE    16

// Encrypt: allocates output buffer [nonce][ciphertext][tag]
// Returns output and sets *out_len. Caller must free output.
uint8_t* aes128_gcm_encrypt(const uint8_t* plaintext, size_t plaintext_len,
                             const uint8_t* key,
                             size_t* out_len);

// Decrypt: allocates output buffer (plaintext)
// Input format: [nonce 12B][ciphertext][tag 16B]
// Returns plaintext and sets *out_len. Caller must free output.
// Returns NULL on authentication failure.
uint8_t* aes128_gcm_decrypt(const uint8_t* data, size_t data_len,
                             const uint8_t* key,
                             size_t* out_len);

/// AES-128-CTR for tunnel/terminal streaming
/// Key: 16 bytes, IV: 16 bytes (used as initial counter)

/// Expand AES-128 key into round keys (176 bytes)
void aes128_expand_key(const uint8_t* key, uint8_t* round_keys);

/// CTR stream context — preserves partial keystream between calls
/// This matches Go's cipher.NewCTR behavior where partial blocks
/// are carried across calls.
typedef struct {
    uint8_t round_keys[176];
    uint8_t counter[16];
    uint8_t keystream[16];  // cached keystream block
    uint8_t ks_offset;      // how many bytes used in current keystream (0-16)
} aes128_ctr_ctx_t;

/// Initialize CTR context with key and IV
void aes128_ctr_init(aes128_ctr_ctx_t* ctx, const uint8_t* key, const uint8_t* iv);

/// Process data with CTR stream (preserves partial block state)
void aes128_ctr_process(aes128_ctr_ctx_t* ctx,
                        const uint8_t* in, uint8_t* out, size_t len);

#endif // CRYPT_H
