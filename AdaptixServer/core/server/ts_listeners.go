package server

import (
	"AdaptixServer/core/extender"
	"fmt"
)

func (ts *Teamserver) ListenerNew(listenerInfo extender.ListenerInfo) error {

	listenerFN := fmt.Sprintf("%v/%v/%v", listenerInfo.ListenerType, listenerInfo.ListenerProtocol, listenerInfo.ListenerName)

	if ts.ex_listeners.Contains(listenerFN) {
		return fmt.Errorf("listener %v already exists", listenerFN)
	}

	ts.ex_listeners.Put(listenerFN, listenerInfo)

	packet := CreateSpListenerNew(listenerFN, listenerInfo.ListenerUI)
	ts.SyncSavePacket(packet.store, packet)
	//ts.SyncAllClients(packet)

	return nil
}
