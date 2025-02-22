#pragma once

#include "std.cpp"
#include "Packer.h"

#define COMMAND_TUNNEL_START 63
#define COMMAND_TUNNEL_WRITE 64
#define COMMAND_TUNNEL_CLOSE 65

#define TUNNEL_STATE_CLOSE   0
#define TUNNEL_STATE_READY   1
#define TUNNEL_STATE_CONNECT 2

#define TUNNEL_MODE_SEND_TCP 0
#define TUNNEL_MODE_SEND_UDP 1
#define TUNNEL_MODE_RECV_TCP 2

struct TunnelData {
	ULONG  channelID;
	SOCKET sock;
	BYTE   state;
	BYTE   mode;
	ULONG  waitTime;
	ULONG  startTick;
	ULONG  closeTimer;
};

class Proxyfire
{
public:
	Vector<TunnelData> tunnels;

	void ProcessTunnels(Packer* packer);

	void CheckProxy(Packer* packer);
	ULONG RecvProxy(Packer* packer);
	void CloseProxy();

	void ConnectMessageTCP(ULONG channelId, CHAR* address, WORD port, Packer* outPacker);
	void ConnectWrite(ULONG channelId, CHAR* data, ULONG dataSize);
	void ConnectClose(ULONG channelId);

	void AddProxyData(ULONG channelId, SOCKET sock, ULONG waitTime, ULONG mode, ULONG state);
};