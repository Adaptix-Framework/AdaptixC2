#pragma once

#include <windows.h>

class AgentInfo
{
	DWORD AgentId;
	WORD  Acp;
	WORD  Oemcp;
	BYTE  GmtOffest;
	WORD  Pid;
	WORD  Tid;
	BOOL  Arch64;
	BOOL  Sys64;
	BOOL  Elevated;
	BOOL  IsServer;
	WORD  MajorVersion;
	WORD  MinorVersion;
	WORD  BuildNumber;
	ULONG InternalIP;
	CHAR* Process;
	CHAR* Domain;
	CHAR* Hostname;
	CHAR* Username;

public:
	AgentInfo();
	~AgentInfo();
};