package server

import (
	"AdaptixServer/core/extender"
	"encoding/json"
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

func (ts *Teamserver) ListenerStart(name string, configType string, config string) error {

	var (
		err          error
		data         []byte
		listenerData ListenerData
	)

	if ts.listener_configs.Contains(configType) {

		if ts.listeners.Contains(name) {
			return errors.New("listener already exists")
		}

		data, err = ts.Extender.ListenerStart(name, configType, config)
		if err != nil {
			return err
		}

		err = json.Unmarshal(data, &listenerData)
		if err != nil {
			return err
		}

		listenerData.Name = name
		listenerData.Type = configType

		ts.listeners.Put(name, listenerData)

		packet := CreateSpListenerStart(listenerData)
		ts.SyncSavePacket(packet.store, packet)
		ts.SyncAllClients(packet)

	} else {
		return fmt.Errorf("listener %v does not exist", configType)
	}

	return nil
}

func (ts *Teamserver) ListenerStop(listenerName string, configType string) error {

	if ts.listener_configs.Contains(configType) {

		if ts.listeners.Contains(listenerName) {

			err := ts.Extender.ListenerStop(listenerName, configType)
			if err != nil {
				return err
			}

			ts.listeners.Delete(listenerName)

			packet := CreateSpListenerStop(listenerName)
			//ts.SyncSavePacket(packet.store, packet)
			ts.SyncAllClients(packet)

		} else {
			return fmt.Errorf("listener '%v' does not exist", listenerName)
		}
	} else {
		return fmt.Errorf("listener %v does not exist", configType)
	}

	return nil
}
