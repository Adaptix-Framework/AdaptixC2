#pragma once

#include <windows.h>

class AgentInfo
{
public:
	DWORD agent_id;
	WORD  acp;
	WORD  oemcp;
	BYTE  gmt_offest;
	WORD  pid;
	WORD  tid;
	BOOL  arch64;
	BOOL  sys64;
	BOOL  elevated;
	BOOL  is_server;
	WORD  major_version;
	WORD  minor_version;
	WORD  build_number;
	ULONG internal_ip;
	CHAR* process_name;
	CHAR* domain_name;
	CHAR* computer_name;
	CHAR* username;

	AgentInfo();
};