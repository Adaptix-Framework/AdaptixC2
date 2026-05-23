#pragma once

// ========== AES-256-GCM (session encryption) ==========
// Key: 32 bytes, IV: 12 bytes (random), Tag: 16 bytes
// Encrypt output format: [IV(12)] [Ciphertext(dataLen)] [Tag(16)]
// Total output size = 12 + dataLen + 16 = dataLen + 28
// Decrypt input format:  [IV(12)] [Ciphertext(len-28)]  [Tag(16)]

#define AES_GCM_KEY_SIZE  32
#define AES_GCM_IV_SIZE   12
#define AES_GCM_TAG_SIZE  16
#define AES_GCM_OVERHEAD  (AES_GCM_IV_SIZE + AES_GCM_TAG_SIZE)

// Returns newly allocated buffer (via MemAllocLocal) containing [IV][Ciphertext][Tag].
// Caller must free with MemFreeLocal. *outLen set to total output size.
unsigned char* EncryptAES256GCM(unsigned char* data, int dataLen, unsigned char* key, int* outLen);

// Decrypts [IV][Ciphertext][Tag] in-place (overwrites input buffer with plaintext).
// Returns 0 on success, -1 on auth failure. *outLen set to plaintext size.
int DecryptAES256GCM(unsigned char* data, int dataLen, unsigned char* key, int* outLen);

// AES-256-CTR stream cipher: in-place encrypt/decrypt with zero overhead.
// Deterministic keystream from key (counter starts at 1).
// Symmetric: encrypt and decrypt are the same operation.
void CryptAES256Stream(unsigned char* data, int dataLen, unsigned char* key);
