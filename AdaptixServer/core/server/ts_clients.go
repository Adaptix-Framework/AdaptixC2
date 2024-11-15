package server

import (
	"github.com/gorilla/websocket"
)

func (ts *Teamserver) TsClientConnect(username string, socket *websocket.Conn) {

	ts.clients.Put(username, socket)

	ts.TsSyncStored(socket)

	packet := CreateSpClientConnect(username)
	ts.TsSyncSavePacket(packet.store, packet)
	ts.TsSyncAllClients(packet)
}

func (ts *Teamserver) TsClientDisconnect(username string) {

	value, ok := ts.clients.GetDelete(username)
	if !ok {
		return
	}

	value.(*websocket.Conn).Close()

	packet := CreateSpClientDisconnect(username)
	ts.TsSyncSavePacket(packet.store, packet)
	ts.TsSyncAllClients(packet)
}
