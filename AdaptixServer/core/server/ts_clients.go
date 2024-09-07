package server

import (
	"github.com/gorilla/websocket"
)

func (ts *Teamserver) ClientConnect(username string, socket *websocket.Conn) {

	ts.clients.Put(username, socket)

	ts.SyncStored(socket)

	packet := CreateSpClientConnect(username)
	ts.SyncSavePacket(packet.store, packet)
	ts.SyncAllClients(packet)
}

func (ts *Teamserver) ClientDisconnect(username string) {

	value, ok := ts.clients.GetDelete(username)
	if !ok {
		return
	}

	value.(*websocket.Conn).Close()

	packet := CreateSpClientDisconnect(username)
	ts.SyncSavePacket(packet.store, packet)
	ts.SyncAllClients(packet)
}
