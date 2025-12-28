#include "main.h"
#include "utils.h"
#include "DnsCompression.h"

#define MINIZ_NO_STDIO
#include "miniz.h"

BOOL DeflateCompress(const BYTE* inBuf, ULONG inLen, BYTE** outBuf, ULONG* outLen)
{
    if (!inBuf || !outBuf || !outLen || inLen == 0)
        return FALSE;

    mz_ulong srcLen = (mz_ulong)inLen;
    mz_ulong bound  = compressBound(srcLen);
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

BOOL DeflateDecompress(const BYTE* inBuf, ULONG inLen, BYTE** outBuf, ULONG expectedLen)
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
