#include "Agent.h"
#include "ApiLoader.h"
#include "utils.h"

Agent::Agent()
{
	this->info = (AgentInfo*) MemAllocLocal(sizeof(AgentInfo));
	*this->info = AgentInfo();
}

Agent::~Agent()
{

}
