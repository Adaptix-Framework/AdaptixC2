#pragma once

#include "Agent.h"

class Connector;

extern Agent* g_Agent;
extern Connector* g_Connector;

DWORD WINAPI AgentMain(LPVOID lpParam);

void AgentExit(int method);