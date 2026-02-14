/**
 * Macro.h - Replicated from Havoc Shellcode
 */

#ifndef _MACRO_H_
#define _MACRO_H_

#include <windows.h>

#ifdef _WIN64
    #define PPEB_PTR __readgsqword( 0x60 )
#else
    #define PPEB_PTR __readfsdword( 0x30 )
#endif

#define SEC( s, x )         __attribute__( ( section( "." #s "$" #x "" ) ) )
#define U_PTR( x )          ( ( UINT_PTR ) x )
#define C_PTR( x )          ( ( LPVOID ) x )
#define NtCurrentProcess()  ( HANDLE ) ( ( HANDLE ) - 1 )

#define GET_SYMBOL( x )     ( ULONG_PTR )( GetRIP( ) - ( ( ULONG_PTR ) & GetRIP - ( ULONG_PTR ) x ) )

#endif // _MACRO_H_
