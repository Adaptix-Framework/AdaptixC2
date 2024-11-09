package server

import (
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

func (ts *Teamserver) SyncXorPacket(store string, packet interface{}) {
	if ts.syncpackets.Contains(store) {
		value, found := ts.syncpackets.Get(store)
		if found {
			slice := value.(*safe.Slice)
			for value := range slice.Iterator() {

				storedValue, storedOk := value.Item.(SyncPackerListenerStart)
				packetValue, packetOk := packet.(SyncPackerListenerStop)
				if storedOk && packetOk {
					if storedValue.ListenerName == packetValue.ListenerName {
						slice.Delete(value.Index)
					}
				}
			}
		}
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

		if ts.syncpackets.Contains(STORE_INIT) {
			sliceInit := mapPackets[STORE_INIT].(*safe.Slice)
			for value := range sliceInit.Iterator() {
				_ = json.NewEncoder(&buffer).Encode(value.Item)
				_ = clientWS.WriteMessage(websocket.BinaryMessage, buffer.Bytes())
				buffer.Reset()
			}
		}

		if ts.syncpackets.Contains(STORE_LOG) {
			sliceLog := mapPackets[STORE_LOG].(*safe.Slice)
			for value := range sliceLog.Iterator() {
				_ = json.NewEncoder(&buffer).Encode(value.Item)
				_ = clientWS.WriteMessage(websocket.BinaryMessage, buffer.Bytes())
				buffer.Reset()
			}
		}
		for mKey, mValue := range mapPackets {
			if mKey != STORE_LOG && mKey != STORE_INIT {
				sliceVal := mValue.(*safe.Slice)
				for value := range sliceVal.Iterator() {
					_ = json.NewEncoder(&buffer).Encode(value.Item)
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
