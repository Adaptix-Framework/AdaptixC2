package main

import (
	"crypto/rand"
	"encoding/binary"
	"fmt"
	"strings"
)

// cryptoRandUint32 returns a cryptographically random uint32.
func cryptoRandUint32() uint32 {
	var buf [4]byte
	_, _ = rand.Read(buf[:])
	return binary.LittleEndian.Uint32(buf[:])
}

// djb2a computes a case-insensitive DJB2 hash for ASCII function names.
func djb2a(seed uint32, s string) uint32 {
	h := seed
	for _, c := range strings.ToLower(s) {
		h = ((h << 5) + h) + uint32(c)
	}
	return h
}

// djb2w computes a case-insensitive DJB2 hash for module names (UTF-16LE on Windows).
// For ASCII-only DLL names, iterating WCHAR* values produces the same result as
// iterating char* values, so this is equivalent to djb2a.
func djb2w(seed uint32, s string) uint32 {
	return djb2a(seed, s)
}

// hashEntry holds a define name and the string to hash.
type hashEntry struct {
	define string
	name   string
}

// libEntry holds a library define name and the DLL name.
type libEntry struct {
	define  string
	dllName string
}

var hashLibs = []libEntry{
	{"HASH_LIB_NTDLL", "ntdll.dll"},
	{"HASH_LIB_KERNEL32", "kernel32.dll"},
	{"HASH_LIB_KERNELBASE", "kernelbase.dll"},
	{"HASH_LIB_IPHLPAPI", "iphlpapi.dll"},
	{"HASH_LIB_ADVAPI32", "advapi32.dll"},
	{"HASH_LIB_MSVCRT", "msvcrt.dll"},
	{"HASH_LIB_WS2_32", "ws2_32.dll"},
	{"HASH_LIB_WININET", "wininet.dll"},
	{"HASH_LIB_USER32", "user32.dll"},
	{"HASH_LIB_AMSI", "amsi.dll"},
}

