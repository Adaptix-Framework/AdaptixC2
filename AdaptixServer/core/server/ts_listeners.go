package server

import (
	"AdaptixServer/core/eventing"
	"AdaptixServer/core/extender"
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	isvalid "AdaptixServer/core/utils/valid"
	"encoding/json"
	"errors"
	"fmt"
	"time"

	"github.com/Adaptix-Framework/axc2"
)

func (ts *Teamserver) TsListenerList() (string, error) {
	var listeners []adaptix.ListenerData
	ts.listeners.ForEach(func(key string, value interface{}) bool {
		listeners = append(listeners, value.(adaptix.ListenerData))
		return true
	})

	jsonListeners, err := json.Marshal(listeners)
	if err != nil {
		return "", err
	}
	return string(jsonListeners), nil
}

func (ts *Teamserver) TsListenerStart(listenerName string, listenerRegName string, listenerConfig string, createTime int64, listenerWatermark string, listenerCustomData []byte) error {
	// --- PRE HOOK ---
	preEvent := &eventing.EventDataListenerStart{
		ListenerName: listenerName,
		ListenerType: listenerRegName,
		Config:       listenerConfig,
	}
	if !ts.EventManager.Emit(eventing.EventListenerStart, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	// ----------------

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
	if createTime == 0 {
		listenerData.CreateTime = time.Now().Unix()
	} else {
		listenerData.CreateTime = createTime
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

	ts.TsNotifyListenerStart(false, listenerName, listenerRegName)

	_ = ts.DBMS.DbListenerInsert(listenerData, customData)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataListenerStart{
		ListenerName: listenerName,
		ListenerType: listenerRegName,
		Config:       listenerConfig,
	}
	ts.EventManager.EmitAsync(eventing.EventListenerStart, postEvent)
	// -----------------

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

	listenerData, customData, err := ts.Extender.ExListenerEdit(listenerName, listenerConfig)
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

	ts.TsNotifyListenerStart(true, listenerName, listenerRegName)

	_ = ts.DBMS.DbListenerUpdate(listenerName, listenerConfig, customData)

	return nil
}

func (ts *Teamserver) TsListenerStop(listenerName string, listenerType string) error {
	// --- PRE HOOK ---
	preEvent := &eventing.EventDataListenerStop{
		ListenerName: listenerName,
		ListenerType: listenerType,
	}
	if !ts.EventManager.Emit(eventing.EventListenerStop, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	// ----------------

	if !ts.listener_configs.Contains(listenerType) {
		return fmt.Errorf("listener %v does not exist", listenerType)
	}
	if !ts.listeners.Contains(listenerName) {
		return fmt.Errorf("listener '%v' does not exist", listenerName)
	}

	err := ts.Extender.ExListenerStop(listenerName)
	if err != nil {
		return err
	}

	ts.listeners.Delete(listenerName)

	packet := CreateSpListenerStop(listenerName)
	ts.TsSyncAllClients(packet)

	ts.TsNotifyListenerStop(listenerName, listenerType)

	_ = ts.DBMS.DbListenerDelete(listenerName)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataListenerStop{
		ListenerName: listenerName,
		ListenerType: listenerType,
	}
	ts.EventManager.EmitAsync(eventing.EventListenerStop, postEvent)
	// -----------------

	return nil
}

func (ts *Teamserver) TsListenerPause(listenerName string, listenerType string) error {
	// --- PRE HOOK ---
	preEvent := &eventing.EventDataListenerStop{
		ListenerName: listenerName,
		ListenerType: listenerType,
	}
	if !ts.EventManager.Emit(eventing.EventListenerStop, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	// ----------------

	if !ts.listener_configs.Contains(listenerType) {
		return fmt.Errorf("listener %v does not exist", listenerType)
	}
	if !ts.listeners.Contains(listenerName) {
		return fmt.Errorf("listener '%v' does not exist", listenerName)
	}

	value, _ := ts.listeners.Get(listenerName)
	listenerData := value.(adaptix.ListenerData)
	if listenerData.Status == "Paused" {
		return fmt.Errorf("listener '%v' is already paused", listenerName)
	}

	err := ts.Extender.ExListenerPause(listenerName)
	if err != nil {
		return err
	}

	listenerData.Status = "Paused"
	ts.listeners.Put(listenerName, listenerData)
	_ = ts.DBMS.DbListenerUpdateStatus(listenerName, "Paused")

	packet := CreateSpListenerEdit(listenerData)
	ts.TsSyncAllClients(packet)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataListenerStop{
		ListenerName: listenerName,
		ListenerType: listenerType,
	}
	ts.EventManager.EmitAsync(eventing.EventListenerStop, postEvent)
	// -----------------

	return nil
}

func (ts *Teamserver) TsListenerResume(listenerName string, listenerType string) error {
	// --- PRE HOOK ---
	preEvent := &eventing.EventDataListenerStart{
		ListenerName: listenerName,
		ListenerType: listenerType,
	}
	if !ts.EventManager.Emit(eventing.EventListenerStart, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	// ----------------

	if !ts.listener_configs.Contains(listenerType) {
		return fmt.Errorf("listener %v does not exist", listenerType)
	}
	if !ts.listeners.Contains(listenerName) {
		return fmt.Errorf("listener '%v' does not exist", listenerName)
	}

	value, _ := ts.listeners.Get(listenerName)
	listenerData := value.(adaptix.ListenerData)
	if listenerData.Status == "Listen" {
		return fmt.Errorf("listener '%v' is already running", listenerName)
	}

	err := ts.Extender.ExListenerResume(listenerName)
	if err != nil {
		return err
	}

	listenerData.Status = "Listen"
	ts.listeners.Put(listenerName, listenerData)
	_ = ts.DBMS.DbListenerUpdateStatus(listenerName, "Listen")

	packet := CreateSpListenerEdit(listenerData)
	ts.TsSyncAllClients(packet)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataListenerStart{
		ListenerName: listenerName,
		ListenerType: listenerType,
	}
	ts.EventManager.EmitAsync(eventing.EventListenerStart, postEvent)
	// -----------------

	return nil
}

func (ts *Teamserver) TsListenerGetProfile(listenerName string) (string, []byte, error) {
	if !ts.listeners.Contains(listenerName) {
		return "", nil, fmt.Errorf("listener %v does not exist", listenerName)
	}
	value, _ := ts.listeners.Get(listenerName)
	watermark := value.(adaptix.ListenerData).Watermark
	data, err := ts.Extender.ExListenerGetProfile(listenerName)
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

	return ts.Extender.ExListenerInternalHandler(listenerName, data)
}
