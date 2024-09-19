package server

import (
	"AdaptixServer/core/extender"
	"errors"
	"fmt"
)

func (ts *Teamserver) ListenerNew(listenerInfo extender.ListenerInfo) error {

	listenerFN := fmt.Sprintf("%v/%v/%v", listenerInfo.ListenerType, listenerInfo.ListenerProtocol, listenerInfo.ListenerName)

	if ts.listener_configs.Contains(listenerFN) {
		return fmt.Errorf("listener %v already exists", listenerFN)
	}

	ts.listener_configs.Put(listenerFN, listenerInfo)

	packet := CreateSpListenerNew(listenerFN, listenerInfo.ListenerUI)
	ts.SyncSavePacket(packet.store, packet)
	//ts.SyncAllClients(packet)

	return nil
}

func (ts *Teamserver) ListenerStart(listenerName string, configType string, config string) error {

	if ts.listener_configs.Contains(configType) {

		if ts.listeners.Contains(listenerName) {
			return errors.New("listener already exists")
		}

		err := ts.Extender.ListenerStart(listenerName, configType, config)
		if err != nil {
			return err
		}

	} else {
		return fmt.Errorf("listener %v does not exist", configType)
	}

	return nil
}
