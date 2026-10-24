#include "Syscalls.h"
#include "ProcLoader.h"

#ifdef __x86_64__

extern "C" {
    DWORD ssn_NtClose = 0;
    DWORD ssn_NtWaitForSingleObject = 0;
    DWORD ssn_NtFreeVirtualMemory = 0;
    DWORD ssn_NtQueryInformationProcess = 0;
    DWORD ssn_NtQuerySystemInformation = 0;
    DWORD ssn_NtOpenProcess = 0;
    DWORD ssn_NtOpenProcessToken = 0;
    DWORD ssn_NtOpenThreadToken = 0;
    DWORD ssn_NtTerminateThread = 0;
    DWORD ssn_NtTerminateProcess = 0;
    DWORD ssn_NtOpenSection = 0;
    DWORD ssn_NtMapViewOfSection = 0;
    DWORD ssn_NtUnmapViewOfSection = 0;
    DWORD ssn_NtProtectVirtualMemory = 0;
    PVOID SyscallAddr = NULL;
}

asm(
    ".intel_syntax noprefix\n"
    ".text\n"

    ".globl SysNtClose\n"
    "SysNtClose:\n"
    "    mov r10, rcx\n"
    "    mov eax, dword ptr [rip + ssn_NtClose]\n"
    "    jmp qword ptr [rip + SyscallAddr]\n"
    "\n"

    ".globl SysNtWaitForSingleObject\n"
    "SysNtWaitForSingleObject:\n"
    "    mov r10, rcx\n"
    "    mov eax, dword ptr [rip + ssn_NtWaitForSingleObject]\n"
    "    jmp qword ptr [rip + SyscallAddr]\n"
    "\n"

    ".globl SysNtFreeVirtualMemory\n"
    "SysNtFreeVirtualMemory:\n"
    "    mov r10, rcx\n"
    "    mov eax, dword ptr [rip + ssn_NtFreeVirtualMemory]\n"
    "    jmp qword ptr [rip + SyscallAddr]\n"
    "\n"

    ".globl SysNtQueryInformationProcess\n"
    "SysNtQueryInformationProcess:\n"
    "    mov r10, rcx\n"
    "    mov eax, dword ptr [rip + ssn_NtQueryInformationProcess]\n"
    "    jmp qword ptr [rip + SyscallAddr]\n"
    "\n"

    ".globl SysNtQuerySystemInformation\n"
    "SysNtQuerySystemInformation:\n"
    "    mov r10, rcx\n"
    "    mov eax, dword ptr [rip + ssn_NtQuerySystemInformation]\n"
    "    jmp qword ptr [rip + SyscallAddr]\n"
    "\n"

    ".globl SysNtOpenProcess\n"
    "SysNtOpenProcess:\n"
    "    mov r10, rcx\n"
    "    mov eax, dword ptr [rip + ssn_NtOpenProcess]\n"
    "    jmp qword ptr [rip + SyscallAddr]\n"
    "\n"

    ".globl SysNtOpenProcessToken\n"
    "SysNtOpenProcessToken:\n"
    "    mov r10, rcx\n"
    "    mov eax, dword ptr [rip + ssn_NtOpenProcessToken]\n"
    "    jmp qword ptr [rip + SyscallAddr]\n"
    "\n"

    ".globl SysNtOpenThreadToken\n"
    "SysNtOpenThreadToken:\n"
    "    mov r10, rcx\n"
    "    mov eax, dword ptr [rip + ssn_NtOpenThreadToken]\n"
    "    jmp qword ptr [rip + SyscallAddr]\n"
    "\n"

    ".globl SysNtTerminateThread\n"
    "SysNtTerminateThread:\n"
    "    mov r10, rcx\n"
    "    mov eax, dword ptr [rip + ssn_NtTerminateThread]\n"
    "    jmp qword ptr [rip + SyscallAddr]\n"
    "\n"

    ".globl SysNtTerminateProcess\n"
    "SysNtTerminateProcess:\n"
    "    mov r10, rcx\n"
    "    mov eax, dword ptr [rip + ssn_NtTerminateProcess]\n"
    "    jmp qword ptr [rip + SyscallAddr]\n"
    "\n"

    ".globl SysNtOpenSection\n"
    "SysNtOpenSection:\n"
    "    mov r10, rcx\n"
    "    mov eax, dword ptr [rip + ssn_NtOpenSection]\n"
    "    jmp qword ptr [rip + SyscallAddr]\n"
    "\n"

    ".globl SysNtMapViewOfSection\n"
    "SysNtMapViewOfSection:\n"
    "    mov r10, rcx\n"
    "    mov eax, dword ptr [rip + ssn_NtMapViewOfSection]\n"
    "    jmp qword ptr [rip + SyscallAddr]\n"
    "\n"

    ".globl SysNtUnmapViewOfSection\n"
    "SysNtUnmapViewOfSection:\n"
    "    mov r10, rcx\n"
    "    mov eax, dword ptr [rip + ssn_NtUnmapViewOfSection]\n"
    "    jmp qword ptr [rip + SyscallAddr]\n"
    "\n"

    ".globl SysNtProtectVirtualMemory\n"
    "SysNtProtectVirtualMemory:\n"
    "    mov r10, rcx\n"
    "    mov eax, dword ptr [rip + ssn_NtProtectVirtualMemory]\n"
    "    jmp qword ptr [rip + SyscallAddr]\n"
);

