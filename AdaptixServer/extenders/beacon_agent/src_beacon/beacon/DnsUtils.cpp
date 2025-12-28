#include "DnsUtils.h"
#include "utils.h"

extern "C" int __cdecl _snprintf(char*, size_t, const char*, ...);
extern "C" int __cdecl _vsnprintf(char*, size_t, const char*, va_list);

static const ULONG DNS_SEQ_XOR_MASK = 0x39913991;

static const CHAR BASE32_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

static const int BASE64_DECODE_TABLE[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1
};

void DnsToHex32(ULONG value, CHAR out[9])
{
    static const CHAR hex[] = "0123456789abcdef";
    for (int i = 7; i >= 0; --i) {
        out[i] = hex[value & 0x0F];
        value >>= 4;
    }
    out[8] = '\0';
}

ULONG DnsBase32Encode(const BYTE* src, ULONG srcLen, CHAR* dst, ULONG dstSize)
{
    if (!src || !dst || !srcLen || !dstSize)
        return 0;

    ULONG bitBuffer = 0;
    int   bitCount  = 0;
    ULONG outLen    = 0;

    for (ULONG i = 0; i < srcLen; ++i) {
        bitBuffer = (bitBuffer << 8) | src[i];
        bitCount += 8;
        while (bitCount >= 5) {
            bitCount -= 5;
            if (outLen + 1 >= dstSize)
                return 0;
            ULONG index = (bitBuffer >> bitCount) & 0x1F;
            dst[outLen++] = BASE32_ALPHABET[index];
        }
    }

    if (bitCount > 0) {
        if (outLen + 1 >= dstSize)
            return 0;
        ULONG index = (bitBuffer << (5 - bitCount)) & 0x1F;
        dst[outLen++] = BASE32_ALPHABET[index];
    }

    if (outLen < dstSize)
        dst[outLen] = '\0';
    return outLen;
}

ULONG DnsBase32Decode(const CHAR* src, ULONG srcLen, BYTE* dst, ULONG dstSize)
{
    if (!src || !dst || !srcLen || !dstSize)
        return 0;

    ULONG bitBuffer = 0;
    int   bitCount  = 0;
    ULONG outLen    = 0;

    for (ULONG i = 0; i < srcLen; ++i) {
        CHAR c = src[i];
        int val = -1;
        
        if (c >= 'A' && c <= 'Z')
            val = c - 'A';
        else if (c >= 'a' && c <= 'z')
            val = c - 'a';
        else if (c >= '2' && c <= '7')
            val = c - '2' + 26;
        else
            continue;

        bitBuffer = (bitBuffer << 5) | val;
        bitCount += 5;
        
        while (bitCount >= 8) {
            bitCount -= 8;
            if (outLen >= dstSize)
                return 0;
            dst[outLen++] = (BYTE)((bitBuffer >> bitCount) & 0xFF);
        }
    }

    return outLen;
}

void DnsBuildQName(const CHAR* sid, const CHAR* op, ULONG seq, ULONG idx, 
                   const CHAR* dataLabel, const CHAR* domain, CHAR* out, ULONG outSize)
{
    CHAR seqHex[9];
    CHAR idxHex[9];
    
    DnsToHex32(seq ^ DNS_SEQ_XOR_MASK, seqHex);
    DnsToHex32(idx ^ DNS_SEQ_XOR_MASK, idxHex);

    const CHAR* dataPart = (dataLabel && dataLabel[0]) ? dataLabel : "x";
    const CHAR* domPart  = (domain && domain[0]) ? domain : "";

    if (domPart[0])
        _snprintf(out, outSize, "%s.%s.%s.%s.%s.%s", sid, op, seqHex, idxHex, dataPart, domPart);
    else
        _snprintf(out, outSize, "%s.%s.%s.%s.%s", sid, op, seqHex, idxHex, dataPart);
}

int DnsEncodeName(const CHAR* host, BYTE* buf, int bufSize)
{
    if (!host || !buf || bufSize <= 1)
        return -1;

    int len = 0;
    const CHAR* p = host;
    
    while (*p) {
        const CHAR* labelStart = p;
        int labelLen = 0;
        
        while (*p && *p != '.') {
            ++p;
            ++labelLen;
        }
        
        if (labelLen == 0)
            break;
        if (labelLen > 63)
            labelLen = 63;

        if (len + 1 + labelLen + 1 > bufSize)
            return -1;

        buf[len++] = (BYTE)labelLen;
        memcpy(buf + len, labelStart, labelLen);
        len += labelLen;

        if (*p == '.')
            ++p;
    }
    
    if (len + 1 > bufSize)
        return -1;
    buf[len++] = 0;
    return len;
}

BOOL DnsBuildDataLabels(const BYTE* src, ULONG srcLen, ULONG labelSize, 
                        CHAR* out, ULONG outSize)
{
    if (!src || !srcLen || !out || !labelSize || labelSize > 63 || outSize == 0)
        return FALSE;

    CHAR encoded[2048];
    memset(encoded, 0, sizeof(encoded));
    ULONG encLen = DnsBase32Encode(src, srcLen, encoded, sizeof(encoded));
    if (encLen == 0)
        return FALSE;

    ULONG written = 0;
    ULONG i = 0;
    while (i < encLen) {
        ULONG chunk = labelSize;
        if (chunk > encLen - i)
            chunk = encLen - i;
        if (written + chunk + 1 >= outSize)
            return FALSE;
        memcpy(out + written, encoded + i, chunk);
        written += chunk;
        i += chunk;
        if (i < encLen) {
            out[written++] = '.';
        }
    }
    if (written >= outSize)
        return FALSE;
    out[written] = '\0';
    return TRUE;
}

int DnsBase64Decode(const CHAR* src, int srcLen, BYTE* dst, int dstMax)
{
    if (!src || !dst || srcLen <= 0 || dstMax <= 0)
        return 0;

    int j = 0;
    int val = 0;
    int valb = -8;

    for (int i = 0; i < srcLen; i++) {
        unsigned char c = src[i];
        if (c > 127) continue;
        int d = BASE64_DECODE_TABLE[c];
        if (d == -1) continue;

        val = (val << 6) | d;
        valb += 6;
        if (valb >= 0) {
            if (j < dstMax) dst[j++] = (BYTE)((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    return j;
}
