#include "main.h"
#include "ApiLoader.h"
#include "utils.h"

Agent* g_Agent;

void AgentMain()
{
	if ( !ApiLoad() ) return;

	g_Agent = (Agent*)MemAllocLocal(sizeof(Agent));
	*g_Agent = Agent();
}