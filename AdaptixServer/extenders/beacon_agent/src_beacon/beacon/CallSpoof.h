#pragma once
#include "ApiLoader.h"

#ifdef __x86_64__

BOOL CallSpoofInit();
void SpoofThreadStack(PVOID imageBase, SIZE_T imageSize);
void RestoreThreadStack();

extern "C" {
PVOID SpoofCall4(PVOID target, PVOID a1, PVOID a2, PVOID a3, PVOID a4);
PVOID SpoofCall6(PVOID target, PVOID a1, PVOID a2, PVOID a3, PVOID a4, PVOID a5, PVOID a6);
}

#else

inline BOOL CallSpoofInit() { return TRUE; }
inline void SpoofThreadStack(PVOID imageBase, SIZE_T imageSize) { (void)imageBase; (void)imageSize; }
inline void RestoreThreadStack() {}

#endif
