package server

import (
	"AdaptixServer/core/utils/safe"
	"bytes"
	"encoding/json"
	"fmt"
	"github.com/gorilla/websocket"
)

func (ts *Teamserver) TsClientConnect(username string, socket *websocket.Conn) {

	client := &Client{
		username:  username,
		synced:    false,
		socket:    socket,
		tmp_store: safe.NewSlice(),
	}

	ts.clients.Put(username, client)

	ts.TsSyncStored(socket)

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

	message := fmt.Sprintf("Client '%v' connected to teamserver", username)
	packet := CreateSpEvent(EVENT_CLIENT_CONNECT, message)
	ts.TsSyncAllClients(packet)
	ts.events.Put(packet)
}

func (ts *Teamserver) TsClientDisconnect(username string) {

	value, ok := ts.clients.GetDelete(username)
	if !ok {
		return
	}

	client := value.(*Client)

	client.synced = false
	client.socket.Close()

	message := fmt.Sprintf("Client '%v' disconnected from teamserver", username)
	packet := CreateSpEvent(EVENT_CLIENT_DISCONNECT, message)
	ts.TsSyncAllClients(packet)
	ts.events.Put(packet)
}
