#pragma once

/*
 * Beacon Object Files (BOF)
 * -------------------------
 * A Beacon Object File is a light-weight post exploitation tool that runs
 * with Beacon's inline-execute command.
 *
 * Additional BOF resources are available here:
 *   - https://github.com/Cobalt-Strike/bof_template
 *
 * Cobalt Strike 4.x
 * ChangeLog:
 *    1/25/2022: updated for 4.5
 *    7/18/2023: Added BeaconInformation API for 4.9
 *    7/31/2023: Added Key/Value store APIs for 4.9
 *                  BeaconAddValue, BeaconGetValue, and BeaconRemoveValue
 *    8/31/2023: Added Data store APIs for 4.9
 *                  BeaconDataStoreGetItem, BeaconDataStoreProtectItem,
 *                  BeaconDataStoreUnprotectItem, and BeaconDataStoreMaxEntries
 *    9/01/2023: Added BeaconGetCustomUserData API for 4.9
 *    3/21/2024: Updated BeaconInformation API for 4.10 to return a BOOL
 *               Updated the BEACON_INFO data structure to add new parameters
 *    4/19/2024: Added BeaconGetSyscallInformation API for 4.10
 *    4/25/2024: Added APIs to call Beacon's system call implementation
 */

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

	/* data API */
	typedef struct {
		char* original; /* the original buffer [so we can free it] */
		char* buffer;   /* current pointer into our buffer */
		int   length;   /* remaining length of data */
		int   size;     /* total size of this buffer */
	} datap;

	void    BeaconDataParse(datap* parser, char* buffer, int size);
	char*	BeaconDataPtr(datap* parser, int size);
	int     BeaconDataInt(datap* parser);
	short   BeaconDataShort(datap* parser);
	int     BeaconDataLength(datap* parser);
	char*   BeaconDataExtract(datap* parser, int* size);

	/* format API */
	typedef struct {
		char* original; /* the original buffer [so we can free it] */
		char* buffer;   /* current pointer into our buffer */
		int   length;   /* remaining length of data */
		int   size;     /* total size of this buffer */
	} formatp;

	void    BeaconFormatAlloc(formatp* format, int maxsz);
	void    BeaconFormatReset(formatp* format);
	void    BeaconFormatAppend(formatp* format, const char* text, int len);
	void    BeaconFormatPrintf(formatp* format, const char* fmt, ...);
	char*   BeaconFormatToString(formatp* format, int* size);
	void    BeaconFormatFree(formatp* format);
	void    BeaconFormatInt(formatp* format, int value);

	/* Output Functions */
#define CALLBACK_OUTPUT      0x0
#define CALLBACK_OUTPUT_OEM  0x1e
#define CALLBACK_OUTPUT_UTF8 0x20
#define CALLBACK_ERROR       0x0d
#define CALLBACK_CUSTOM      0x1000
#define CALLBACK_CUSTOM_LAST 0x13ff

	void   BeaconOutput(int type, const char* data, int len);
	void   BeaconPrintf(int type, const char* fmt, ...);

	/* Token Functions */
	BOOL   BeaconUseToken(HANDLE token);
	void   BeaconRevertToken();
	BOOL   BeaconIsAdmin();

	/* Spawn+Inject Functions */
	void   BeaconGetSpawnTo(BOOL x86, char* buffer, int length);
	void   BeaconInjectProcess(HANDLE hProc, int pid, char* payload, int p_len, int p_offset, char* arg, int a_len);
	void   BeaconInjectTemporaryProcess(PROCESS_INFORMATION* pInfo, char* payload, int p_len, int p_offset, char* arg, int a_len);
	BOOL   BeaconSpawnTemporaryProcess(BOOL x86, BOOL ignoreToken, STARTUPINFO* si, PROCESS_INFORMATION* pInfo);
	void   BeaconCleanupProcess(PROCESS_INFORMATION* pInfo);

	/* Utility Functions */
	BOOL   toWideChar(char* src, wchar_t* dst, int max);

	/* Beacon Information */
	/*
	 *  ptr  - pointer to the base address of the allocated memory.
	 *  size - the number of bytes allocated for the ptr.
	 */
	typedef struct {
		char* ptr;
		size_t size;
	} HEAP_RECORD;
