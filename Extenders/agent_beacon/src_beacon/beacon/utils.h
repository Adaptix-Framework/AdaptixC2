#pragma once

#include <windows.h>

//////////

LPVOID MemAllocLocal(DWORD bufferSize);

LPVOID MemReallocLocal(LPVOID buffer, DWORD bufferSize);

void MemFreeLocal(LPVOID* buffer, DWORD bufferSize);

//////////

BYTE* ReadDataFromAnonPipe(HANDLE hPipe, ULONG* bufferSize);

BOOL PeekNamedPipeTime(HANDLE hNamedPipe, int waitTime);

int ReadFromPipe(HANDLE hPipe, BYTE* buffer, ULONG bufferSize);

int ReadDataFromPipe(HANDLE hPipe, LPVOID* buffer, ULONG* bufferSize);

BOOL WriteToPipe(HANDLE hPipe, BYTE* buffer, ULONG bufferSize);

BOOL WriteDataToPipe(HANDLE hPipe, BYTE* buffer, ULONG bufferSize);


//////////

WORD _htons(WORD hostshort);

ULONG _inet_addr(const char* ip);

BOOL PeekSocketTime(SOCKET sock, int waitTime);

int ReadFromSocket(SOCKET sock, char* buffer, int bufferSize);

int ReadDataFromSocket(SOCKET sock, LPVOID* buffer, ULONG* bufferSize);

BOOL WriteToSocket(SOCKET sock, BYTE* buffer, ULONG bufferSize);

BOOL WriteDataToSocket(SOCKET sock, BYTE* buffer, ULONG bufferSize);

//////////

ULONG GenerateRandom32();

BYTE GetGmtOffset();

BOOL IsElevate();

ULONG GetInternalIpLong();

CHAR* _GetUserName();

CHAR* _GetHostName();

CHAR* _GetDomainName();

CHAR* _GetProcessName();

HANDLE TokenCurrentHandle();

BOOL TokenToUser(HANDLE hToken, CHAR* username, DWORD* usernameSize, CHAR* domain, DWORD* domainSize, BOOL* elevated);

//////////

CHAR* StrChrA(CHAR* str, CHAR c);

CHAR* StrTokA(CHAR* str, CHAR* delim);

DWORD StrIndexA(CHAR* str, CHAR target);

DWORD StrLenA(CHAR* str);

DWORD StrCmpA(CHAR* str1, CHAR* str2);

DWORD StrNCmpA(CHAR* str1, CHAR* str2, SIZE_T n);

DWORD StrCmpLowA(CHAR* str1, CHAR* str2);

DWORD StrCmpLowW(WCHAR* str1, WCHAR* str2);

ULONG FileTimeToUnixTimestamp(FILETIME ft);

void ConvertUnicodeStringToChar( wchar_t* src, size_t srcSize, char* dst, size_t dstSize);
