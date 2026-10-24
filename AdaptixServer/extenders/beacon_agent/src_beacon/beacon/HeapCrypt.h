#pragma once
#include "ApiLoader.h"

void HeapCryptInit();
void HeapTrack(PVOID ptr, DWORD size);
void HeapUntrack(PVOID ptr);
void HeapEncryptAll();
void HeapDecryptAll();