struct ZW_ENTRY {
    ULONG_PTR addr;
    ULONG     ntHash;
};

static void SortEntries(ZW_ENTRY* entries, DWORD count)
{
    for (DWORD i = 1; i < count; i++) {
        ZW_ENTRY tmp = entries[i];
        DWORD j = i;
        while (j > 0 && entries[j - 1].addr > tmp.addr) {
            entries[j] = entries[j - 1];
            j--;
        }
        entries[j] = tmp;
    }
}

static PVOID FindSyscallGadget(HMODULE ntdll)
{
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)ntdll;
    PIMAGE_NT_HEADERS nt  = (PIMAGE_NT_HEADERS)((BYTE*)ntdll + dos->e_lfanew);
    PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);

    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) {
            BYTE* start = (BYTE*)ntdll + sec[i].VirtualAddress;
            DWORD size  = sec[i].Misc.VirtualSize;
            for (DWORD j = 0; j + 2 < size; j++) {
                if (start[j] == 0x0F && start[j + 1] == 0x05 && start[j + 2] == 0xC3)
                    return &start[j];
            }
        }
    }
    return NULL;
}

static ULONG ZwNameToNtHash(PUCHAR zwName)
{
    ULONG hash = 1572;
    hash = ((hash << 5) + hash) + 'n';
    hash = ((hash << 5) + hash) + 't';

    PUCHAR p = zwName + 2;
    int c;
    while ((c = *p++)) {
        if (c >= 'A' && c <= 'Z')
            c += 0x20;
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

BOOL SyscallsInit()
{
    HMODULE ntdll = SysModules->Ntdll;
    if (!ntdll)
        return FALSE;

    SyscallAddr = FindSyscallGadget(ntdll);
    if (!SyscallAddr)
        return FALSE;

    uintptr_t base = (uintptr_t)ntdll;
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)(base + ((PIMAGE_DOS_HEADER)base)->e_lfanew);
    PIMAGE_DATA_DIRECTORY dataDir = &ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    PIMAGE_EXPORT_DIRECTORY exports = (PIMAGE_EXPORT_DIRECTORY)(base + dataDir->VirtualAddress);

    DWORD  numNames = exports->NumberOfNames;
    DWORD* nameRVAs = (DWORD*)(base + exports->AddressOfNames);
    WORD*  ordinals = (WORD*)(base + exports->AddressOfNameOrdinals);
    DWORD* funcRVAs = (DWORD*)(base + exports->AddressOfFunctions);

    #define MAX_ZW 600
    ZW_ENTRY* zw = (ZW_ENTRY*)ApiWin->LocalAlloc(LPTR, sizeof(ZW_ENTRY) * MAX_ZW);
    if (!zw)
        return FALSE;

    DWORD zwCount = 0;
    for (DWORD i = 0; i < numNames && zwCount < MAX_ZW; i++) {
        PUCHAR name = (PUCHAR)(base + nameRVAs[i]);
        if (name[0] == 'Z' && name[1] == 'w') {
            zw[zwCount].addr   = base + funcRVAs[ordinals[i]];
            zw[zwCount].ntHash = ZwNameToNtHash(name);
            zwCount++;
        }
    }

    SortEntries(zw, zwCount);

    DWORD targetHashes[] = {
        HASH_FUNC_NTCLOSE,
        HASH_FUNC_NTWAITFORSINGLEOBJECT,
        HASH_FUNC_NTFREEVIRTUALMEMORY,
        HASH_FUNC_NTQUERYINFORMATIONPROCESS,
        HASH_FUNC_NTQUERYSYSTEMINFORMATION,
        HASH_FUNC_NTOPENPROCESS,
        HASH_FUNC_NTOPENPROCESSTOKEN,
        HASH_FUNC_NTOPENTHREADTOKEN,
        HASH_FUNC_NTTERMINATETHREAD,
        HASH_FUNC_NTTERMINATEPROCESS,
        HASH_FUNC_NTOPENSECTION,
        HASH_FUNC_NTMAPVIEWOFSECTION,
        HASH_FUNC_NTUNMAPVIEWOFSECTION,
        HASH_FUNC_NTPROTECTVIRTUALMEMORY,
    };

    DWORD* ssnSlots[] = {
        &ssn_NtClose,
        &ssn_NtWaitForSingleObject,
        &ssn_NtFreeVirtualMemory,
        &ssn_NtQueryInformationProcess,
        &ssn_NtQuerySystemInformation,
        &ssn_NtOpenProcess,
        &ssn_NtOpenProcessToken,
        &ssn_NtOpenThreadToken,
        &ssn_NtTerminateThread,
        &ssn_NtTerminateProcess,
        &ssn_NtOpenSection,
        &ssn_NtMapViewOfSection,
        &ssn_NtUnmapViewOfSection,
        &ssn_NtProtectVirtualMemory,
    };

    DWORD numTargets = sizeof(targetHashes) / sizeof(targetHashes[0]);
    DWORD found = 0;

    for (DWORD i = 0; i < zwCount && found < numTargets; i++) {
        for (DWORD j = 0; j < numTargets; j++) {
            if (zw[i].ntHash == targetHashes[j]) {
                *ssnSlots[j] = i;
                found++;
                break;
            }
        }
    }

    ApiWin->LocalFree(zw);
    return (found == numTargets);
}

#else

BOOL SyscallsInit()
{
    return TRUE;
}

#endif
