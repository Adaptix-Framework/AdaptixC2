#pragma once

#include "std.h"
#include "Packer.h"

struct MemoryData {
	ULONG memoryId;
	ULONG totalSize;
	ULONG currentSize;
	PBYTE buffer;
	bool  complete;
};

class MemorySaver
{
public:
	Map<ULONG, MemoryData> chunks;

	MemorySaver();

	void WriteMemoryData(ULONG memoryId, ULONG totalSize, ULONG dataSize, PBYTE data);
	void RemoveMemoryData(ULONG memoryId);
};