#pragma once

#include "main.h"

BOOL DeflateCompress(const BYTE* inBuf, ULONG inLen, BYTE** outBuf, ULONG* outLen);
BOOL DeflateDecompress(const BYTE* inBuf, ULONG inLen, BYTE** outBuf, ULONG expectedLen);
