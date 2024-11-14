#include "Downloader.h"
#include "utils.h"

Downloader::Downloader(ULONG chunk_size)
{
	this->chunkSize = chunk_size;
}

bool Downloader::IsEmpty()
{
	return this->downloads.size() == 0;
}

DownloadData Downloader::CreateDownloadData(ULONG taskId, HANDLE hFile, ULONG size)
{
	DownloadData downloadData;
	downloadData.taskId   = taskId;
	downloadData.fileId   = GenerateRandom32();
	downloadData.hFile    = hFile;
	downloadData.fileSize = size;
	downloadData.index = 0;
	downloadData.state    = DOWNLOAD_STATE_RUNNING;

	this->downloads.push_back(downloadData);

    return downloadData;
}

void Downloader::ProcessDownloadTasks(Packer* packer)
{
	if (this->downloads.size() == 0)
		return;

	LPVOID buffer = MemAllocLocal(this->chunkSize);
	
	for (int i = 0; i < downloads.size(); i++) {
		if (downloads[i].state == DOWNLOAD_STATE_RUNNING) {
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
					
					memset(buffer, 0, readedBytes);
				}
				else {
					downloads[i].state = DOWNLOAD_STATE_CANCELED;
				}
		}
		if (downloads[i].state == DOWNLOAD_STATE_FINISHED || downloads[i].state == DOWNLOAD_STATE_CANCELED) {
			packer->Pack32(downloads[i].taskId);
			packer->Pack32(COMMAND_DOWNLOAD);
			packer->Pack32(downloads[i].fileId);
			packer->Pack8(DOWNLOAD_FINISH);
			packer->Pack8(downloads[i].state);

			if (downloads[i].hFile) {
				ApiNt->NtClose(downloads[i].hFile);
				downloads[i].hFile = NULL;
			}

			downloads.remove(i);
			--i;
	    }
	}
}