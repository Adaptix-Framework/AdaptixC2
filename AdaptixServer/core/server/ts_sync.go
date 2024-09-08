package server

import (
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/safe"
	"bytes"
	"encoding/json"
	"github.com/gorilla/websocket"
)

func (ts *Teamserver) SyncClient(username string, packet interface{}) {
	var (
		buffer   bytes.Buffer
		err      error
		clientWS *websocket.Conn
	)

	err = json.NewEncoder(&buffer).Encode(packet)
	if err != nil {
		return
	}

	logs.Debug("SyncClient to '%v': %v\n", username, buffer.String())

	value, found := ts.clients.Get(username)
	if found {
		clientWS = value.(*websocket.Conn)
		err = clientWS.WriteMessage(websocket.BinaryMessage, buffer.Bytes())
		if err != nil {
			return
		}
	}
}

func (ts *Teamserver) SyncAllClients(packet interface{}) {
	var (
		buffer bytes.Buffer
		err    error
	)

	err = json.NewEncoder(&buffer).Encode(packet)
	if err != nil {
		return
	}

	logs.Debug("SyncAllClients: %v\n", buffer.String())

	ts.clients.ForEach(func(key string, value interface{}) {
		clientWS := value.(*websocket.Conn)
		_ = clientWS.WriteMessage(websocket.BinaryMessage, buffer.Bytes())
	})
}

func (ts *Teamserver) SyncSavePacket(store string, packet interface{}) {
	if ts.syncpackets.Contains(store) == false {
		ts.syncpackets.Put(store, safe.NewSlice())
	}

	value, found := ts.syncpackets.Get(store)
	if found {
		value.(*safe.Slice).Put(packet)
	}
}

func (ts *Teamserver) SyncStored(clientWS *websocket.Conn) {
	var (
		buffer bytes.Buffer
		packet interface{}
	)

	ts.syncpackets.DirectLock()
	mapPackets := ts.syncpackets.DirectMap()

	sumLen := 0
	for _, value := range mapPackets {
		sumLen += value.(*safe.Slice).Len()
	}

	if sumLen != 0 {
		packet = CreateSpSyncStart(sumLen)
		_ = json.NewEncoder(&buffer).Encode(packet)
		_ = clientWS.WriteMessage(websocket.BinaryMessage, buffer.Bytes())
		buffer.Reset()

		sliceLog := mapPackets[STORE_LOG].(*safe.Slice)
		for value := range sliceLog.Iterator() {
			_ = json.NewEncoder(&buffer).Encode(value)
			_ = clientWS.WriteMessage(websocket.BinaryMessage, buffer.Bytes())
			buffer.Reset()
		}

		for mKey, mValue := range mapPackets {
			if mKey != STORE_LOG {
				sliceVal := mValue.(*safe.Slice)
				for value := range sliceVal.Iterator() {
					_ = json.NewEncoder(&buffer).Encode(value)
					_ = clientWS.WriteMessage(websocket.BinaryMessage, buffer.Bytes())
					buffer.Reset()
				}
			}
		}

		packet = CreateSpSyncFinish()
		_ = json.NewEncoder(&buffer).Encode(packet)
		_ = clientWS.WriteMessage(websocket.BinaryMessage, buffer.Bytes())
		buffer.Reset()
	}

	ts.syncpackets.DirectUnlock()
}
