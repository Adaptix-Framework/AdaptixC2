#pragma once

#include "Agent.h"

#pragma pack(push, 1)
typedef struct
{
    PVOID KaynLdr;
    PVOID DllCopy;
    PVOID Demon;
    DWORD DemonSize;
    PVOID TxtBase;
    DWORD TxtSize;
} KAYN_ARGS, *PKAYN_ARGS;
#pragma pack(pop)

extern Agent* g_Agent;

DWORD WINAPI AgentMain(LPVOID lpParam);

void AgentExit(int method);