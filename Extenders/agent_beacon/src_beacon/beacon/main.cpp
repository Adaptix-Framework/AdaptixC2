#include "main.h"
#include "config.h"

#if defined(DEBUG)

int main()
{
    AgentMain();
    return 0;
}

#elif defined(BUILD_SVC)

SERVICE_STATUS        ServiceStatus = {0};
SERVICE_STATUS_HANDLE hStatus;

void ServiceMain(int argc, char** argv);
void ControlHandler(DWORD request);

void ServiceMain(int argc, char** argv) 
{
    CHAR* SvcName = getServiceName();
    hStatus = RegisterServiceCtrlHandlerA(SvcName, (LPHANDLER_FUNCTION)ControlHandler);
    if( !hStatus )
        return;

    ServiceStatus.dwServiceType = SERVICE_WIN32;
    ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    
    ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(hStatus, &ServiceStatus);

    AgentMain();

    ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(hStatus, &ServiceStatus);
}

void ControlHandler(DWORD request) 
{
    switch (request) {
    case SERVICE_CONTROL_STOP:
        ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        break;
    case SERVICE_CONTROL_SHUTDOWN:
        ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        break;
    default:
        break;
    }

    SetServiceStatus(hStatus, &ServiceStatus);
}

int main() 
{
    SERVICE_TABLE_ENTRYA ServiceTable[2];
    CHAR* SvcName = getServiceName();
    ServiceTable[0].lpServiceName = (LPSTR) SvcName;
    ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTIONA)ServiceMain;

    ServiceTable[1].lpServiceName = NULL;
    ServiceTable[1].lpServiceProc = NULL;

    StartServiceCtrlDispatcherA(ServiceTable);

    return 0;
}

#elif defined(BUILD_DLL)

extern "C" __declspec(dllexport) void WINAPI GetVersions()
{
    while (TRUE) {
        Sleep(24 * 60 * 60 * 1000);
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    HANDLE hThread = NULL;
	
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
	hThread = CreateThread(NULL, 0, AgentMain, NULL, 0, NULL);
        break;
    case DLL_PROCESS_DETACH:
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}

#elif defined(BUILD_SHELLCODE)

__declspec(dllexport) void GetVersions() {};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    AgentMain();
    return TRUE;
}
#else

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    AgentMain();
	return 0;
}

#endif
