#pragma once
#include "beacon.h"
#include "ApiLoader.h"
#include "ApiDefines.h"
#include "Packer.h"

#define MAX_SECTIONS	   25
#define MAP_FUNCTIONS_SIZE 4096

#define IMAGE_REL_AMD64_ADDR64   0x0001
#define IMAGE_REL_AMD64_ADDR32NB 0x0003
#define IMAGE_REL_AMD64_REL32    0x0004
#define HASH_KEY 13

#define BOF_ERROR_PARSE	    0x101
#define BOF_ERROR_SYMBOL    0x102
#define BOF_ERROR_MAX_FUNCS 0x103
#define BOF_ERROR_ENTRY     0x104
#define BOF_ERROR_ALLOC     0x105

typedef struct {
	ULONG  hash;
	LPVOID proc;
} BOF_API;

extern Packer* bofOutputPacker;
extern int     bofOutputCount;
extern ULONG   bofTaskId;

extern ULONG bofImpersonate;


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

void InitBofOutputData();

Packer* ObjectExecute(ULONG taskId, char* targetFuncName, unsigned char* coffFile, unsigned int cofFileSize, unsigned char* args, int argsSize);