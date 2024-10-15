#include "ApiLoader.h"

LPVOID MemAllocLocal(DWORD bufferSize) {
	return ApiWin->LocalAlloc(LPTR, bufferSize);
}

void MemFreeLocal(LPVOID* buffer, DWORD bufferSize) {
    memset((PBYTE)*buffer, 0, bufferSize);
	ApiWin->LocalFree(*buffer);
	*buffer = NULL;
}


//////////

ULONG GenerateRandom32()
{
	ULONG seed = ApiWin->GetTickCount();
	seed = ApiNt->RtlRandomEx(&seed);
	seed = seed % ULONG_MAX;
	return seed;
}

BYTE GetGmtOffset() 
{
	TIME_ZONE_INFORMATION temp;
	ApiWin->GetTimeZoneInformation(&temp);
	BYTE diff = temp.Bias / (-60);
	return diff;
}

BOOL IsElevate() 
{
    BOOL            success   = FALSE;
    BOOL            high      = FALSE;
    HANDLE          hToken    = NULL;
    TOKEN_ELEVATION Elevation = { 0 };
    DWORD           cbSize    = sizeof(TOKEN_ELEVATION);

    NTSTATUS NtStatus = ApiNt->NtOpenProcessToken(NtCurrentProcess(), TOKEN_QUERY, &hToken);
    if (NT_SUCCESS(NtStatus)) {
        success = ApiWin->GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize);
        if (success) 
            high = (BOOL)Elevation.TokenIsElevated;
    }

    if (hToken) {
        ApiNt->NtClose(hToken);
        hToken = NULL;
    }
    return high;
}

ULONG GetInternalIpLong()
{
	ULONG   internalIP      = 0;
	ULONG   success         = 0;
    ULONG   length          = 0;
	IN_ADDR ipAddressObject = { 0 };
	LPCSTR  terminator = NULL;
    ApiWin->GetAdaptersInfo(NULL, &length);
	PIP_ADAPTER_INFO Adapter = (PIP_ADAPTER_INFO)MemAllocLocal(length);
    if ( Adapter ) {

        PIP_ADAPTER_INFO nextAdapter = Adapter;
		success = ApiWin->GetAdaptersInfo(nextAdapter, &length);
		if (success == NO_ERROR ) {
            while (nextAdapter) {

				success = ApiNt->RtlIpv4StringToAddressA(nextAdapter->IpAddressList.IpAddress.String, FALSE, &terminator, &ipAddressObject);
				if ( success == ERROR_SUCCESS && ipAddressObject.S_un.S_addr != 0 ) {
					internalIP = ipAddressObject.S_un.S_addr;
					break;
				}
				nextAdapter = nextAdapter->Next;
            }
        }
        MemFreeLocal((LPVOID*)&Adapter, length);
        nextAdapter = NULL;
    }
    return internalIP;
}

CHAR* _GetUserName()
{
    DWORD length = 0;
    ApiWin->GetUserNameA(NULL, &length);
    CHAR* userName = (CHAR*)MemAllocLocal(length);
    ApiWin->GetUserNameA(userName, &length);
    return userName;
}

CHAR* _GetHostName()
{
    DWORD length = 0;
    ApiWin->GetComputerNameExA(ComputerNameNetBIOS, NULL, &length);
    CHAR* hostName = (CHAR*)MemAllocLocal(length);
    ApiWin->GetComputerNameExA(ComputerNameNetBIOS, hostName, &length);
    return hostName;
}

CHAR* _GetDomainName()
{
    DWORD length = 0;
    ApiWin->GetComputerNameExA(ComputerNameDnsDomain, NULL, &length);
    CHAR* hostName = (CHAR*)MemAllocLocal(length);
    ApiWin->GetComputerNameExA(ComputerNameDnsDomain, hostName, &length);
    return hostName;
}

CHAR* _GetProcessName()
{
    DWORD length = ((PRTL_USER_PROCESS_PARAMETERS)NtCurrentTeb()->ProcessEnvironmentBlock->ProcessParameters)->ImagePathName.Length / 2;
    PWCHAR tmpName = ((PRTL_USER_PROCESS_PARAMETERS)NtCurrentTeb()->ProcessEnvironmentBlock->ProcessParameters)->ImagePathName.Buffer;
    int i = 0;
    for (; tmpName[length - i] != L'\\' && (length - i) >= 0; i++);
    CHAR* processName = (CHAR*)MemAllocLocal(i);
    ApiWin->GetModuleBaseNameA(GetCurrentProcess(), NULL, processName, i);
    return processName;
}

///////////

DWORD StrLenA(CHAR* str)
{
    int i = 0;
    if (str != NULL)
        for (; str[i]; i++);
    return i;
}