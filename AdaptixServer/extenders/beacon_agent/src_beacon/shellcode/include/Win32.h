/**
 * Win32.h - Replicated from Havoc Shellcode
 */

#ifndef _WIN32_H_
#define _WIN32_H_

#include <windows.h>
#include <Macro.h>

UINT_PTR LdrModulePeb( UINT_PTR hModuleHash );
PVOID LdrFunctionAddr( UINT_PTR hModule, UINT_PTR ProcHash );

#endif // _WIN32_H_
