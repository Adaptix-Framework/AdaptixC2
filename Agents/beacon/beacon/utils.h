#pragma once

#include <windows.h>

LPVOID MemAllocLocal(DWORD bufferSize);

void MemFreeLocal(LPVOID* buffer, DWORD bufferSize);

ULONG GenerateRandom32();

BYTE GetGmtOffset();

BOOL IsElevate();

ULONG GetInternalIpLong();

CHAR* _GetUserName();

CHAR* _GetHostName();

CHAR* _GetDomainName();

CHAR* _GetProcessName();

DWORD StrLenA(CHAR* str);