/**
 * Utils.h - Replicated from Havoc Shellcode
 */

#ifndef _UTILS_H_
#define _UTILS_H_

#include <windows.h>

#define BARRIER __asm__ __volatile__ ("" ::: "memory")

#define HIDDEN_CONST(var, a, b) \
    do { \
        var = (a); \
        BARRIER; \
        var += (b); \
        BARRIER; \
    } while(0)


UINT_PTR HashString( LPVOID String, UINT_PTR Length );

#endif // _UTILS_H_
