#pragma once

#include <windows.h>

class AgentConfig
{
	DWORD AgentType;


public:
	AgentConfig();
	~AgentConfig();

	void LoadConfig(BYTE* config);
};