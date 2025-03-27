#include "WaitMask.h"

void WaitMask(ULONG sleepTime, ULONG jitter) 
{
    ULONG deltaTime = 0;
    ULONG maxTime   = sleepTime * 1000;
    ULONG minTime   = sleepTime * jitter / 100;
    
    if (minTime)
        deltaTime = GenerateRandom32() % minTime;

    if (deltaTime < maxTime)
        maxTime -= deltaTime;

    ApiWin->Sleep(maxTime);
}