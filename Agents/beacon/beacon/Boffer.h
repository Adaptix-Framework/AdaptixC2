#pragma once
#include "beacon.h"
#include "ApiLoader.h"

#define MAX_SECTIONS	   25
#define MAP_FUNCTIONS_SIZE 4096

#define IMAGE_REL_AMD64_ADDR64   0x0001
#define IMAGE_REL_AMD64_ADDR32NB 0x0003
#define IMAGE_REL_AMD64_REL32    0x0004
#define HASH_KEY 13

extern unsigned char* BeaconFunctions[27][2];
extern int BeaconFunctionsCount;

extern char* beacon_output;
extern int   output_size;
extern int   output_offset;

typedef struct COF_HEADER {
	short Machine;
	short NumberOfSections;
	int   TimeDateStamp;
	int   PointerToSymbolTable;
	int   NumberOfSymbols;
	short SizeOfOptionalHeader;
	short Characteristics;
} COF_HEADER;

#pragma pack(push,1)

typedef struct COF_SECTION {
	char  Name[8];
	int   VirtualSize;
	int   VirtualAddress;
	int   SizeOfRawData;
	int   PointerToRawData;
	int   PointerToRelocations;
	int   PointerToLineNumbers;
	short NumberOfRelocations;
	short NumberOfLinenumbers;
	int   Characteristics;
} COF_SECTION;

typedef struct COF_RELOCATION {
	int   VirtualAddress;
	int   SymbolTableIndex;
	short Type;
} COF_RELOCATION;

typedef struct COF_SYMBOL {
	union {
		char cName[8];
		int  dwName[2];
	}     Name;
	int   Value;
	short SectionNumber;
	short Type;
	char  StorageClass;
	char  NumberOfAuxSymbols;
} COF_SYMBOL;

#pragma pack(pop)

class Boffer 
{
public:
	Boffer();
};

char* ObjectExecute(int* retSize, char* targetFuncName, unsigned char* coffFile, unsigned int cofFileSize, unsigned char* args, int argsSize);