#pragma once
#include "ApiLoader.h"

BOOL SyscallsInit();

#ifdef __x86_64__

extern "C" {

NTSTATUS SysNtClose(HANDLE Handle);
NTSTATUS SysNtWaitForSingleObject(HANDLE Handle, BOOLEAN Alertable, PLARGE_INTEGER Timeout);
NTSTATUS SysNtFreeVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress, PSIZE_T RegionSize, ULONG FreeType);
NTSTATUS SysNtQueryInformationProcess(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength);
NTSTATUS SysNtQuerySystemInformation(SYSTEM_INFORMATION_CLASS SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength);
NTSTATUS SysNtOpenProcess(PHANDLE ProcessHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PCLIENT_ID ClientId);
NTSTATUS SysNtOpenProcessToken(HANDLE ProcessHandle, ACCESS_MASK DesiredAccess, PHANDLE TokenHandle);
NTSTATUS SysNtOpenThreadToken(HANDLE ThreadHandle, ACCESS_MASK DesiredAccess, BOOLEAN OpenAsSelf, PHANDLE TokenHandle);
NTSTATUS SysNtTerminateThread(HANDLE ThreadHandle, NTSTATUS ExitStatus);
NTSTATUS SysNtTerminateProcess(HANDLE ProcessHandle, NTSTATUS ExitStatus);
NTSTATUS SysNtOpenSection(PHANDLE SectionHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes);
NTSTATUS SysNtMapViewOfSection(HANDLE SectionHandle, HANDLE ProcessHandle, PVOID* BaseAddress, ULONG_PTR ZeroBits, SIZE_T CommitSize, PLARGE_INTEGER SectionOffset, PSIZE_T ViewSize, SECTION_INHERIT InheritDisposition, ULONG AllocationType, ULONG Win32Protect);
NTSTATUS SysNtUnmapViewOfSection(HANDLE ProcessHandle, PVOID BaseAddress);
NTSTATUS SysNtProtectVirtualMemory(HANDLE ProcessHandle, PVOID* BaseAddress, PSIZE_T RegionSize, ULONG NewProtect, PULONG OldProtect);

}

#else

inline NTSTATUS SysNtClose(HANDLE h) { return ApiNt->NtClose(h); }
inline NTSTATUS SysNtWaitForSingleObject(HANDLE h, BOOLEAN a, PLARGE_INTEGER t) { return ApiNt->NtWaitForSingleObject(h, a, t); }
inline NTSTATUS SysNtFreeVirtualMemory(HANDLE p, PVOID* b, PSIZE_T r, ULONG f) { return ApiNt->NtFreeVirtualMemory(p, b, r, f); }
inline NTSTATUS SysNtQueryInformationProcess(HANDLE p, PROCESSINFOCLASS c, PVOID i, ULONG l, PULONG r) { return ApiNt->NtQueryInformationProcess(p, c, i, l, r); }
inline NTSTATUS SysNtQuerySystemInformation(SYSTEM_INFORMATION_CLASS c, PVOID i, ULONG l, PULONG r) { return ApiNt->NtQuerySystemInformation(c, i, l, r); }
inline NTSTATUS SysNtOpenProcess(PHANDLE p, ACCESS_MASK d, POBJECT_ATTRIBUTES o, PCLIENT_ID c) { return ApiNt->NtOpenProcess(p, d, o, c); }
inline NTSTATUS SysNtOpenProcessToken(HANDLE p, ACCESS_MASK d, PHANDLE t) { return ApiNt->NtOpenProcessToken(p, d, t); }
inline NTSTATUS SysNtOpenThreadToken(HANDLE t, ACCESS_MASK d, BOOLEAN o, PHANDLE h) { return ApiNt->NtOpenThreadToken(t, d, o, h); }
inline NTSTATUS SysNtTerminateThread(HANDLE t, NTSTATUS e) { return ApiNt->NtTerminateThread(t, e); }
inline NTSTATUS SysNtTerminateProcess(HANDLE p, NTSTATUS e) { return ApiNt->NtTerminateProcess(p, e); }
inline NTSTATUS SysNtOpenSection(PHANDLE s, ACCESS_MASK d, POBJECT_ATTRIBUTES o) { return NtOpenSection(s, d, o); }
inline NTSTATUS SysNtMapViewOfSection(HANDLE s, HANDLE p, PVOID* b, ULONG_PTR z, SIZE_T c, PLARGE_INTEGER o, PSIZE_T v, SECTION_INHERIT i, ULONG a, ULONG w) { return NtMapViewOfSection(s, p, b, z, c, o, v, i, a, w); }
inline NTSTATUS SysNtUnmapViewOfSection(HANDLE p, PVOID b) { return NtUnmapViewOfSection(p, b); }
inline NTSTATUS SysNtProtectVirtualMemory(HANDLE p, PVOID* b, PSIZE_T r, ULONG n, PULONG o) { return NtProtectVirtualMemory(p, b, r, n, o); }

#endif
