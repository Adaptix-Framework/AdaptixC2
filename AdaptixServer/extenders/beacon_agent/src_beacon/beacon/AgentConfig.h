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

// DNS transport profile will be used when building BEACON_DNS payloads.
// It is kept minimal for now and will be aligned with the Teamserver
// DNS listener configuration once the transport is implemented.
typedef struct {
	BYTE* domain;       // e.g. "example.com"
	BYTE* resolvers;    // optional custom resolvers list / string
	BYTE* qtype;        // e.g. "TXT", "A" (stored as ASCII string)
	ULONG pkt_size;     // max payload per DNS message
	ULONG label_size;   // max Base32 chars per DNS label (<=63)
	ULONG ttl;          // response TTL
	BYTE* encrypt_key;  // RC4 key, same as listener_encrypt_key
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