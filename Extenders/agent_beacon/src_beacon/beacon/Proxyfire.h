#pragma once

#include "std.cpp"
#include "Packer.h"

#define COMMAND_TUNNEL_START_TCP 62
#define COMMAND_TUNNEL_START_UDP 63
#define COMMAND_TUNNEL_WRITE_TCP 64
#define COMMAND_TUNNEL_WRITE_UDP 65
#define COMMAND_TUNNEL_CLOSE     66
#define COMMAND_TUNNEL_REVERSE   67
#define COMMAND_TUNNEL_ACCEPT    68

#define TUNNEL_STATE_CLOSE   0
#define TUNNEL_STATE_READY   1
#define TUNNEL_STATE_CONNECT 2

#define TUNNEL_MODE_SEND_TCP 0
#define TUNNEL_MODE_SEND_UDP 1
#define TUNNEL_MODE_REVERSE_TCP 2

struct TunnelData {
	ULONG  channelID;
	SOCKET sock;
	BYTE   state;
	BYTE   mode;
	ULONG  i_address;
	WORD   port;
	ULONG  waitTime;
	ULONG  startTick;
	ULONG  closeTimer;
};

class Proxyfire
{
public:
	Vector<TunnelData> tunnels;
	
	void ProcessTunnels(Packer* packer);

	void  CheckProxy(Packer* packer);
	ULONG RecvProxy(Packer* packer);
	void  CloseProxy();

	void ConnectMessageTCP(ULONG channelId, CHAR* address, WORD port, Packer* outPacker);
	void ConnectMessageUDP(ULONG channelId, CHAR* address, WORD port, Packer* outPacker);
	void ConnectWriteTCP(ULONG channelId, CHAR* data, ULONG dataSize);
	void ConnectWriteUDP(ULONG channelId, CHAR* data, ULONG dataSize);
	void ConnectClose(ULONG channelId);
	void ConnectMessageReverse(ULONG tunnelId, WORD port, Packer* outPacker);

	void AddProxyData(ULONG channelId, SOCKET sock, ULONG waitTime, ULONG mode, ULONG address, WORD port, ULONG state);
};