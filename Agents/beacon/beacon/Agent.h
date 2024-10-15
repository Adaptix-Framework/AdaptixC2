#pragma once

#include "AgentInfo.h"
#include "AgentConfig.h"

class Agent
{
	AgentInfo*   info;
	AgentConfig* config;

public:
	Agent();
	~Agent();
};
