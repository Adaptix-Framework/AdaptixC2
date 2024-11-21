#pragma once

#include <windows.h>
#include <iphlpapi.h>
#include <psapi.h>
#include "ntdll.h"


extern void* __cdecl memset(void*, int, size_t);
extern void* __cdecl memcpy(void*, const void*, size_t);

#define TEB NtCurrentTeb()
#define DECL_API(x) decltype(x) * x

struct WINAPIFUNC
{
	// kernel32
	DECL_API(CopyFile);
	DECL_API(CreateFile);
	DECL_API(GetACP);
	DECL_API(GetComputerNameEx);
	DECL_API(GetCurrentDirectory);
	DECL_API(GetFileSize);
	DECL_API(GetFullPathName);
	DECL_API(GetOEMCP);
	DECL_API(GetModuleBaseName);
	DECL_API(GetModuleHandleW);
	DECL_API(GetProcAddress);
	DECL_API(GetTickCount);
	DECL_API(GetTokenInformation);
	DECL_API(GetTimeZoneInformation);
	DECL_API(GetUserName);
	DECL_API(HeapAlloc);
	DECL_API(HeapCreate);
	DECL_API(HeapDestroy);
	DECL_API(HeapReAlloc);
	DECL_API(HeapFree);
	DECL_API(LocalAlloc);
	DECL_API(LocalFree);
	DECL_API(LocalReAlloc);
	DECL_API(ReadFile);
	DECL_API(SetCurrentDirectory);
	DECL_API(WriteFile);
	
	// iphlpapi
	DECL_API(GetAdaptersInfo);
};

struct NTAPIFUNC
{
	DECL_API(NtClose);
	DECL_API(NtQuerySystemInformation);
	DECL_API(NtOpenProcessToken);
	DECL_API(RtlGetVersion);
	DECL_API(RtlIpv4StringToAddressA);
	DECL_API(RtlRandomEx);
};

extern WINAPIFUNC* ApiWin;
extern NTAPIFUNC*  ApiNt;

BOOL ApiLoad();
