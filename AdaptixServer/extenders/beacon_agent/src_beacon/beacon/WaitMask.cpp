#include "WaitMask.h"

void WaitMask(ULONG worktime, ULONG sleepTime, ULONG jitter) 
{
    ULONG maxSleepTime = 0;
    if (worktime) {
        maxSleepTime = worktime * 1000;
    }
    else if (sleepTime) {
        maxSleepTime = sleepTime * 1000;
        if (jitter) {
            ULONG deltaTime = 0;
            ULONG minTime = sleepTime * jitter / 100;
            if (minTime)
                deltaTime = GenerateRandom32() % minTime;
            if (deltaTime < maxSleepTime)
                maxSleepTime -= deltaTime;
        }
    }
    mySleep(maxSleepTime);
}

void WaitMaskWithEvent(HANDLE hEvent, ULONG worktime, ULONG sleepTime, ULONG jitter)
{
    ULONG maxSleepTime = 0;
    if (worktime) {
        maxSleepTime = worktime * 1000;
    }
    else if (sleepTime) {
        maxSleepTime = sleepTime * 1000;
        if (jitter) {
            ULONG deltaTime = 0;
            ULONG minTime = sleepTime * jitter / 100;
            if (minTime)
                deltaTime = GenerateRandom32() % minTime;
            if (deltaTime < maxSleepTime)
                maxSleepTime -= deltaTime;
        }
    }
    
    if (hEvent) {
        DWORD waitResult = ApiWin->WaitForSingleObject(hEvent, maxSleepTime);
        if (waitResult == WAIT_OBJECT_0)
            ApiWin->ResetEvent(hEvent);
    }
    else {
        ApiWin->Sleep(maxSleepTime);
    }
}

void mySleep(ULONG ms) 
{
    ApiWin->Sleep(ms);
}
