package server

import (
	"AdaptixServer/core/utils/safe"
	"bytes"
	"encoding/json"
	"sync"

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
		username:   username,
		synced:     false,
		lockSocket: &sync.Mutex{},
		socket:     socket,
		tmp_store:  safe.NewSlice(),
	}

	ts.clients.Put(username, client)
}

func (ts *Teamserver) TsClientDisconnect(username string) {
	value, ok := ts.clients.GetDelete(username)
	if !ok {
		return
	}
	client := value.(*Client)

	client.synced = false
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
	socket := client.socket

	if !client.synced {
		ts.TsSyncStored(client)

		for {
			if client.tmp_store.Len() > 0 {
				arr := client.tmp_store.CutArray()
				for _, v := range arr {
					var buffer bytes.Buffer
					_ = json.NewEncoder(&buffer).Encode(v)
					_ = socket.WriteMessage(websocket.BinaryMessage, buffer.Bytes())
				}
			} else {
				client.synced = true
				break
			}
		}
	}

	ts.TsEventClient(true, username)
}