#define MASK_SIZE 13

	/* Information the user can set in the USER_DATA via a UDRL */
	typedef enum {
		PURPOSE_EMPTY,
		PURPOSE_GENERIC_BUFFER,
		PURPOSE_BEACON_MEMORY,
		PURPOSE_SLEEPMASK_MEMORY,
		PURPOSE_BOF_MEMORY,
		PURPOSE_USER_DEFINED_MEMORY = 1000
	} ALLOCATED_MEMORY_PURPOSE;

	typedef enum {
		LABEL_EMPTY,
		LABEL_BUFFER,
		LABEL_PEHEADER,
		LABEL_TEXT,
		LABEL_RDATA,
		LABEL_DATA,
		LABEL_PDATA,
		LABEL_RELOC,
		LABEL_USER_DEFINED = 1000
	} ALLOCATED_MEMORY_LABEL;

	typedef enum {
		METHOD_UNKNOWN,
		METHOD_VIRTUALALLOC,
		METHOD_HEAPALLOC,
		METHOD_MODULESTOMP,
		METHOD_NTMAPVIEW,
		METHOD_USER_DEFINED = 1000,
	} ALLOCATED_MEMORY_ALLOCATION_METHOD;

	/**
	* This structure allows the user to provide additional information
	* about the allocated heap for cleanup. It is mandatory to provide
	* the HeapHandle but the DestroyHeap Boolean can be used to indicate
	* whether the clean up code should destroy the heap or simply free the pages.
	* This is useful in situations where a loader allocates memory in the
	* processes current heap.
	*/
	typedef struct _HEAPALLOC_INFO {
		PVOID HeapHandle;
		BOOL  DestroyHeap;
	} HEAPALLOC_INFO, * PHEAPALLOC_INFO;

	typedef struct _MODULESTOMP_INFO {
		HMODULE ModuleHandle;
	} MODULESTOMP_INFO, * PMODULESTOMP_INFO;

	typedef union _ALLOCATED_MEMORY_ADDITIONAL_CLEANUP_INFORMATION {
		HEAPALLOC_INFO HeapAllocInfo;
		MODULESTOMP_INFO ModuleStompInfo;
		PVOID Custom;
	} ALLOCATED_MEMORY_ADDITIONAL_CLEANUP_INFORMATION, * PALLOCATED_MEMORY_ADDITIONAL_CLEANUP_INFORMATION;

	typedef struct _ALLOCATED_MEMORY_CLEANUP_INFORMATION {
		BOOL Cleanup;
		ALLOCATED_MEMORY_ALLOCATION_METHOD AllocationMethod;
		ALLOCATED_MEMORY_ADDITIONAL_CLEANUP_INFORMATION AdditionalCleanupInformation;
	} ALLOCATED_MEMORY_CLEANUP_INFORMATION, * PALLOCATED_MEMORY_CLEANUP_INFORMATION;

	typedef struct _ALLOCATED_MEMORY_SECTION {
		ALLOCATED_MEMORY_LABEL Label; // A label to simplify Sleepmask development
		PVOID  BaseAddress;           // Pointer to virtual address of section
		SIZE_T VirtualSize;           // Virtual size of the section
		DWORD  CurrentProtect;        // Current memory protection of the section
		DWORD  PreviousProtect;       // The previous memory protection of the section (prior to masking/unmasking)
		BOOL   MaskSection;           // A boolean to indicate whether the section should be masked
	} ALLOCATED_MEMORY_SECTION, * PALLOCATED_MEMORY_SECTION;

	typedef struct _ALLOCATED_MEMORY_REGION {
		ALLOCATED_MEMORY_PURPOSE Purpose;      // A label to indicate the purpose of the allocated memory
		PVOID  AllocationBase;                 // The base address of the allocated memory block
		SIZE_T RegionSize;                     // The size of the allocated memory block
		DWORD Type;                            // The type of memory allocated
		ALLOCATED_MEMORY_SECTION Sections[8];  // An array of section information structures
		ALLOCATED_MEMORY_CLEANUP_INFORMATION CleanupInformation; // Information required to cleanup the allocation
	} ALLOCATED_MEMORY_REGION, * PALLOCATED_MEMORY_REGION;

	typedef struct {
		ALLOCATED_MEMORY_REGION AllocatedMemoryRegions[6];
	} ALLOCATED_MEMORY, * PALLOCATED_MEMORY;

	/*
	 *  version               - The version of the beacon dll was added for release 4.10
	 *                          version format: 0xMMmmPP, where MM = Major, mm = Minor, and PP = Patch
	 *                          e.g. 0x040900 -> CS 4.9
	 *                               0x041000 -> CS 4.10
	 *
	 *  sleep_mask_ptr        - pointer to the sleep mask base address
	 *  sleep_mask_text_size  - the sleep mask text section size
	 *  sleep_mask_total_size - the sleep mask total memory size
	 *
	 *  beacon_ptr   - pointer to beacon's base address
	 *                 The stage.obfuscate flag affects this value when using CS default loader.
	 *                    true:  beacon_ptr = allocated_buffer - 0x1000 (Not a valid address)
	 *                    false: beacon_ptr = allocated_buffer (A valid address)
	 *                 For a UDRL the beacon_ptr will be set to the 1st argument to DllMain
	 *                 when the 2nd argument is set to DLL_PROCESS_ATTACH.
	 *  heap_records - list of memory addresses on the heap beacon wants to mask.
	 *                 The list is terminated by the HEAP_RECORD.ptr set to NULL.
	 *  mask         - the mask that beacon randomly generated to apply
	 *
	 *  Added in version 4.10
	 *  allocatedMemory - An ALLOCATED_MEMORY structure that can be set in the USER_DATA
	 *                     via a UDRL.
	 */
	typedef struct {
		unsigned int version;
		char* sleep_mask_ptr;
		DWORD   sleep_mask_text_size;
		DWORD   sleep_mask_total_size;

		char* beacon_ptr;
		HEAP_RECORD* heap_records;
		char    mask[MASK_SIZE];

		ALLOCATED_MEMORY allocatedMemory;
	} BEACON_INFO, * PBEACON_INFO;

	BOOL   BeaconInformation(PBEACON_INFO info);

	/* Key/Value store functions
	 *    These functions are used to associate a key to a memory address and save
	 *    that information into beacon.  These memory addresses can then be
	 *    retrieved in a subsequent execution of a BOF.
	 *
	 *    key - the key will be converted to a hash which is used to locate the
	 *          memory address.
	 *
	 *    ptr - a memory address to save.
	 *
	 * Considerations:
	 *    - The contents at the memory address is not masked by beacon.
	 *    - The contents at the memory address is not released by beacon.
	 *
	 */
	BOOL  BeaconAddValue(const char* key, void* ptr);
	void* BeaconGetValue(const char* key);
	BOOL  BeaconRemoveValue(const char* key);

	/* Beacon Data Store functions
	 *    These functions are used to access items in Beacon's Data Store.
	 *    BeaconDataStoreGetItem returns NULL if the index does not exist.
	 *
	 *    The contents are masked by default, and BOFs must unprotect the entry
	 *    before accessing the data buffer. BOFs must also protect the entry
	 *    after the data is not used anymore.
	 *
	 */

