#pragma once

#include "std.cpp"
#include "Packer.h"
#include "ApiLoader.h"

#define ASYNC_BOF_STATE_PENDING   0x0
#define ASYNC_BOF_STATE_RUNNING   0x1
#define ASYNC_BOF_STATE_FINISHED  0x2
#define ASYNC_BOF_STATE_STOPPED   0x3

#define ASYNC_BOF_OUTPUT_BUFFER_SIZE 0x10000

struct AsyncBofContext {
    ULONG   taskId;
    ULONG   state;
    HANDLE  hThread;
    DWORD   threadId;
    HANDLE  hStopEvent;
    
    BYTE*   coffFile;
    ULONG   coffFileSize;
    BYTE*   args;
    ULONG   argsSize;
    CHAR*   entryName;
    
    CRITICAL_SECTION outputLock;
    Packer* outputBuffer;
    
    PCHAR   mapSections[25];
    LPVOID* mapFunctions;
};

extern __declspec(thread) AsyncBofContext* tls_CurrentBofContext;



class Boffer
{
public:
    Vector<AsyncBofContext*> asyncBofs;
    
    HANDLE  wakeupEvent;
    CRITICAL_SECTION managerLock;
    
    Boffer();
    ~Boffer();
    
    BOOL Initialize();
    
    AsyncBofContext* CreateAsyncBof(ULONG taskId, CHAR* entryName, BYTE* coffFile, ULONG coffFileSize, BYTE* args, ULONG argsSize);
    
    BOOL StartAsyncBof(AsyncBofContext* ctx);
    
    BOOL StopAsyncBof(ULONG taskId);
    
    void ProcessAsyncBofs(Packer* outPacker);
    
    void CleanupFinishedBofs();
    
    AsyncBofContext* FindBofByThreadId(DWORD threadId);
    
    HANDLE GetWakeupEvent();
    
    void SignalWakeup();
        
    static void* operator new(size_t sz);
    static void operator delete(void* p) noexcept;
    
private:
    void CleanupBofContext(AsyncBofContext* ctx);
};

extern Boffer* g_AsyncBofManager;

