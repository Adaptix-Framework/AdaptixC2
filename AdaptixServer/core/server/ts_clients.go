package server

import (
	"errors"
	"github.com/gorilla/websocket"
)

func (ts *Teamserver) ClientConnect(username string, socket *websocket.Conn) {

	ts.clients.Put(username, socket)

	//TODO: Sync Data to Client

	//TODO: Broadcast LogEvent - UserConnected
}

func (ts *Teamserver) ClientDisconnect(username string) error {
	var (
		val    any
		ok     bool
		wsConn *websocket.Conn
		err    error
	)

	val, ok = ts.clients.GetDelete(username)
	if !ok {
		return errors.New("user not found")
	}

	wsConn = val.(*websocket.Conn)

	err = wsConn.Close()

	//TODO: Broadcast LogEvent - UserDisconnected

	return err
}
