#pragma once

#include "Agent.h"

extern Agent* g_Agent;

DWORD WINAPI AgentMain(LPVOID lpParam);

void AgentExit(int method);