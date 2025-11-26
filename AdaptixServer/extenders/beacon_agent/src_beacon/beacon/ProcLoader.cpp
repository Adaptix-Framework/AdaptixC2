#include "ProcLoader.h"
#include "ApiLoader.h"
#include "utils.h"
#include "ntdll.h"

ULONG Djb2A(PUCHAR str)
{
    if (str == NULL)
        return 0;

    ULONG hash = 1572;
    int c;
    while (c = *str++) {
        if (c >= 'A' && c <= 'Z')
            c += 0x20;
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

ULONG Djb2W(PWCHAR str) 
{
    if (str == NULL)
        return 0;

    ULONG hash = 1572;
    int c;
    while (c = *str++) {
        if (c >= L'A' && c <= L'Z')
            c += 0x20;
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

HMODULE GetModuleAddress(ULONG modHash) 
{

#ifdef _M_IX86
    PEB* ProcEnvBlk = (PEB*)__readfsdword(0x30);
#else
    PEB* ProcEnvBlk = (PEB*)__readgsqword(0x60);
#endif

    PEB_LDR_DATA* Ldr = ProcEnvBlk->Ldr;
    LIST_ENTRY* ModuleList = NULL;
    ModuleList = &Ldr->InMemoryOrderModuleList;
    LIST_ENTRY* pStartListEntry = ModuleList->Flink;

    for (LIST_ENTRY* pListEntry = pStartListEntry; pListEntry != ModuleList; pListEntry = pListEntry->Flink) {
        LDR_DATA_TABLE_ENTRY* pEntry = (LDR_DATA_TABLE_ENTRY*)((BYTE*)pListEntry - sizeof(LIST_ENTRY));
        if ( Djb2W((PWCHAR)pEntry->BaseDllName.Buffer) == modHash )
            return (HMODULE)pEntry->DllBase;
    }
    return NULL;
}

LPVOID GetSymbolAddress(HANDLE hModule, ULONG symbHash)
{
    if (hModule == NULL)
        return 0;

    uintptr_t dllAddress = (uintptr_t) hModule;

    PIMAGE_NT_HEADERS       ntHeaders       = (PIMAGE_NT_HEADERS)(dllAddress + ((PIMAGE_DOS_HEADER)dllAddress)->e_lfanew);
    PIMAGE_DATA_DIRECTORY   dataDirectory   = (PIMAGE_DATA_DIRECTORY)&ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    PIMAGE_EXPORT_DIRECTORY exportDirectory = (PIMAGE_EXPORT_DIRECTORY)(dllAddress + dataDirectory->VirtualAddress);

    uintptr_t exportedAddressTable = dllAddress + exportDirectory->AddressOfFunctions;
    uintptr_t namePointerTable     = dllAddress + exportDirectory->AddressOfNames;
    uintptr_t ordinalTable         = dllAddress + exportDirectory->AddressOfNameOrdinals;
    uintptr_t symbolAddress        = 0;
    DWORD     procAddress          = 0;

    DWORD dwCounter = exportDirectory->NumberOfNames;
    while (dwCounter--) {
        PUCHAR cpExportedFunctionName = (PUCHAR)(dllAddress + *(DWORD*)namePointerTable);
        if ( Djb2A(cpExportedFunctionName) == symbHash ) {
            exportedAddressTable += (*(WORD*)ordinalTable * sizeof(DWORD));
            procAddress           = *(DWORD*)exportedAddressTable;
            symbolAddress         = dllAddress + procAddress;

            if ( dataDirectory->VirtualAddress < procAddress && procAddress < dataDirectory->VirtualAddress + dataDirectory->Size ) {
                char* symbol = (char*)(symbolAddress);
                char  moduleName[64] = { 0 };
                char  funcName[64]   = { 0 };

                int index = StrIndexA(symbol, '.');
                if (index >= 0) {
                    memcpy(moduleName, symbol, ++index);
                    moduleName[index    ] = 'd';
                    moduleName[index + 1] = 'l';
                    moduleName[index + 2] = 'l';
                    moduleName[index + 3] = 0;

                    memcpy(funcName, symbol + index, StrLenA(symbol) - index + 1);

                    HMODULE hModule  = ApiWin->LoadLibraryA(moduleName);
                    ULONG   hashFunc = Djb2A((PUCHAR)funcName);

                    memset(moduleName, 0, StrLenA(moduleName));
                    memset(funcName, 0, StrLenA(funcName));

                    if (hModule) {
                        return GetSymbolAddress(hModule, hashFunc);
                    }
                }
                break;
            }
            else {
                return (LPVOID) symbolAddress;
            }
        }
        namePointerTable += sizeof(DWORD);
        ordinalTable     += sizeof(WORD);
    }

    return NULL;
}