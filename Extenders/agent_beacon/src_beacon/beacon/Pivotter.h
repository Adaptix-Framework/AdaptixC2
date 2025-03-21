#pragma once

#include "std.cpp"
#include "Packer.h"

#define PIVOT_TYPE_SMB 1

struct PivotData {
	ULONG  Id;
	ULONG  Type;
	HANDLE Channel;
};

class Pivotter
{
public:
	Vector<PivotData> pivots;

	void LinkPivotSMB(ULONG taskId, ULONG commandId, CHAR* pipename, Packer* outPacker);
};