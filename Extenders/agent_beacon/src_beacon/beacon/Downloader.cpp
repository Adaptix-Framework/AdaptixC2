#include "Downloader.h"
#include "utils.h"

Downloader::Downloader(ULONG chunk_size)
{
	this->chunkSize = chunk_size;
}

BOOL Downloader::IsTasks()
{
	if (this->downloads.size()) {
		for (int i = 0; i < this->downloads.size(); i++) {
			if (this->downloads[i].state == DOWNLOAD_STATE_RUNNING)
				return true;
		}
	}
	return false;
}

DownloadData Downloader::CreateDownloadData(ULONG taskId, HANDLE hFile, ULONG size)
{
	DownloadData downloadData;
	downloadData.taskId   = taskId;
	downloadData.fileId   = GenerateRandom32();
	downloadData.hFile    = hFile;
	downloadData.fileSize = size;
	downloadData.index    = 0;
	downloadData.state    = DOWNLOAD_STATE_RUNNING;

	this->downloads.push_back(downloadData);

    return downloadData;
}

void Downloader::ProcessDownloader(Packer* packer)
{
	if ( !this->downloads.size() )
		return;
	
	for (int i = 0; i < downloads.size(); i++) {
		BOOL close = false;
		if (downloads[i].state == DOWNLOAD_STATE_RUNNING) {
			LPVOID buffer = MemAllocLocal(this->chunkSize);
			ULONG readedBytes = 0;
			ApiWin->ReadFile(downloads[i].hFile, buffer, this->chunkSize, &readedBytes, NULL);
			if (readedBytes > 0) {
				downloads[i].index += readedBytes;

				packer->Pack32(downloads[i].taskId);
				packer->Pack32(COMMAND_DOWNLOAD);
				packer->Pack32(downloads[i].fileId);
				packer->Pack8(DOWNLOAD_CONTINUE);
				packer->PackBytes( (BYTE*) buffer, readedBytes);

				if (downloads[i].fileSize == downloads[i].index)
					downloads[i].state = DOWNLOAD_STATE_FINISHED;
			}
			else {
				downloads[i].state = DOWNLOAD_STATE_CANCELED;

				packer->Pack32(downloads[i].taskId);
				packer->Pack32(COMMAND_DOWNLOAD_STATE);
				packer->Pack32(downloads[i].fileId);
				packer->Pack8(downloads[i].state);
			}
			if(buffer)
				MemFreeLocal(&buffer, this->chunkSize);
		}

		if ( downloads[i].state == DOWNLOAD_STATE_FINISHED ) {
			packer->Pack32(downloads[i].taskId);
			packer->Pack32(COMMAND_DOWNLOAD);
			packer->Pack32(downloads[i].fileId);
			packer->Pack8(DOWNLOAD_FINISH);
	    }

		if ( downloads[i].state == DOWNLOAD_STATE_CANCELED || downloads[i].state == DOWNLOAD_STATE_FINISHED ) {
			if (downloads[i].hFile) {
				ApiNt->NtClose(downloads[i].hFile);
				downloads[i].hFile = NULL;
			}

			downloads.remove(i);
			--i;
		}
	}
}