#pragma once

#include "AgentInfo.h"
#include "AgentConfig.h"

class Agent
{
	AgentInfo*   info;

public:
	AgentConfig* config;

	Agent();
	~Agent();

	LPSTR BuildBeat();
};
