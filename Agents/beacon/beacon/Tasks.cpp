#include "Tasks.h"

void CmdPwd(ULONG commandId, Packer* inPacker, Packer* outPacker)
{
	CHAR  path[MAX_PATH] = { 0 };
	ULONG pathSize = ApiWin->GetCurrentDirectoryA(MAX_PATH, path);

	if (pathSize) {
		outPacker->Pack32(commandId);
		outPacker->PackBytes((PBYTE)path, pathSize);
	}
	else {
		outPacker->Pack32( TEB->LastErrorValue );
	}
}