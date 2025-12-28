#ifndef DNS_UTILS_H
#define DNS_UTILS_H

#include <windows.h>
#include "ApiDefines.h"

void DnsToHex32(ULONG value, CHAR out[9]);

ULONG DnsBase32Encode(const BYTE* src, ULONG srcLen, CHAR* dst, ULONG dstSize);

ULONG DnsBase32Decode(const CHAR* src, ULONG srcLen, BYTE* dst, ULONG dstSize);

void DnsBuildQName(const CHAR* sid, const CHAR* op, ULONG seq, ULONG idx, 
                   const CHAR* dataLabel, const CHAR* domain, CHAR* out, ULONG outSize);

int DnsEncodeName(const CHAR* host, BYTE* buf, int bufSize);

BOOL DnsBuildDataLabels(const BYTE* src, ULONG srcLen, ULONG labelSize, 
                        CHAR* out, ULONG outSize);

int DnsBase64Decode(const CHAR* src, int srcLen, BYTE* dst, int dstMax);

#endif
