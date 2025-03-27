#pragma once

#include "std.cpp"
#include "Packer.h"

#define COMMAND_PIVOT_EXEC   37
#define COMMAND_LINK	     38
#define COMMAND_UNLINK	     39

#define PIVOT_TYPE_SMB		  1
#define PIVOT_TYPE_DISCONNECT 10

struct PivotData {
	ULONG  Id;
	ULONG  Type;
	HANDLE Channel;
};

class Pivotter
{
public:
	Vector<PivotData> pivots;

	void ProcessPivots(Packer* packer);

	void LinkPivotSMB(ULONG taskId, ULONG commandId, CHAR* pipename, Packer* outPacker);
	void UnlinkPivot(ULONG taskId, ULONG commandId, ULONG pivotId, Packer* outPacker);
	void WritePivot(ULONG pivotId, BYTE* data, ULONG size);
};