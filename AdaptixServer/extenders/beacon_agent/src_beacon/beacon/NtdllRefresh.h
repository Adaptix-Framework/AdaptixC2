#pragma once
#include "ApiLoader.h"

BOOL IsNtStubHooked(PVOID pFunc);
BOOL RefreshNtdllStub(PVOID pFunc);
