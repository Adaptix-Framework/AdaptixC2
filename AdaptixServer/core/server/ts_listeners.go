package server

import (
	"AdaptixServer/core/extender"
	"encoding/json"
	"errors"
	"fmt"
)

func (ts *Teamserver) TsListenerReg(listenerInfo extender.ListenerInfo) error {

	listenerFN := fmt.Sprintf("%v/%v/%v", listenerInfo.Type, listenerInfo.ListenerProtocol, listenerInfo.ListenerName)

	if ts.listener_configs.Contains(listenerFN) {
		return fmt.Errorf("listener %v already exists", listenerFN)
	}

	ts.listener_configs.Put(listenerFN, listenerInfo)

	packet := CreateSpListenerReg(listenerFN, listenerInfo.ListenerUI)
	ts.TsSyncSavePacket(packet.store, packet)

	return nil
}

func (ts *Teamserver) TsListenerStart(listenerName string, listenerType string, listenerConfig string) error {
	var (
		err          error
		data         []byte
		listenerData ListenerData
	)

	if ts.listener_configs.Contains(listenerType) {

		if ts.listeners.Contains(listenerName) {
			return errors.New("listener already exists")
		}

		data, err = ts.Extender.ExListenerStart(listenerName, listenerType, listenerConfig)
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
		ts.TsSyncAllClients(packet)
		ts.TsSyncSavePacket(packet.store, packet)

		_ = ts.DBMS.DbListenerInsert(listenerName, listenerType, listenerConfig)
	} else {
		return fmt.Errorf("listener %v does not exist", listenerType)
	}

	return nil
}

func (ts *Teamserver) TsListenerStop(listenerName string, listenerType string) error {

	if ts.listener_configs.Contains(listenerType) {

		if ts.listeners.Contains(listenerName) {

			err := ts.Extender.ExListenerStop(listenerName, listenerType)
			if err != nil {
				return err
			}

			ts.listeners.Delete(listenerName)

			packet := CreateSpListenerStop(listenerName)
			ts.TsSyncAllClients(packet)
			ts.TsSyncXorPacket(packet.store, packet)

			_ = ts.DBMS.DbListenerDelete(listenerName)
		} else {
			return fmt.Errorf("listener '%v' does not exist", listenerName)
		}
	} else {
		return fmt.Errorf("listener %v does not exist", listenerType)
	}

	return nil
}

func (ts *Teamserver) TsListenerEdit(listenerName string, listenerType string, listenerConfig string) error {
	var (
		err          error
		data         []byte
		listenerData ListenerData
	)

	if ts.listener_configs.Contains(listenerType) {

		if ts.listeners.Contains(listenerName) {

			data, err = ts.Extender.ExListenerEdit(listenerName, listenerType, listenerConfig)
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
			ts.TsSyncAllClients(packet)
			ts.TsSyncSavePacket(packet.store, packet)

			_ = ts.DBMS.DbListenerUpdate(listenerName, listenerConfig)
		} else {
			return fmt.Errorf("listener '%v' does not exist", listenerName)
		}
	} else {
		return fmt.Errorf("listener %v does not exist", listenerType)
	}

	return nil
}
