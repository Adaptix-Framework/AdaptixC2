#pragma once

#include "std.cpp"
#include "Packer.h"

#define COMMAND_DOWNLOAD  32
#define COMMAND_DOWNLOAD_STATE     35

#define DOWNLOAD_START    0x1
#define DOWNLOAD_CONTINUE 0x2
#define DOWNLOAD_FINISH   0x3

#define DOWNLOAD_STATE_RUNNING  0x1
#define DOWNLOAD_STATE_STOPPED  0x2
#define DOWNLOAD_STATE_FINISHED 0x3
#define DOWNLOAD_STATE_CANCELED 0x4

struct DownloadData {
	ULONG   taskId;
	ULONG   fileId;
	HANDLE  hFile;
	ULONG64 fileSize;
	ULONG64 index;
	BYTE    state;
};

class Downloader
{
public:
	Vector<DownloadData> downloads;
	ULONG chunkSize = 0;

	Downloader( ULONG chunk_size );

	DownloadData CreateDownloadData(ULONG taskId, HANDLE hFile, ULONG64 size);
	void         ProcessDownloader(Packer* packer);
	BOOL		 IsTasks();

	static void* operator new(size_t sz);
	static void operator delete(void* p) noexcept;
};