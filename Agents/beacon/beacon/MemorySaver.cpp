#include "MemorySaver.h"

MemorySaver::MemorySaver(){}

void MemorySaver::WriteMemoryData(ULONG memoryId, ULONG totalSize, ULONG dataSize, PBYTE data)
{
	if ( !chunks.contains(memoryId) ) {
		MemoryData memoryData = { 0 };
		memoryData.memoryId  = memoryId;
		memoryData.totalSize = totalSize;
		memoryData.buffer    = (PBYTE) MemAllocLocal(totalSize);
		
		chunks[memoryId] = memoryData;
	}

	memcpy( chunks[memoryId].buffer + chunks[memoryId].currentSize, data, dataSize );
	chunks[memoryId].currentSize += dataSize;

	if (chunks[memoryId].currentSize == chunks[memoryId].totalSize)
		chunks[memoryId].complete = true;
}

void MemorySaver::RemoveMemoryData(ULONG memoryId)
{
	if (chunks[memoryId].buffer)
		MemFreeLocal((LPVOID*)(&chunks[memoryId].buffer), chunks[memoryId].totalSize);
	chunks[memoryId] = { 0 };
	chunks.remove(memoryId);
}