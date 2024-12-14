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
		ApiWin->CreateDirectoryA = CreateDirectoryA;
		ApiWin->CreateFileA = CreateFileA;
		ApiWin->CreatePipe = CreatePipe;
		ApiWin->CreateProcessA = CreateProcessA;
		ApiWin->DeleteFileA = DeleteFileA;
		ApiWin->GetExitCodeProcess = GetExitCodeProcess;
		ApiWin->GetExitCodeThread = GetExitCodeThread;
		ApiWin->FindClose = FindClose;
		ApiWin->FindFirstFileA = FindFirstFileA;
		ApiWin->FindNextFileA = FindNextFileA;
		ApiWin->GetACP = GetACP;
		ApiWin->GetComputerNameExA = GetComputerNameExA;
		ApiWin->GetCurrentDirectoryA = GetCurrentDirectoryA;
		ApiWin->GetDriveTypeA = GetDriveTypeA;
		ApiWin->GetFileSize = GetFileSize;
		ApiWin->GetFileAttributesA = GetFileAttributesA;
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
		ApiWin->MoveFileA = MoveFileA;
		ApiWin->MultiByteToWideChar = MultiByteToWideChar;
		ApiWin->PeekNamedPipe = PeekNamedPipe;
		ApiWin->ReadFile = ReadFile;
		ApiWin->RemoveDirectoryA = RemoveDirectoryA;
		ApiWin->RtlCaptureContext = RtlCaptureContext;
		ApiWin->SetCurrentDirectoryA = SetCurrentDirectoryA;
		ApiWin->SetNamedPipeHandleState = SetNamedPipeHandleState;
		ApiWin->VirtualAlloc = VirtualAlloc;
		ApiWin->VirtualFree = VirtualFree;
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

		// msvcrt
		HMODULE hMsvcrtModule = LoadLibraryA("msvcrt.dll");
		if (hMsvcrtModule) {
			ApiWin->vsnprintf = (vsnprintf_t) (GetProcAddress(hMsvcrtModule, "vsnprintf"));
		}

	}
	else {
		return FALSE;
	}

	if (ApiNt) {
		HMODULE hNtdllModule = ApiWin->GetModuleHandleW(L"ntdll.dll");
		if ( hNtdllModule ) {
			ApiNt->NtClose                   = (decltype(NtClose)*) ApiWin->GetProcAddress(hNtdllModule, "NtClose");
			ApiNt->NtContinue                = (decltype(NtContinue)*) ApiWin->GetProcAddress(hNtdllModule, "NtContinue");
			ApiNt->NtFreeVirtualMemory       = (decltype(NtFreeVirtualMemory)*) ApiWin->GetProcAddress(hNtdllModule, "NtFreeVirtualMemory");
			ApiNt->NtQueryInformationProcess = (decltype(NtQueryInformationProcess)*) ApiWin->GetProcAddress(hNtdllModule, "NtQueryInformationProcess");
			ApiNt->NtQuerySystemInformation  = (decltype(NtQuerySystemInformation)*) ApiWin->GetProcAddress(hNtdllModule, "NtQuerySystemInformation");
			ApiNt->NtOpenProcess             = (decltype(NtOpenProcess)*) ApiWin->GetProcAddress(hNtdllModule, "NtOpenProcess");
			ApiNt->NtOpenProcessToken        = (decltype(NtOpenProcessToken)*) ApiWin->GetProcAddress(hNtdllModule, "NtOpenProcessToken");
			ApiNt->NtTerminateThread         = (decltype(NtTerminateThread)*) ApiWin->GetProcAddress(hNtdllModule, "NtTerminateThread");
			ApiNt->NtTerminateProcess        = (decltype(NtTerminateProcess)*) ApiWin->GetProcAddress(hNtdllModule, "NtTerminateProcess");
			ApiNt->RtlGetVersion             = (decltype(RtlGetVersion)*) ApiWin->GetProcAddress(hNtdllModule, "RtlGetVersion");
			ApiNt->RtlExitUserThread         = (decltype(RtlExitUserThread)*) ApiWin->GetProcAddress(hNtdllModule, "RtlExitUserThread");
			ApiNt->RtlExitUserProcess        = (decltype(RtlExitUserProcess)*) ApiWin->GetProcAddress(hNtdllModule, "RtlExitUserProcess");
			ApiNt->RtlIpv4StringToAddressA   = (decltype(RtlIpv4StringToAddressA)*) ApiWin->GetProcAddress(hNtdllModule, "RtlIpv4StringToAddressA");
			ApiNt->RtlRandomEx               = (decltype(RtlRandomEx)*) ApiWin->GetProcAddress(hNtdllModule, "RtlRandomEx");
			ApiNt->RtlNtStatusToDosError     = (decltype(RtlNtStatusToDosError)*) ApiWin->GetProcAddress(hNtdllModule, "RtlNtStatusToDosError");
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
