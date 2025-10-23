package server

import (
	"AdaptixServer/core/utils/safe"
	"bytes"
	"encoding/json"

	"github.com/gorilla/websocket"
)

func (ts *Teamserver) TsClientExists(username string) bool {
	return ts.clients.Contains(username)
}

func (ts *Teamserver) TsClientSendChannel(username string) chan []byte {
	value, ok := ts.clients.Get(username)
	if !ok {
		return nil
	}
	client := value.(*Client)
	return client.Send
}

func (ts *Teamserver) TsClientSocketMatch(username string, socket *websocket.Conn) bool {
	value, ok := ts.clients.Get(username)
	if !ok {
		return false
	}
	client := value.(*Client)
	return client.Socket == socket
}

func (ts *Teamserver) TsClientConnect(username string, socket *websocket.Conn) {
	client := &Client{
		Username:      username,
		Synced:        false,
		Socket:        socket,
		TmpStore:      safe.NewSlice(),
		Send:          make(chan []byte, 256), // Buffered channel with 256 message capacity
		HeartbeatStop: make(chan struct{}),
	}

	ts.clients.Put(username, client)
}

func (ts *Teamserver) TsClientHeartbeatStop(username string) <-chan struct{} {
	value, ok := ts.clients.Get(username)
	if !ok {
		return nil
	}
	client := value.(*Client)
	return client.HeartbeatStop
}

func (ts *Teamserver) TsClientDisconnect(username string) {
	value, ok := ts.clients.GetDelete(username)
	if !ok {
		return
	}
	client := value.(*Client)

	client.Synced = false

	// Close channels to signal goroutines to stop
	close(client.HeartbeatStop)
	close(client.Send) // This will cause writePump to exit gracefully

	client.Socket.Close()

	ts.TsEventClient(false, username)

	var tunnels []string
	ts.tunnels.ForEach(func(key string, value interface{}) bool {
		tunnel := value.(*Tunnel)
		if tunnel.Data.Client == username {
			tunnels = append(tunnels, tunnel.Data.TunnelId)
		}
		return true
	})
	for _, id := range tunnels {
		_ = ts.TsTunnelStop(id)
	}
}

func (ts *Teamserver) TsClientSync(username string) {
	value, ok := ts.clients.Get(username)
	if !ok {
		return
	}
	client := value.(*Client)

	if !client.Synced {
		// 先发送所有存储的数据（TsSyncStored内部使用channel）
		ts.TsSyncStored(client)

		// 在标记为synced之前，发送tmp_store中的数据（避免丢失同步期间的新数据）
		// 注意：必须在设置synced=true之前发送，否则可能丢失新数据
		for {
			if client.TmpStore.Len() > 0 {
				arr := client.TmpStore.CutArray()

				for _, v := range arr {
					var buffer bytes.Buffer
					err := json.NewEncoder(&buffer).Encode(v)
					if err != nil {
						return
					}
					// Use channel for sending (Gorilla pattern)
					select {
					case client.Send <- buffer.Bytes():
						// Successfully queued message
					default:
						// Send channel is full - shouldn't happen during sync but handle it
						return
					}
				}
			} else {
				// 所有数据发送完成后，标记为synced
				// 这样后续的TsSyncAllClients调用就会直接发送，不会放入tmp_store
				client.Synced = true
				break
			}
		}
	}

	ts.TsEventClient(true, username)
}
