#pragma once
#include "adaptix.h"
#include "ApiLoader.h"

typedef struct _BOF_STOMP_CTX {
    HMODULE          hModule;
    PVOID            mappedView;
    SIZE_T           viewSize;
    int              method;
    PVOID            textBase;
    SIZE_T           textSize;
    PVOID            savedBytes;
    DWORD            savedSize;
    PVOID            pdataBase;
    DWORD            pdataSize;
    PVOID            savedPdata;
    PVOID            cursorBase;
    DWORD            cursorSize;
    BOOL             inUse;
    CRITICAL_SECTION lock;
    BOOL             initialised;
    PVOID            fakeLdrEntry;
} BOF_STOMP_CTX;

extern BOF_STOMP_CTX g_BofStomp;

BOOL InitBofStomp(const char* sacrificialDll, int method);
