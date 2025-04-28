package server

import (
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	isvalid "AdaptixServer/core/utils/valid"
	"errors"
	"fmt"
	"github.com/Adaptix-Framework/axc2"
)

func (ts *Teamserver) TsListenerStart(listenerName string, listenerType string, listenerConfig string, listenerWatermark string, listenerCustomData []byte) error {

	if !ts.listener_configs.Contains(listenerType) {
		return fmt.Errorf("listener %v does not exist", listenerType)
	}
	if ts.listeners.Contains(listenerName) {
		return errors.New("listener already exists")
	}

	listenerData, customData, err := ts.Extender.ExListenerStart(listenerName, listenerType, listenerConfig, listenerCustomData)
	if err != nil {
		return err
	}

	listenerData.Name = listenerName
	listenerData.Type = listenerType
	listenerData.Data = listenerConfig
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

	ts.wm_listeners[listenerData.Watermark] = []string{listenerName, listenerType}

	packet := CreateSpListenerStart(listenerData)
	ts.TsSyncAllClients(packet)

	message := fmt.Sprintf("Listener '%v' (%v) started", listenerName, listenerType)
	packet2 := CreateSpEvent(EVENT_LISTENER_START, message)
	ts.TsSyncAllClients(packet2)
	ts.events.Put(packet2)

	_ = ts.DBMS.DbListenerInsert(listenerData, customData)

	return nil
}

func (ts *Teamserver) TsListenerEdit(listenerName string, listenerType string, listenerConfig string) error {
	if !ts.listener_configs.Contains(listenerType) {
		return fmt.Errorf("listener %v does not exist", listenerType)
	}
	if !ts.listeners.Contains(listenerName) {
		return fmt.Errorf("listener '%v' does not exist", listenerName)
	}

	listenerData, customData, err := ts.Extender.ExListenerEdit(listenerName, listenerType, listenerConfig)
	if err != nil {
		return err
	}

	listenerData.Name = listenerName
	listenerData.Type = listenerType
	listenerData.Data = listenerConfig

	ts.listeners.Put(listenerName, listenerData)

	packet := CreateSpListenerEdit(listenerData)
	ts.TsSyncAllClients(packet)

	message := fmt.Sprintf("Listener '%v' reconfigured", listenerName)
	packet2 := CreateSpEvent(EVENT_LISTENER_START, message)
	ts.TsSyncAllClients(packet2)
	ts.events.Put(packet2)

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

	message := fmt.Sprintf("Listener '%v' stopped", listenerName)
	packet2 := CreateSpEvent(EVENT_LISTENER_STOP, message)
	ts.TsSyncAllClients(packet2)
	ts.events.Put(packet2)

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