// hashFunctions is organized by sections matching the original hashes.py + extra hashes from ApiDefines.h
var hashFuncSections = []struct {
	comment string
	funcs   []hashEntry
}{
	{"//ntdll", []hashEntry{
		{"HASH_FUNC_NTCLOSE", "NtClose"},
		{"HASH_FUNC_NTCONTINUE", "NtContinue"},
		{"HASH_FUNC_NTFREEVIRTUALMEMORY", "NtFreeVirtualMemory"},
		{"HASH_FUNC_NTQUERYINFORMATIONPROCESS", "NtQueryInformationProcess"},
		{"HASH_FUNC_NTQUERYSYSTEMINFORMATION", "NtQuerySystemInformation"},
		{"HASH_FUNC_NTOPENPROCESS", "NtOpenProcess"},
		{"HASH_FUNC_NTOPENPROCESSTOKEN", "NtOpenProcessToken"},
		{"HASH_FUNC_NTOPENTHREADTOKEN", "NtOpenThreadToken"},
		{"HASH_FUNC_NTTERMINATETHREAD", "NtTerminateThread"},
		{"HASH_FUNC_NTTERMINATEPROCESS", "NtTerminateProcess"},
		{"HASH_FUNC_RTLGETVERSION", "RtlGetVersion"},
		{"HASH_FUNC_RTLEXITUSERTHREAD", "RtlExitUserThread"},
		{"HASH_FUNC_RTLEXITUSERPROCESS", "RtlExitUserProcess"},
		{"HASH_FUNC_RTLIPV4STRINGTOADDRESSA", "RtlIpv4StringToAddressA"},
		{"HASH_FUNC_RTLRANDOMEX", "RtlRandomEx"},
		{"HASH_FUNC_RTLNTSTATUSTODOSERROR", "RtlNtStatusToDosError"},
		{"HASH_FUNC_NTFLUSHINSTRUCTIONCACHE", "NtFlushInstructionCache"},
		{"HASH_FUNC_RTLINITIALIZECRITICALSECTION", "RtlInitializeCriticalSection"},
		{"HASH_FUNC_RTLDELETECRITICALSECTION", "RtlDeleteCriticalSection"},
		{"HASH_FUNC_NTCREATEEVENT", "NtCreateEvent"},
		{"HASH_FUNC_NTWAITFORSINGLEOBJECT", "NtWaitForSingleObject"},
		{"HASH_FUNC_NTSIGNALANDWAITFORSINGLEOBJECT", "NtSignalAndWaitForSingleObject"},
		{"HASH_FUNC_NTQUEUEAPCTHREAD", "NtQueueApcThread"},
		{"HASH_FUNC_NTALERTRESUMETHREAD", "NtAlertResumeThread"},
		{"HASH_FUNC_NTPROTECTVIRTUALMEMORY", "NtProtectVirtualMemory"},
		{"HASH_FUNC_NTCREATETHREADEX", "NtCreateThreadEx"},
		{"HASH_FUNC_NTSETEVENT", "NtSetEvent"},
		{"HASH_FUNC_NTQUERYVIRTUALMEMORY", "NtQueryVirtualMemory"},
		{"HASH_FUNC_NTCREATESECTION", "NtCreateSection"},
		{"HASH_FUNC_NTMAPVIEWOFSECTION", "NtMapViewOfSection"},
		{"HASH_FUNC_NTUNMAPVIEWOFSECTION", "NtUnmapViewOfSection"},
		{"HASH_FUNC_ETWEVENTWRITE", "EtwEventWrite"},
		{"HASH_FUNC_NTALLOCATEVIRTUALMEMORY", "NtAllocateVirtualMemory"},
		{"HASH_FUNC_NTOPENFILE", "NtOpenFile"},
		{"HASH_FUNC_NTREADFILE", "NtReadFile"},
		{"HASH_FUNC_RTLADDVECTOREDEXCEPTIONHANDLER", "RtlAddVectoredExceptionHandler"},
		{"HASH_FUNC_RTLREMOVEVECTOREDEXCEPTIONHANDLER", "RtlRemoveVectoredExceptionHandler"},
		{"HASH_FUNC_RTLCREATEHEAP", "RtlCreateHeap"},
		{"HASH_FUNC_RTLALLOCATEHEAP", "RtlAllocateHeap"},
		{"HASH_FUNC_RTLFREEHEAP", "RtlFreeHeap"},
		{"HASH_FUNC_RTLDESTROYHEAP", "RtlDestroyHeap"},
	}},
	{"// Shinkiro Zw* (SSN sort by address)", []hashEntry{
		{"HASH_FUNC_ZWCLOSE", "ZwClose"},
		{"HASH_FUNC_ZWCONTINUE", "ZwContinue"},
		{"HASH_FUNC_ZWFREEVIRTUALMEMORY", "ZwFreeVirtualMemory"},
		{"HASH_FUNC_ZWQUERYINFORMATIONPROCESS", "ZwQueryInformationProcess"},
		{"HASH_FUNC_ZWQUERYSYSTEMINFORMATION", "ZwQuerySystemInformation"},
		{"HASH_FUNC_ZWOPENPROCESS", "ZwOpenProcess"},
		{"HASH_FUNC_ZWOPENPROCESSTOKEN", "ZwOpenProcessToken"},
		{"HASH_FUNC_ZWOPENTHREADTOKEN", "ZwOpenThreadToken"},
		{"HASH_FUNC_ZWTERMINATETHREAD", "ZwTerminateThread"},
		{"HASH_FUNC_ZWTERMINATEPROCESS", "ZwTerminateProcess"},
		{"HASH_FUNC_ZWCREATEEVENT", "ZwCreateEvent"},
		{"HASH_FUNC_ZWWAITFORSINGLEOBJECT", "ZwWaitForSingleObject"},
		{"HASH_FUNC_ZWSIGNALANDWAITFORSINGLEOBJECT", "ZwSignalAndWaitForSingleObject"},
		{"HASH_FUNC_ZWQUEUEAPCTHREAD", "ZwQueueApcThread"},
		{"HASH_FUNC_ZWALERTRESUMETHREAD", "ZwAlertResumeThread"},
		{"HASH_FUNC_ZWPROTECTVIRTUALMEMORY", "ZwProtectVirtualMemory"},
		{"HASH_FUNC_ZWCREATETHREADEX", "ZwCreateThreadEx"},
		{"HASH_FUNC_ZWSETEVENT", "ZwSetEvent"},
		{"HASH_FUNC_ZWCREATESECTION", "ZwCreateSection"},
		{"HASH_FUNC_ZWMAPVIEWOFSECTION", "ZwMapViewOfSection"},
		{"HASH_FUNC_ZWUNMAPVIEWOFSECTION", "ZwUnmapViewOfSection"},
		{"HASH_FUNC_ZWALLOCATEVIRTUALMEMORY", "ZwAllocateVirtualMemory"},
		{"HASH_FUNC_ZWOPENFILE", "ZwOpenFile"},
		{"HASH_FUNC_ZWREADFILE", "ZwReadFile"},
		{"HASH_FUNC_ZWFLUSHINSTRUCTIONCACHE", "ZwFlushInstructionCache"},
		{"HASH_FUNC_ZWQUERYVIRTUALMEMORY", "ZwQueryVirtualMemory"},
	}},
	{"// ThreadStack Spoofing (TsInit frame targets)", []hashEntry{
		{"HASH_TS_WAITFORSINGLEOBJECTEX", "WaitForSingleObjectEx"},
		{"HASH_TS_BASETHREADINITTHUNK", "BaseThreadInitThunk"},
		{"HASH_TS_RTLUSERTHREADSTART", "RtlUserThreadStart"},
		{"HASH_TS_NTWAITFORSINGLEOBJECT", "NtWaitForSingleObject"},
	}},
	{"// Ekko Sleep Obfuscation (timer queue)", []hashEntry{
		{"HASH_FUNC_RTLCREATETIMERQUEUE", "RtlCreateTimerQueue"},
		{"HASH_FUNC_RTLCREATETIMER", "RtlCreateTimer"},
		{"HASH_FUNC_RTLDELETETIMERQUEUE", "RtlDeleteTimerQueue"},
	}},
	{"// Ekko Sleep Obfuscation (kernel32 ROP chain)", []hashEntry{
		{"HASH_FUNC_VIRTUALPROTECT", "VirtualProtect"},
		{"HASH_FUNC_WAITFORSINGLEOBJECTEX", "WaitForSingleObjectEx"},
		{"HASH_FUNC_SETEVENT", "SetEvent"},
	}},
	{"// amsi", []hashEntry{
		{"HASH_FUNC_AMSISCANBUFFER", "AmsiScanBuffer"},
	}},
	{"//kernel32", []hashEntry{
		{"HASH_FUNC_CONNECTNAMEDPIPE", "ConnectNamedPipe"},
		{"HASH_FUNC_COPYFILEA", "CopyFileA"},
		{"HASH_FUNC_CREATEDIRECTORYA", "CreateDirectoryA"},
		{"HASH_FUNC_CREATEFILEA", "CreateFileA"},
		{"HASH_FUNC_CREATENAMEDPIPEA", "CreateNamedPipeA"},
		{"HASH_FUNC_CREATEPIPE", "CreatePipe"},
		{"HASH_FUNC_CREATEPROCESSA", "CreateProcessA"},
		{"HASH_FUNC_DELETEFILEA", "DeleteFileA"},
		{"HASH_FUNC_DISCONNECTNAMEDPIPE", "DisconnectNamedPipe"},
		{"HASH_FUNC_FINDCLOSE", "FindClose"},
		{"HASH_FUNC_FINDFIRSTFILEA", "FindFirstFileA"},
		{"HASH_FUNC_FINDNEXTFILEA", "FindNextFileA"},
		{"HASH_FUNC_FREELIBRARY", "FreeLibrary"},
		{"HASH_FUNC_FLUSHFILEBUFFERS", "FlushFileBuffers"},
		{"HASH_FUNC_GETACP", "GetACP"},
		{"HASH_FUNC_GETCOMPUTERNAMEEXA", "GetComputerNameExA"},
		{"HASH_FUNC_GETCURRENTDIRECTORYA", "GetCurrentDirectoryA"},
		{"HASH_FUNC_GETDRIVETYPEA", "GetDriveTypeA"},
		{"HASH_FUNC_GETEXITCODEPROCESS", "GetExitCodeProcess"},
		{"HASH_FUNC_GETEXITCODETHREAD", "GetExitCodeThread"},
		{"HASH_FUNC_GETFILESIZE", "GetFileSize"},
		{"HASH_FUNC_GETFILEATTRIBUTESA", "GetFileAttributesA"},
		{"HASH_FUNC_GETFULLPATHNAMEA", "GetFullPathNameA"},
		{"HASH_FUNC_GETTHREADCONTEXT", "GetThreadContext"},
		{"HASH_FUNC_GETLASTERROR", "GetLastError"},
		{"HASH_FUNC_GETLOGICALDRIVES", "GetLogicalDrives"},
		{"HASH_FUNC_GETOEMCP", "GetOEMCP"},
		{"HASH_FUNC_K32GETMODULEBASENAMEA", "K32GetModuleBaseNameA"},
		{"HASH_FUNC_GETMODULEBASENAMEA", "GetModuleBaseNameA"},
		{"HASH_FUNC_GETMODULEHANDLEA", "GetModuleHandleA"},
		{"HASH_FUNC_GETPROCADDRESS", "GetProcAddress"},
		{"HASH_FUNC_GETLOCALTIME", "GetLocalTime"},
		{"HASH_FUNC_GETSYSTEMTIMEASFILETIME", "GetSystemTimeAsFileTime"},
		{"HASH_FUNC_GETTICKCOUNT", "GetTickCount"},
		{"HASH_FUNC_GETTIMEZONEINFORMATION", "GetTimeZoneInformation"},
		{"HASH_FUNC_GETUSERNAMEA", "GetUserNameA"},
		{"HASH_FUNC_HEAPALLOC", "HeapAlloc"},
		{"HASH_FUNC_HEAPCREATE", "HeapCreate"},
		{"HASH_FUNC_HEAPDESTROY", "HeapDestroy"},
		{"HASH_FUNC_HEAPREALLOC", "HeapReAlloc"},
		{"HASH_FUNC_HEAPFREE", "HeapFree"},
		{"HASH_FUNC_ISWOW64PROCESS", "IsWow64Process"},
		{"HASH_FUNC_LOADLIBRARYA", "LoadLibraryA"},
		{"HASH_FUNC_LOCALALLOC", "LocalAlloc"},
		{"HASH_FUNC_LOCALFREE", "LocalFree"},
		{"HASH_FUNC_LOCALREALLOC", "LocalReAlloc"},
		{"HASH_FUNC_MOVEFILEA", "MoveFileA"},
		{"HASH_FUNC_MULTIBYTETOWIDECHAR", "MultiByteToWideChar"},
		{"HASH_FUNC_PEEKNAMEDPIPE", "PeekNamedPipe"},
		{"HASH_FUNC_READFILE", "ReadFile"},
		{"HASH_FUNC_REMOVEDIRECTORYA", "RemoveDirectoryA"},
		{"HASH_FUNC_RTLCAPTURECONTEXT", "RtlCaptureContext"},
		{"HASH_FUNC_SETCURRENTDIRECTORYA", "SetCurrentDirectoryA"},
		{"HASH_FUNC_SETNAMEDPIPEHANDLESTATE", "SetNamedPipeHandleState"},
		{"HASH_FUNC_SLEEP", "Sleep"},
		{"HASH_FUNC_VIRTUALALLOC", "VirtualAlloc"},
		{"HASH_FUNC_VIRTUALFREE", "VirtualFree"},
		{"HASH_FUNC_WAITNAMEDPIPEA", "WaitNamedPipeA"},
		{"HASH_FUNC_WIDECHARTOMULTIBYTE", "WideCharToMultiByte"},
		{"HASH_FUNC_WRITEFILE", "WriteFile"},
	}},
	{"// iphlpapi", []hashEntry{
		{"HASH_FUNC_GETADAPTERSINFO", "GetAdaptersInfo"},
	}},
	{"// advapi32", []hashEntry{
		{"HASH_FUNC_ALLOCATEANDINITIALIZESID", "AllocateAndInitializeSid"},
		{"HASH_FUNC_GETTOKENINFORMATION", "GetTokenInformation"},
		{"HASH_FUNC_INITIALIZESECURITYDESCRIPTOR", "InitializeSecurityDescriptor"},
		{"HASH_FUNC_IMPERSONATELOGGEDONUSER", "ImpersonateLoggedOnUser"},
		{"HASH_FUNC_FREESID", "FreeSid"},
		{"HASH_FUNC_LOOKUPACCOUNTSIDA", "LookupAccountSidA"},
		{"HASH_FUNC_REVERTTOSELF", "RevertToSelf"},
		{"HASH_FUNC_SETTHREADTOKEN", "SetThreadToken"},
		{"HASH_FUNC_SETENTRIESINACLA", "SetEntriesInAclA"},
		{"HASH_FUNC_SETSECURITYDESCRIPTORDACL", "SetSecurityDescriptorDacl"},
		{"HASH_FUNC_SYSTEMFUNCTION036", "SystemFunction036"},
		{"HASH_FUNC_SYSTEMFUNCTION032", "SystemFunction032"},
	}},
	{"// msvcrt", []hashEntry{
		{"HASH_FUNC_PRINTF", "printf"},
		{"HASH_FUNC_VSNPRINTF", "vsnprintf"},
		{"HASH_FUNC__SNPRINTF", "_snprintf"},
	}},
	{"// BOF", []hashEntry{
		{"HASH_FUNC_BEACONDATAPARSE", "BeaconDataParse"},
		{"HASH_FUNC_BEACONDATAINT", "BeaconDataInt"},
		{"HASH_FUNC_BEACONDATASHORT", "BeaconDataShort"},
		{"HASH_FUNC_BEACONDATALENGTH", "BeaconDataLength"},
		{"HASH_FUNC_BEACONDATAEXTRACT", "BeaconDataExtract"},
		{"HASH_FUNC_BEACONFORMATALLOC", "BeaconFormatAlloc"},
		{"HASH_FUNC_BEACONFORMATRESET", "BeaconFormatReset"},
		{"HASH_FUNC_BEACONFORMATAPPEND", "BeaconFormatAppend"},
		{"HASH_FUNC_BEACONFORMATPRINTF", "BeaconFormatPrintf"},
		{"HASH_FUNC_BEACONFORMATTOSTRING", "BeaconFormatToString"},
		{"HASH_FUNC_BEACONFORMATFREE", "BeaconFormatFree"},
		{"HASH_FUNC_BEACONFORMATINT", "BeaconFormatInt"},
		{"HASH_FUNC_BEACONOUTPUT", "BeaconOutput"},
		{"HASH_FUNC_BEACONPRINTF", "BeaconPrintf"},
		{"HASH_FUNC_BEACONUSETOKEN", "BeaconUseToken"},
		{"HASH_FUNC_BEACONREVERTTOKEN", "BeaconRevertToken"},
		{"HASH_FUNC_BEACONISADMIN", "BeaconIsAdmin"},
		{"HASH_FUNC_BEACONGETSPAWNTO", "BeaconGetSpawnTo"},
		{"HASH_FUNC_BEACONINJECTPROCESS", "BeaconInjectProcess"},
		{"HASH_FUNC_BEACONINJECTTEMPORARYPROCESS", "BeaconInjectTemporaryProcess"},
		{"HASH_FUNC_BEACONSPAWNTEMPORARYPROCESS", "BeaconSpawnTemporaryProcess"},
		{"HASH_FUNC_BEACONCLEANUPPROCESS", "BeaconCleanupProcess"},
		{"HASH_FUNC_TOWIDECHAR", "toWideChar"},
		{"HASH_FUNC_BEACONINFORMATION", "BeaconInformation"},
		{"HASH_FUNC_BEACONADDVALUE", "BeaconAddValue"},
		{"HASH_FUNC_BEACONGETVALUE", "BeaconGetValue"},
		{"HASH_FUNC_BEACONREMOVEVALUE", "BeaconRemoveValue"},
		// duplicates from kernel32 (BOF API resolution)
		// HASH_FUNC_LOADLIBRARYA, HASH_FUNC_GETPROCADDRESS, HASH_FUNC_GETMODULEHANDLEA, HASH_FUNC_FREELIBRARY already defined above
		{"HASH_FUNC___C_SPECIFIC_HANDLER", "__C_specific_handler"},
		{"HASH_FUNC_AXADDSCREENSHOT", "AxAddScreenshot"},
		{"HASH_FUNC_AXDOWNLOADMEMORY", "AxDownloadMemory"},
		{"HASH_FUNC_BEACONWAKEUP", "BeaconWakeup"},
		{"HASH_FUNC_BEACONGETSTOPJOBEVENT", "BeaconGetStopJobEvent"},
	}},
	{"// wininet", []hashEntry{
		{"HASH_FUNC_INTERNETOPENA", "InternetOpenA"},
		{"HASH_FUNC_INTERNETCONNECTA", "InternetConnectA"},
		{"HASH_FUNC_HTTPOPENREQUESTA", "HttpOpenRequestA"},
		{"HASH_FUNC_HTTPSENDREQUESTA", "HttpSendRequestA"},
		{"HASH_FUNC_INTERNETSETOPTIONA", "InternetSetOptionA"},
		{"HASH_FUNC_INTERNETQUERYOPTIONA", "InternetQueryOptionA"},
		{"HASH_FUNC_HTTPQUERYINFOA", "HttpQueryInfoA"},
		{"HASH_FUNC_INTERNETQUERYDATAAVAILABLE", "InternetQueryDataAvailable"},
		{"HASH_FUNC_INTERNETCLOSEHANDLE", "InternetCloseHandle"},
		{"HASH_FUNC_INTERNETREADFILE", "InternetReadFile"},
	}},
	{"// user32 (keylogger)", []hashEntry{
		{"HASH_FUNC_GETASYNCKEYSTATE", "GetAsyncKeyState"},
		{"HASH_FUNC_GETKEYBOARDSTATE", "GetKeyboardState"},
		{"HASH_FUNC_TOUNICODE", "ToUnicode"},
		{"HASH_FUNC_MAPVIRTUALKEYW", "MapVirtualKeyW"},
		{"HASH_FUNC_GETFOREGROUNDWINDOW", "GetForegroundWindow"},
		{"HASH_FUNC_GETWINDOWTEXTW", "GetWindowTextW"},
		{"HASH_FUNC_GETWINDOWTHREADPROCESSID", "GetWindowThreadProcessId"},
	}},
	{"// ws2_32", []hashEntry{
		{"HASH_FUNC_WSASTARTUP", "WSAStartup"},
		{"HASH_FUNC_WSACLEANUP", "WSACleanup"},
		{"HASH_FUNC_SOCKET", "socket"},
		{"HASH_FUNC_GETHOSTBYNAME", "gethostbyname"},
		{"HASH_FUNC_IOCTLSOCKET", "ioctlsocket"},
		{"HASH_FUNC_CONNECT", "connect"},
		{"HASH_FUNC_SETSOCKOPT", "setsockopt"},
		{"HASH_FUNC_GETSOCKOPT", "getsockopt"},
		{"HASH_FUNC_WSAGETLASTERROR", "WSAGetLastError"},
		{"HASH_FUNC_CLOSESOCKET", "closesocket"},
		{"HASH_FUNC_SELECT", "select"},
		{"HASH_FUNC___WSAFDISSET", "__WSAFDIsSet"},
		{"HASH_FUNC_SHUTDOWN", "shutdown"},
		{"HASH_FUNC_RECV", "recv"},
		{"HASH_FUNC_SEND", "send"},
		{"HASH_FUNC_ACCEPT", "accept"},
		{"HASH_FUNC_BIND", "bind"},
		{"HASH_FUNC_LISTEN", "listen"},
		{"HASH_FUNC_RECVFROM", "recvfrom"},
		{"HASH_FUNC_SENDTO", "sendto"},
	}},
	{"// shinkiro v2: per-syscall Win32 wrapper functions (kernelbase/kernel32/advapi32)", []hashEntry{
		{"HASH_WRAPPER_CLOSEHANDLE", "CloseHandle"},
		{"HASH_WRAPPER_OPENPROCESS", "OpenProcess"},
		{"HASH_WRAPPER_OPENPROCESSTOKEN", "OpenProcessToken"},
		{"HASH_WRAPPER_OPENTHREADTOKEN", "OpenThreadToken"},
		{"HASH_WRAPPER_TERMINATETHREAD", "TerminateThread"},
		{"HASH_WRAPPER_TERMINATEPROCESS", "TerminateProcess"},
		{"HASH_WRAPPER_CREATEEVENTW", "CreateEventW"},
		{"HASH_WRAPPER_WAITFORSINGLEOBJECT", "WaitForSingleObject"},
		{"HASH_WRAPPER_SIGNALOBJECTANDWAIT", "SignalObjectAndWait"},
		{"HASH_WRAPPER_QUEUEUSERAPC", "QueueUserAPC"},
		{"HASH_WRAPPER_CREATEREMOTETHREAD", "CreateRemoteThread"},
		{"HASH_WRAPPER_CREATEFILEMAPPINGW", "CreateFileMappingW"},
		{"HASH_WRAPPER_MAPVIEWOFFILE", "MapViewOfFile"},
		{"HASH_WRAPPER_UNMAPVIEWOFFILE", "UnmapViewOfFile"},
		{"HASH_WRAPPER_CREATEFILEW", "CreateFileW"},
		{"HASH_FUNC_FLUSHINSTRUCTIONCACHE", "FlushInstructionCache"},
	}},
}

