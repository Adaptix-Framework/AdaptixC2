package server

import (
	"AdaptixServer/core/extender"
	"encoding/json"
	"errors"
	"fmt"
)

func (ts *Teamserver) ListenerReg(listenerInfo extender.ListenerInfo) error {

	listenerFN := fmt.Sprintf("%v/%v/%v", listenerInfo.ListenerType, listenerInfo.ListenerProtocol, listenerInfo.ListenerName)

	if ts.listener_configs.Contains(listenerFN) {
		return fmt.Errorf("listener %v already exists", listenerFN)
	}

	ts.listener_configs.Put(listenerFN, listenerInfo)

	packet := CreateSpListenerReg(listenerFN, listenerInfo.ListenerUI)
	ts.SyncSavePacket(packet.store, packet)

	return nil
}

func (ts *Teamserver) ListenerStart(listenerName string, listenerType string, listenerConfig string) error {
	var (
		err          error
		data         []byte
		listenerData ListenerData
	)

	if ts.listener_configs.Contains(listenerType) {

		if ts.listeners.Contains(listenerName) {
			return errors.New("listener already exists")
		}

		data, err = ts.Extender.ListenerStart(listenerName, listenerType, listenerConfig)
		if err != nil {
			return err
		}

		err = json.Unmarshal(data, &listenerData)
		if err != nil {
			return err
		}

		listenerData.Name = listenerName
		listenerData.Type = listenerType
		listenerData.Data = listenerConfig

		ts.listeners.Put(listenerName, listenerData)

		packet := CreateSpListenerStart(listenerData)
		ts.SyncAllClients(packet)
		ts.SyncSavePacket(packet.store, packet)

		_ = ts.DBMS.ListenerInsert(listenerName, listenerType, listenerConfig)
	} else {
		return fmt.Errorf("listener %v does not exist", listenerType)
	}

	return nil
}

func (ts *Teamserver) ListenerStop(listenerName string, listenerType string) error {

	if ts.listener_configs.Contains(listenerType) {

		if ts.listeners.Contains(listenerName) {

			err := ts.Extender.ListenerStop(listenerName, listenerType)
			if err != nil {
				return err
			}

			ts.listeners.Delete(listenerName)

			packet := CreateSpListenerStop(listenerName)
			ts.SyncAllClients(packet)
			ts.SyncXorPacket(packet.store, packet)

			_ = ts.DBMS.ListenerDelete(listenerName)
		} else {
			return fmt.Errorf("listener '%v' does not exist", listenerName)
		}
	} else {
		return fmt.Errorf("listener %v does not exist", listenerType)
	}

	return nil
}

func (ts *Teamserver) ListenerEdit(listenerName string, listenerType string, listenerConfig string) error {
	var (
		err          error
		data         []byte
		listenerData ListenerData
	)

	if ts.listener_configs.Contains(listenerType) {

		if ts.listeners.Contains(listenerName) {

			data, err = ts.Extender.ListenerEdit(listenerName, listenerType, listenerConfig)
			if err != nil {
				return err
			}

			err = json.Unmarshal(data, &listenerData)
			if err != nil {
				return err
			}

			listenerData.Name = listenerName
			listenerData.Type = listenerType
			listenerData.Data = listenerConfig

			ts.listeners.Put(listenerName, listenerData)

			packet := CreateSpListenerEdit(listenerData)
			ts.SyncAllClients(packet)
			ts.SyncSavePacket(packet.store, packet)

			_ = ts.DBMS.ListenerUpdate(listenerName, listenerConfig)
		} else {
			return fmt.Errorf("listener '%v' does not exist", listenerName)
		}
	} else {
		return fmt.Errorf("listener %v does not exist", listenerType)
	}

	return nil
}
