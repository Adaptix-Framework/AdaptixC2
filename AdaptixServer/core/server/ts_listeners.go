package server

import (
	"AdaptixServer/core/extender"
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	isvalid "AdaptixServer/core/utils/valid"
	"errors"
	"fmt"
	"github.com/Adaptix-Framework/axc2"
)

func (ts *Teamserver) TsListenerStart(listenerName string, listenerRegName string, listenerConfig string, listenerWatermark string, listenerCustomData []byte) error {
	value, ok := ts.listener_configs.Get(listenerRegName)
	if !ok {
		return fmt.Errorf("listener %v does not register", listenerRegName)
	}
	listenerInfo, _ := value.(extender.ListenerInfo)

	if ts.listeners.Contains(listenerName) {
		return errors.New("listener already exists")
	}

	listenerData, customData, err := ts.Extender.ExListenerStart(listenerName, listenerRegName, listenerConfig, listenerCustomData)
	if err != nil {
		return err
	}

	listenerData.Name = listenerName
	listenerData.RegName = listenerRegName
	listenerData.Data = listenerConfig
	listenerData.Type = listenerInfo.Type
	if listenerData.Protocol == "" {
		listenerData.Protocol = listenerInfo.Protocol
	}
	if listenerData.Watermark == "" {
		listenerData.Watermark = listenerWatermark
	}

	if !isvalid.ValidHex8(listenerData.Watermark) {
		if listenerData.Watermark != "" {
			logs.Error("", "Listener %s is invalid watermark. Set random...", listenerName)
		}
		listenerData.Watermark, _ = krypt.GenerateUID(8)
	}

	ts.listeners.Put(listenerName, listenerData)

	ts.wm_listeners[listenerData.Watermark] = []string{listenerName, listenerRegName}

	packet := CreateSpListenerStart(listenerData)
	ts.TsSyncAllClients(packet)

	ts.TsEventListenerStart(false, listenerName, listenerRegName)

	_ = ts.DBMS.DbListenerInsert(listenerData, customData)

	return nil
}

func (ts *Teamserver) TsListenerEdit(listenerName string, listenerRegName string, listenerConfig string) error {
	value, ok := ts.listener_configs.Get(listenerRegName)
	if !ok {
		return fmt.Errorf("listener %v does not register", listenerRegName)
	}
	listenerInfo, _ := value.(extender.ListenerInfo)

	if !ts.listeners.Contains(listenerName) {
		return fmt.Errorf("listener '%v' does not exist", listenerName)
	}

	listenerData, customData, err := ts.Extender.ExListenerEdit(listenerName, listenerRegName, listenerConfig)
	if err != nil {
		return err
	}

	listenerData.Name = listenerName
	listenerData.RegName = listenerRegName
	listenerData.Data = listenerConfig
	listenerData.Type = listenerInfo.Type
	if listenerData.Protocol == "" {
		listenerData.Protocol = listenerInfo.Protocol
	}

	ts.listeners.Put(listenerName, listenerData)

	packet := CreateSpListenerEdit(listenerData)
	ts.TsSyncAllClients(packet)

	ts.TsEventListenerStart(true, listenerName, listenerRegName)

	_ = ts.DBMS.DbListenerUpdate(listenerName, listenerConfig, customData)

	return nil
}

func (ts *Teamserver) TsListenerStop(listenerName string, listenerType string) error {
	if !ts.listener_configs.Contains(listenerType) {
		return fmt.Errorf("listener %v does not exist", listenerType)
	}
	if !ts.listeners.Contains(listenerName) {
		return fmt.Errorf("listener '%v' does not exist", listenerName)
	}

	err := ts.Extender.ExListenerStop(listenerName, listenerType)
	if err != nil {
		return err
	}

	ts.listeners.Delete(listenerName)

	packet := CreateSpListenerStop(listenerName)
	ts.TsSyncAllClients(packet)

	ts.TsEventListenerStop(listenerName, listenerType)

	_ = ts.DBMS.DbListenerDelete(listenerName)

	return nil
}

func (ts *Teamserver) TsListenerGetProfile(listenerName string, listenerType string) (string, []byte, error) {
	if !ts.listener_configs.Contains(listenerType) {
		return "", nil, fmt.Errorf("listener '%v' does not exist", listenerType)
	}
	if !ts.listeners.Contains(listenerName) {
		return "", nil, fmt.Errorf("listener %v does not exist", listenerName)
	}

	value, _ := ts.listeners.Get(listenerName)
	watermark := value.(adaptix.ListenerData).Watermark
	data, err := ts.Extender.ExListenerGetProfile(listenerName, listenerType)
	return watermark, data, err
}

func (ts *Teamserver) TsListenerInteralHandler(watermark string, data []byte) (string, error) {
	pair, ok := ts.wm_listeners[watermark]
	if !ok {
		return "", fmt.Errorf("listener %v does not exist", watermark)
	}

	listenerName := pair[0]
	listenerType := pair[1]

	if !ts.listener_configs.Contains(listenerType) {
		return "", fmt.Errorf("listener %v does not exist", listenerType)
	}
	if !ts.listeners.Contains(listenerName) {
		return "", fmt.Errorf("listener '%v' does not exist", listenerName)
	}

	return ts.Extender.ExListenerInteralHandler(listenerName, listenerType, data)
}
