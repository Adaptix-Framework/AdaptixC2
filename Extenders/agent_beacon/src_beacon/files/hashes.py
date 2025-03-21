#!/usr/bin/env python3
# -*- coding:utf-8 -*-

import sys

def djb2a(input_str: str) -> int:
    input_str = input_str.lower()
    hash_value = 1572
    for char in input_str:
        hash_value = ((hash_value << 5) + hash_value) + ord(char)
    return hash_value & 0xFFFFFFFF

def djb2w(input_str: str) -> int:
    input_str = input_str.lower()
    hash_value = 1572
    for i in range(0, len(input_str), 2):
        val = int.from_bytes(input_str[i:i+2].encode(), 'little')
        hash_value = ((hash_value << 5) + hash_value) + val
    return hash_value & 0xFFFFFFFF

##############################################

libs = """
// ntdll.dll
n\x00t\x00d\x00l\x00l\x00.\x00d\x00l\x00l\x00
// kernel32.dll
k\x00e\x00r\x00n\x00e\x00l\x003\x002\x00.\x00d\x00l\x00l\x00
// iphlpapi.dll
i\x00p\x00h\x00l\x00p\x00a\x00p\x00i\x00.\x00d\x00l\x00l\x00
// advapi32.dll
a\x00d\x00v\x00a\x00p\x00i\x003\x002\x00.\x00d\x00l\x00l\x00
// msvcrt.dll
m\x00s\x00v\x00c\x00r\x00t\x00.\x00d\x00l\x00l\x00
"""

functions = """
//ntdll
NtClose
NtContinue
NtFreeVirtualMemory
NtQueryInformationProcess
NtQuerySystemInformation
NtOpenProcess
NtOpenProcessToken
NtOpenThreadToken
NtTerminateThread
NtTerminateProcess
RtlGetVersion
RtlExitUserThread
RtlExitUserProcess
RtlIpv4StringToAddressA
RtlRandomEx
RtlNtStatusToDosError
NtFlushInstructionCache

//kernel32
ConnectNamedPipe
CopyFileA
CreateDirectoryA
CreateFileA
CreateNamedPipeA
CreatePipe
CreateProcessA
DeleteFileA
DisconnectNamedPipe
FindClose
FindFirstFileA
FindNextFileA
FreeLibrary
FlushFileBuffers
GetACP
GetComputerNameExA
GetCurrentDirectoryA
GetDriveTypeA
GetExitCodeProcess
GetExitCodeThread
GetFileSize
GetFileAttributesA
GetFullPathNameA
GetLastError
GetLogicalDrives
GetOEMCP
K32GetModuleBaseNameA
GetModuleBaseNameA
GetModuleHandleA
GetProcAddress
GetTickCount
GetTimeZoneInformation
GetUserNameA
HeapAlloc
HeapCreate
HeapDestroy
HeapReAlloc
HeapFree
IsWow64Process
LoadLibraryA
LocalAlloc
LocalFree
LocalReAlloc
MoveFileA
MultiByteToWideChar
PeekNamedPipe
ReadFile
RemoveDirectoryA
RtlCaptureContext
SetCurrentDirectoryA
SetNamedPipeHandleState
Sleep
VirtualAlloc
VirtualFree
WaitNamedPipeA
WideCharToMultiByte
WriteFile

// iphlpapi
GetAdaptersInfo

// advapi32
AllocateAndInitializeSid
GetTokenInformation
InitializeSecurityDescriptor
ImpersonateLoggedOnUser
FreeSid
LookupAccountSidA
RevertToSelf
SetThreadToken
SetEntriesInAclA
SetSecurityDescriptorDacl

// msvcrt
printf
vsnprintf

// BOF
BeaconDataParse
BeaconDataInt
BeaconDataShort
BeaconDataLength
BeaconDataExtract
BeaconFormatAlloc
BeaconFormatReset
BeaconFormatAppend
BeaconFormatPrintf
BeaconFormatToString
BeaconFormatFree
BeaconFormatInt
BeaconOutput
BeaconPrintf
BeaconUseToken
BeaconRevertToken
BeaconIsAdmin
BeaconGetSpawnTo
BeaconInjectProcess
BeaconInjectTemporaryProcess
BeaconSpawnTemporaryProcess
BeaconCleanupProcess
toWideChar
BeaconInformation
BeaconAddValue
BeaconGetValue
BeaconRemoveValue
LoadLibraryA
GetProcAddress
GetModuleHandleA
FreeLibrary
__C_specific_handler

// wininet
InternetOpenA
InternetConnectA
HttpOpenRequestA
HttpSendRequestA
InternetSetOptionA
InternetQueryOptionA
HttpQueryInfoA
InternetQueryDataAvailable
InternetCloseHandle
InternetReadFile

// ws2_32
WSAStartup
WSACleanup
socket
gethostbyname
ioctlsocket
connect
WSAGetLastError
closesocket
select
__WSAFDIsSet
shutdown
recv
send
accept
bind
listen
recvfrom
sendto
"""

##############################################

print('#pragma once')

for f in libs.split('\n'):
    if len(f) == 0:
        print()
    elif f[:2]=='//':
        continue
    else:
        print('#define HASH_LIB_%s%s0x%x' % ( f.upper().split(".")[0], (35-len(f))*" ", djb2w(f) ) )

for f in functions.split('\n'):
    if len(f) == 0:
        print()
    elif f[:2]=='//':
        print(f)
    else:
        print('#define HASH_FUNC_%s%s0x%x' % ( f.upper(), (35-len(f))*" ", djb2a(f) ) )