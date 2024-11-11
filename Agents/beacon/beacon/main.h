#pragma once

#include "Agent.h"
#include "ConnectorHTTP.h"
#pragma comment(lib, "wininet.lib")
#pragma warning(disable : 4996)

extern Agent*         g_Agent;
extern ConnectorHTTP* g_Connector;

void AgentMain(); 
