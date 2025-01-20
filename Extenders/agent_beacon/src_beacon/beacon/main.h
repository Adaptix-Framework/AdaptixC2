#pragma once

#include "Agent.h"
#include "ConnectorHTTP.h"

extern Agent*         g_Agent;
extern ConnectorHTTP* g_Connector;

void AgentMain(); 

void AgentClear(int method);