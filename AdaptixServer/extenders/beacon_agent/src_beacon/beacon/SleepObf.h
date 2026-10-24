#pragma once
#include "ApiLoader.h"

BOOL EkkoSleep(ULONG sleepMs);

void ObfSleep(ULONG ms);

void ObfSleepWithEvent(HANDLE hEvent, ULONG ms);

void SaveImageInfo();

void StompPEHeaders();

extern PVOID  g_ImageBase;
extern SIZE_T g_ImageSize;
