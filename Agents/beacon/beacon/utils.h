#pragma once

#include <windows.h>

LPVOID MemAllocLocal(DWORD bufferSize);

LPVOID MemReallocLocal(LPVOID buffer, DWORD bufferSize);

void MemFreeLocal(LPVOID* buffer, DWORD bufferSize);

BYTE* ReadFromPipe(HANDLE hPipe, ULONG* bufferSize);

ULONG GenerateRandom32();

BYTE GetGmtOffset();

BOOL IsElevate();

ULONG GetInternalIpLong();

CHAR* _GetUserName();

CHAR* _GetHostName();

CHAR* _GetDomainName();

CHAR* _GetProcessName();

DWORD StrLenA(CHAR* str);

ULONG FileTimeToUnixTimestamp(FILETIME ft);

void ConvertUnicodeStringToChar( wchar_t* src, size_t srcSize, char* dst, size_t dstSize);