// generateApiDefines produces the full ApiDefines.h content with the given DJB2 seed.
func generateApiDefines(seed uint32) string {
	var b strings.Builder
	b.WriteString("#pragma once\n")

	// Library hashes (DJB2W)
	for _, lib := range hashLibs {
		h := djb2w(seed, lib.dllName)
		pad := 35 - len(lib.define)
		if pad < 1 {
			pad = 1
		}
		b.WriteString(fmt.Sprintf("#define %s%s0x%x\n", lib.define, strings.Repeat(" ", pad), h))
	}
	b.WriteString("\n")

	// Function hashes (DJB2A) organized by section
	for _, section := range hashFuncSections {
		b.WriteString(section.comment + "\n")
		for _, entry := range section.funcs {
			h := djb2a(seed, entry.name)
			pad := 35 - len(entry.define)
			if pad < 1 {
				pad = 1
			}
			b.WriteString(fmt.Sprintf("#define %s%s0x%x\n", entry.define, strings.Repeat(" ", pad), h))
		}
		b.WriteString("\n")
	}

	return b.String()
}

// StubHashes holds the pre-computed DJB2 hashes for the reflective loader stub.
// These are passed to nasm as -D defines so each payload has unique hash constants.
type StubHashes struct {
	ModNtdll              uint32
	ModKernel32           uint32
	NtCreateSection       uint32
	NtMapViewOfSection    uint32
	NtProtectVirtualMem   uint32
	NtClose               uint32
	LoadLibraryA          uint32
	GetProcAddress        uint32
	FlushInstructionCache uint32
	FreeLibrary           uint32
	LoadLibraryExA        uint32
}

// computeStubHashes computes all DJB2 hashes needed by stub_rdi.x64.asm
// using the given random seed. Module names use djb2w (wchar), export
// names use djb2a (ASCII) — both case-insensitive.
func computeStubHashes(seed uint32) StubHashes {
	return StubHashes{
		ModNtdll:              djb2w(seed, "ntdll.dll"),
		ModKernel32:           djb2w(seed, "kernel32.dll"),
		NtCreateSection:       djb2a(seed, "NtCreateSection"),
		NtMapViewOfSection:    djb2a(seed, "NtMapViewOfSection"),
		NtProtectVirtualMem:   djb2a(seed, "NtProtectVirtualMemory"),
		NtClose:               djb2a(seed, "NtClose"),
		LoadLibraryA:          djb2a(seed, "LoadLibraryA"),
		GetProcAddress:        djb2a(seed, "GetProcAddress"),
		FlushInstructionCache: djb2a(seed, "FlushInstructionCache"),
		FreeLibrary:           djb2a(seed, "FreeLibrary"),
		LoadLibraryExA:        djb2a(seed, "LoadLibraryExA"),
	}
}
