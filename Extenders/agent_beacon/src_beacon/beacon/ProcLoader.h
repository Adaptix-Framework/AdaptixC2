#pragma once
#include <windows.h>
#include "ApiDefines.h"

ULONG Djb2A(PUCHAR str);

ULONG Djb2W(PWCHAR str);

HMODULE GetModuleAddress(ULONG modHash);

LPVOID GetSymbolAddress(HANDLE hModule, ULONG symbHash);