package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
	"runtime"
)

type (
	PluginType   string
	ListenerType string
)

const (
	LISTENER PluginType = "listener"

	INTERNAL ListenerType = "internal"
	EXTERNAL ListenerType = "external"
)

type Teamserver interface {
	TsAgentRequest(agentType string, agentId string, beat []byte, bodyData []byte, listenerName string, ExternalIP string) ([]byte, error)
}

type ModuleExtender struct {
	ts Teamserver
}

type ModuleInfo struct {
	ModuleName string
	ModuleType PluginType
}

type ListenerInfo struct {
	Type             ListenerType
	ListenerProtocol string
	ListenerName     string
	ListenerUI       string
}

type ListenerData struct {
	Name      string `json:"l_name"`
	Type      string `json:"l_type"`
	BindHost  string `json:"l_bind_host"`
	BindPort  string `json:"l_bind_port"`
	AgentHost string `json:"l_agent_host"`
	AgentPort string `json:"l_agent_port"`
	Status    string `json:"l_status"`
	Data      string `json:"l_data"`
}

var ModuleObject ModuleExtender
var ModulePath string

func (m *ModuleExtender) InitPlugin(ts any) ([]byte, error) {
	var (
		buffer bytes.Buffer
		err    error
	)

	ModuleObject.ts = ts.(Teamserver)

	info := ModuleInfo{
		ModuleName: SetName,
		ModuleType: LISTENER,
	}

	err = json.NewEncoder(&buffer).Encode(info)
	if err != nil {
		return nil, err
	}

	return buffer.Bytes(), nil
}

func (m *ModuleExtender) ListenerInit(path string) ([]byte, error) {
	var (
		buffer bytes.Buffer
		err    error
	)

	ModulePath = path

	_, file, _, _ := runtime.Caller(0)
	dir := filepath.Dir(file)
	uiPath := filepath.Join(dir, SetUiPath)
	listenerUI, err := os.ReadFile(uiPath)
	if err != nil {
		return nil, err
	}

	info := ListenerInfo{
		Type:             SetType,
		ListenerProtocol: SetProtocol,
		ListenerName:     SetName,
		ListenerUI:       string(listenerUI),
	}

	err = json.NewEncoder(&buffer).Encode(info)
	if err != nil {
		return nil, err
	}

	return buffer.Bytes(), nil
}

func (m *ModuleExtender) ListenerValid(data string) error {
	return ValidateListenerConfig(data)
}

func (m *ModuleExtender) ListenerStart(name string, data string) ([]byte, error) {
	var (
		err          error
		listenerData ListenerData
		buffer       bytes.Buffer
		listener     any
	)

	listenerData, listener, err = CreateListenerDataAndStart(name, data)

	err = json.NewEncoder(&buffer).Encode(listenerData)
	if err != nil {
		return nil, err
	}

	ListenersObject = append(ListenersObject, listener)

	return buffer.Bytes(), nil
}

func (m *ModuleExtender) ListenerEdit(name string, data string) ([]byte, error) {
	var (
		err          error
		buffer       bytes.Buffer
		listenerData ListenerData
		ok           bool
	)

	for _, value := range ListenersObject {

		listenerData, ok = EditListenerData(name, value, data)
		if ok {
			err = json.NewEncoder(&buffer).Encode(listenerData)
			if err != nil {
				return nil, err
			}
			return buffer.Bytes(), nil
		}
	}
	return nil, errors.New("listener not found")
}

func (m *ModuleExtender) ListenerStop(name string) error {
	var (
		index int
		err   error
		ok    bool
	)

	for ind, value := range ListenersObject {
		ok, err = StopListener(name, value)
		if ok {
			index = ind
			break
		}
	}

	if ok {
		ListenersObject = append(ListenersObject[:index], ListenersObject[index+1:]...)
	} else {
		return errors.New("listener not found")
	}

	return err
}

func (m *ModuleExtender) ListenerGetProfile(name string) ([]byte, error) {
	var (
		profile []byte
		ok      bool
	)

	for _, value := range ListenersObject {

		profile, ok = GetProfile(name, value)
		if ok {
			return profile, nil
		}

	}
	return nil, errors.New("listener not found")
}
