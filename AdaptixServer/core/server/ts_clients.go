package server

import (
	"AdaptixServer/core/utils/safe"
	"bytes"
	"encoding/json"
	"errors"
	"sync"
	"time"

	"github.com/gorilla/websocket"
)

func (ts *Teamserver) TsClientExists(username string) bool {
	return ts.clients.Contains(username)
}

func (ts *Teamserver) TsClientSocketMatch(username string, socket *websocket.Conn) bool {
	value, ok := ts.clients.Get(username)
	if !ok {
		return false
	}
	client := value.(*Client)
	return client.socket == socket
}

func (ts *Teamserver) TsClientConnect(username string, socket *websocket.Conn) {
	client := &Client{
		username:      username,
		synced:        false,
		lockSocket:    &sync.Mutex{},
		socket:        socket,
		tmp_store:     safe.NewSlice(),
		heartbeatStop: make(chan struct{}),
	}

	ts.clients.Put(username, client)
}

func (ts *Teamserver) TsClientWriteControl(username string, messageType int, data []byte) error {
	value, ok := ts.clients.Get(username)
	if !ok {
		return errors.New("client not found")
	}

	client := value.(*Client)
	client.lockSocket.Lock()
	defer client.lockSocket.Unlock()

	deadline := time.Now().Add(5 * time.Second)
	if err := client.socket.SetWriteDeadline(deadline); err != nil {
		return err
	}

	return client.socket.WriteControl(messageType, data, deadline)
}

func (ts *Teamserver) TsClientHeartbeatStop(username string) <-chan struct{} {
	value, ok := ts.clients.Get(username)
	if !ok {
		return nil
	}
	client := value.(*Client)
	return client.heartbeatStop
}

func (ts *Teamserver) TsClientDisconnect(username string) {
	value, ok := ts.clients.GetDelete(username)
	if !ok {
		return
	}
	client := value.(*Client)

	client.synced = false
	close(client.heartbeatStop)
	client.socket.Close()

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

	if !client.synced {
		// 先发送所有存储的数据（TsSyncStored内部会加锁）
		ts.TsSyncStored(client)

		// 在标记为synced之前，发送tmp_store中的数据（避免丢失同步期间的新数据）
		// 注意：必须在设置synced=true之前发送，否则可能丢失新数据
		for {
			if client.tmp_store.Len() > 0 {
				arr := client.tmp_store.CutArray()

				client.lockSocket.Lock()
				for _, v := range arr {
					var buffer bytes.Buffer
					err := json.NewEncoder(&buffer).Encode(v)
					if err != nil {
						client.lockSocket.Unlock()
						return
					}
					err = client.socket.WriteMessage(websocket.BinaryMessage, buffer.Bytes())
					if err != nil {
						client.lockSocket.Unlock()
						return
					}
				}
				client.lockSocket.Unlock()
			} else {
				// 所有数据发送完成后，标记为synced
				// 这样后续的TsSyncAllClients调用就会直接发送，不会放入tmp_store
				client.synced = true
				break
			}
		}
	}

	ts.TsEventClient(true, username)
}
