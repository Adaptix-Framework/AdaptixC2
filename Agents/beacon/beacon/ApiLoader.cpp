#include "ApiLoader.h"

WINAPIFUNC* ApiWin = NULL;
NTAPIFUNC*  ApiNt = NULL;

#pragma intrinsic(memset)
#pragma function(memset)
void* __cdecl memset(void* Destination, int Value, size_t Size) 
{
	unsigned char* p = (unsigned char*)Destination;
	while (Size > 0) {
		*p = (unsigned char)Value;
		p++;
		Size--;
	}
	return Destination;
}

#pragma intrinsic(memcpy)
#pragma function(memcpy)
extern void* __cdecl memcpy(void* Dst, const void* Src, size_t Size)
{
	unsigned char* p = (unsigned char*) Dst;
	unsigned char* q = (unsigned char*) Src;
	while (Size > 0) {
		*p++ = *q++;
		Size--;
	}
	return Dst;
}

BOOL ApiLoad()
{
	decltype(LocalAlloc)* alloc = LocalAlloc;

	ApiWin = (WINAPIFUNC*) alloc(LPTR, sizeof(WINAPIFUNC));
	ApiNt  = (NTAPIFUNC*)  alloc(LPTR, sizeof(NTAPIFUNC));

	if (ApiWin) {
		// kernel32
		ApiWin->CopyFileA = CopyFileA;
		ApiWin->CreateFileA = CreateFileA;
		ApiWin->FindClose = FindClose;
		ApiWin->FindFirstFileA = FindFirstFileA;
		ApiWin->FindNextFileA = FindNextFileA;
		ApiWin->GetACP = GetACP;
		ApiWin->GetComputerNameExA = GetComputerNameExA;
		ApiWin->GetCurrentDirectoryA = GetCurrentDirectoryA;
		ApiWin->GetDriveTypeA = GetDriveTypeA;
		ApiWin->GetFileSize = GetFileSize;
		ApiWin->GetFullPathNameA = GetFullPathNameA;
		ApiWin->GetLogicalDrives = GetLogicalDrives;
		ApiWin->GetOEMCP = GetOEMCP;
		ApiWin->GetModuleBaseNameA = GetModuleBaseNameA;
		ApiWin->GetModuleHandleW = GetModuleHandleW;
		ApiWin->GetProcAddress = GetProcAddress;
		ApiWin->GetTickCount = GetTickCount;
		//ApiWin->GetTokenInformation = GetTokenInformation;
		ApiWin->GetTimeZoneInformation = GetTimeZoneInformation;
		ApiWin->GetUserNameA = GetUserNameA;
		ApiWin->HeapAlloc = HeapAlloc;
		ApiWin->HeapCreate = HeapCreate;
		ApiWin->HeapDestroy = HeapDestroy;
		ApiWin->HeapReAlloc = HeapReAlloc;
		ApiWin->HeapFree = HeapFree;
		ApiWin->LocalAlloc = alloc;
		ApiWin->LocalFree = LocalFree;
		ApiWin->LocalReAlloc = LocalReAlloc;
		ApiWin->ReadFile = ReadFile;
		ApiWin->SetCurrentDirectoryA = SetCurrentDirectoryA;
		ApiWin->WideCharToMultiByte = WideCharToMultiByte;
		ApiWin->WriteFile = WriteFile;

		// iphlpapi
		HMODULE hIphlpapiModule = LoadLibraryW(L"Iphlpapi.dll");
		if (hIphlpapiModule) {
			ApiWin->GetAdaptersInfo = (decltype(GetAdaptersInfo)*)ApiWin->GetProcAddress(hIphlpapiModule, "GetAdaptersInfo");
		}

		// advapi32
		HMODULE hAdvapiModule = LoadLibraryW(L"Advapi32.dll");
		if (hAdvapiModule) {
			ApiWin->GetTokenInformation = (decltype(GetTokenInformation)*)ApiWin->GetProcAddress(hAdvapiModule, "GetTokenInformation");
			ApiWin->LookupAccountSidA   = (decltype(LookupAccountSidA)*)ApiWin->GetProcAddress(hAdvapiModule, "LookupAccountSidA");
		}
	}
	else {
		return FALSE;
	}

	if (ApiNt) {
		HMODULE hNtdllModule = ApiWin->GetModuleHandleW(L"ntdll.dll");
		if ( hNtdllModule ) {
			ApiNt->NtClose                   = (decltype(NtClose)*) ApiWin->GetProcAddress(hNtdllModule, "NtClose");
			ApiNt->NtQueryInformationProcess = (decltype(NtQueryInformationProcess)*) ApiWin->GetProcAddress(hNtdllModule, "NtQueryInformationProcess");
			ApiNt->NtQuerySystemInformation  = (decltype(NtQuerySystemInformation)*) ApiWin->GetProcAddress(hNtdllModule, "NtQuerySystemInformation");
			ApiNt->NtOpenProcess             = (decltype(NtOpenProcess)*) ApiWin->GetProcAddress(hNtdllModule, "NtOpenProcess");
			ApiNt->NtOpenProcessToken        = (decltype(NtOpenProcessToken)*) ApiWin->GetProcAddress(hNtdllModule, "NtOpenProcessToken");
			ApiNt->RtlGetVersion             = (decltype(RtlGetVersion)*) ApiWin->GetProcAddress(hNtdllModule, "RtlGetVersion");
			ApiNt->RtlIpv4StringToAddressA   = (decltype(RtlIpv4StringToAddressA)*) ApiWin->GetProcAddress(hNtdllModule, "RtlIpv4StringToAddressA");
			ApiNt->RtlRandomEx               = (decltype(RtlRandomEx)*) ApiWin->GetProcAddress(hNtdllModule, "RtlRandomEx");
		}
		else {
			return FALSE;
		}
	}
	else {
		return FALSE;
	}
	return TRUE;
}