#define DATA_STORE_TYPE_EMPTY 0
#define DATA_STORE_TYPE_GENERAL_FILE 1

	typedef struct {
		int type;
		DWORD64 hash;
		BOOL masked;
		char* buffer;
		size_t length;
	} DATA_STORE_OBJECT, * PDATA_STORE_OBJECT;

	PDATA_STORE_OBJECT BeaconDataStoreGetItem(size_t index);
	void BeaconDataStoreProtectItem(size_t index);
	void BeaconDataStoreUnprotectItem(size_t index);
	ULONG BeaconDataStoreMaxEntries();

	/* Beacon User Data functions */
	char* BeaconGetCustomUserData();

	/* Beacon System call */
	/* Syscalls API */
	typedef struct
	{
		PVOID fnAddr;
		PVOID jmpAddr;
		DWORD sysnum;
	} SYSCALL_API_ENTRY, * PSYSCALL_API_ENTRY;

	typedef struct
	{
		SYSCALL_API_ENTRY ntAllocateVirtualMemory;
		SYSCALL_API_ENTRY ntProtectVirtualMemory;
		SYSCALL_API_ENTRY ntFreeVirtualMemory;
		SYSCALL_API_ENTRY ntGetContextThread;
		SYSCALL_API_ENTRY ntSetContextThread;
		SYSCALL_API_ENTRY ntResumeThread;
		SYSCALL_API_ENTRY ntCreateThreadEx;
		SYSCALL_API_ENTRY ntOpenProcess;
		SYSCALL_API_ENTRY ntOpenThread;
		SYSCALL_API_ENTRY ntClose;
		SYSCALL_API_ENTRY ntCreateSection;
		SYSCALL_API_ENTRY ntMapViewOfSection;
		SYSCALL_API_ENTRY ntUnmapViewOfSection;
		SYSCALL_API_ENTRY ntQueryVirtualMemory;
		SYSCALL_API_ENTRY ntDuplicateObject;
		SYSCALL_API_ENTRY ntReadVirtualMemory;
		SYSCALL_API_ENTRY ntWriteVirtualMemory;
		SYSCALL_API_ENTRY ntReadFile;
		SYSCALL_API_ENTRY ntWriteFile;
		SYSCALL_API_ENTRY ntCreateFile;
	} SYSCALL_API, * PSYSCALL_API;

	/* Additional Run Time Library (RTL) addresses used to support system calls.
	 * If they are not set then system calls that require them will fall back
	 * to the Standard Windows API.
	 *
	 * Required to support the following system calls:
	 *    ntCreateFile
	 */
	typedef struct
	{
		PVOID rtlDosPathNameToNtPathNameUWithStatusAddr;
		PVOID rtlFreeHeapAddr;
		PVOID rtlGetProcessHeapAddr;
	} RTL_API, * PRTL_API;

	typedef struct
	{
		PSYSCALL_API syscalls;
		PRTL_API     rtls;
	} BEACON_SYSCALLS, * PBEACON_SYSCALLS;

	BOOL BeaconGetSyscallInformation(PBEACON_SYSCALLS info, BOOL resolveIfNotInitialized);

	///* Beacon System call functions which will use the current system call method */
	LPVOID BeaconVirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);
	LPVOID BeaconVirtualAllocEx(HANDLE processHandle, LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect);
	BOOL   BeaconVirtualProtect(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect);
	BOOL   BeaconVirtualProtectEx(HANDLE processHandle, LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect);
	BOOL   BeaconVirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType);
	BOOL   BeaconGetThreadContext(HANDLE threadHandle, PCONTEXT threadContext);
	BOOL   BeaconSetThreadContext(HANDLE threadHandle, PCONTEXT threadContext);
	DWORD  BeaconResumeThread(HANDLE threadHandle);
	HANDLE BeaconOpenProcess(DWORD desiredAccess, BOOL inheritHandle, DWORD processId);
	HANDLE BeaconOpenThread(DWORD desiredAccess, BOOL inheritHandle, DWORD threadId);
	BOOL   BeaconCloseHandle(HANDLE object);
	BOOL   BeaconUnmapViewOfFile(LPCVOID baseAddress);
	SIZE_T BeaconVirtualQuery(LPCVOID address, PMEMORY_BASIC_INFORMATION buffer, SIZE_T length);
	BOOL   BeaconDuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle, LPHANDLE lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions);
	BOOL   BeaconReadProcessMemory(HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesRead);
	BOOL   BeaconWriteProcessMemory(HANDLE hProcess, LPVOID lpBaseAddress, LPCVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesWritten);

	/* Beacon User Data
	 *
	 * version format: 0xMMmmPP, where MM = Major, mm = Minor, and PP = Patch
	 * e.g. 0x040900 -> CS 4.9
	 *      0x041000 -> CS 4.10
	*/

#define DLL_BEACON_USER_DATA 0x0d
#define BEACON_USER_DATA_CUSTOM_SIZE 32
	typedef struct
	{
		unsigned int version;
		PSYSCALL_API syscalls;
		char         custom[BEACON_USER_DATA_CUSTOM_SIZE];
		PRTL_API     rtls;
		PALLOCATED_MEMORY allocatedMemory;
	} USER_DATA, * PUSER_DATA;

#ifdef __cplusplus
}
#endif // __cplusplus

/// ADD CUSTOM

HMODULE proxy_LoadLibraryA(LPCSTR lpLibFileName);

HMODULE proxy_GetModuleHandleA(LPCSTR lpModuleName);

FARPROC proxy_GetProcAddress(HMODULE hModule, LPCSTR  lpProcName);

BOOL proxy_FreeLibrary(HMODULE hLibModule);