#pragma once
#include "ApiLoader.h"
#include "utils.h"

class Packer
{
	DWORD  size;
	BYTE*  buffer;
	DWORD  index;

public:
	Packer();
	Packer(BYTE* buffer, ULONG size);
	~Packer();

	VOID Set32(ULONG index, ULONG value);

	VOID Pack64(ULONG64 value);
	VOID Pack32(ULONG value);
	VOID Pack16(WORD value);
	VOID Pack8(BYTE value);
	VOID PackBytes(PBYTE data, ULONG data_size);
	VOID PackStringA(LPSTR str);

	ULONG Unpack32();
	BYTE* UnpackBytes(ULONG* size);

	PBYTE GetData();
	ULONG GetDataSize();
};