package main

import (
	"bytes"
	"encoding/json"
	"errors"
)

type Teamserver interface {
	TsAgentIsExists(agentId string) bool
	TsAgentCreate(agentCrc string, agentId string, beat []byte, listenerName string, ExternalIP string, Async bool) error
	TsAgentProcessData(agentId string, bodyData []byte) error
	TsAgentGetHostedTasks(agentId string, maxDataSize int) ([]byte, error)
}

type ModuleExtender struct {
	ts Teamserver
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
	Watermark string `json:"l_watermark"`
}

/// Module

var (
	ModuleObject    *ModuleExtender
	ModuleDir       string
	ListenerDataDir string
	ListenersObject []any //*HTTP
)

func InitPlugin(ts any, moduleDir string, listenerDir string) any {
	ModuleDir = moduleDir
	ListenerDataDir = listenerDir

	ModuleObject = &ModuleExtender{
		ts: ts.(Teamserver),
	}
	return ModuleObject
}

func (m *ModuleExtender) ListenerValid(data string) error {
	return m.HandlerListenerValid(data)
}

func (m *ModuleExtender) ListenerStart(name string, data string, listenerCustomData []byte) ([]byte, []byte, error) {
	var (
		err          error
		listenerData ListenerData
		customData   []byte
		buffer       bytes.Buffer
		listener     any
	)

	listenerData, customData, listener, err = m.HandlerCreateListenerDataAndStart(name, data, listenerCustomData)

	err = json.NewEncoder(&buffer).Encode(listenerData)
	if err != nil {
		return nil, customData, err
	}

	ListenersObject = append(ListenersObject, listener)

	return buffer.Bytes(), customData, nil
}

func (m *ModuleExtender) ListenerEdit(name string, data string) ([]byte, []byte, error) {
	var (
		err          error
		buffer       bytes.Buffer
		listenerData ListenerData
		customData   []byte
		ok           bool
	)

	for _, value := range ListenersObject {

		listenerData, customData, ok = m.HandlerEditListenerData(name, value, data)
		if ok {
			err = json.NewEncoder(&buffer).Encode(listenerData)
			if err != nil {
				return nil, nil, err
			}
			return buffer.Bytes(), customData, nil
		}
	}
	return nil, nil, errors.New("listener not found")
}

func (m *ModuleExtender) ListenerStop(name string) error {
	var (
		index int
		err   error
		ok    bool
	)

	for ind, value := range ListenersObject {
		ok, err = m.HandlerListenerStop(name, value)
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

		profile, ok = m.HandlerListenerGetProfile(name, value)
		if ok {
			return profile, nil
		}

	}
	return nil, errors.New("listener not found")
}

func (m *ModuleExtender) ListenerInteralHandler(name string, data []byte) (string, error) {
	return "", errors.New("listener not found")
}
