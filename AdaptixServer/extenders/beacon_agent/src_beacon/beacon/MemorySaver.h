#pragma once

#include "std.cpp"
#include "Packer.h"

struct MemoryData {
	ULONG memoryId;
	ULONG totalSize;
	ULONG currentSize;
	PBYTE buffer;
	BOOL  complete;
};

class MemorySaver
{
public:
	Map<ULONG, MemoryData> chunks;

	MemorySaver();

	void WriteMemoryData(ULONG memoryId, ULONG totalSize, ULONG dataSize, PBYTE data);
	void RemoveMemoryData(ULONG memoryId);

	static void* operator new(size_t sz);
	static void operator delete(void* p) noexcept;
};