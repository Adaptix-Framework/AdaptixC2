#pragma once
#include "ApiLoader.h"
#include "utils.h"

class Packer
{
	LPVOID original;
	DWORD  size;
	LPVOID buffer;
	DWORD  index;

public:
	Packer();

	VOID Add64(ULONG64 value);
	VOID Add32(ULONG value);
	VOID Add16(WORD value);
	VOID Add8(BYTE value);
	VOID AddBytes(PBYTE data, ULONG data_size);
	VOID AddStringA(LPSTR str);

	PBYTE GetData();
	ULONG GetDataSize();
};