package main

import (
	"errors"
	"github.com/Adaptix-Framework/axc2"
)

type Teamserver interface {
	TsAgentIsExists(agentId string) bool
	TsAgentCreate(agentCrc string, agentId string, beat []byte, listenerName string, ExternalIP string, Async bool) error
	TsAgentProcessData(agentId string, bodyData []byte) error
	TsAgentGetHostedTasksAll(agentId string, maxDataSize int) ([]byte, error)
	TsAgentGetHostedTasksOnly(agentId string, maxDataSize int) ([]byte, error)
	TsAgentGetHostedTasksTunnels(agentId string, channelId int, maxDataSize int) ([]byte, error)
	TsAgentSetMark(agentId string, makr string) error

	TsTaskRunningExists(agentId string, taskId string) bool
	TsTunnelChannelExists(channelId int) bool

	TsAgentTerminalCloseChannel(terminalId string, status string) error
	TsTerminalConnExists(terminalId string) bool
	TsTerminalConnData(terminalId string, data []byte)
	TsTerminalConnResume(agentId string, terminalId string)

	TsTunnelConnectionResume(AgentId string, channelId int)
	TsTunnelConnectionClose(channelId int)
	TsTunnelConnectionData(channelId int, data []byte)
}

type ModuleExtender struct {
	ts Teamserver
}

var (
	ModuleObject    *ModuleExtender
	ModuleDir       string
	ListenerDataDir string
	ListenersObject []any //*TCP
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

func (m *ModuleExtender) ListenerStart(name string, data string, listenerCustomData []byte) (adaptix.ListenerData, []byte, error) {
	listenerData, customData, listener, err := m.HandlerCreateListenerDataAndStart(name, data, listenerCustomData)
	if err != nil {
		return listenerData, customData, err
	}

	ListenersObject = append(ListenersObject, listener)

	return listenerData, customData, nil
}

func (m *ModuleExtender) ListenerEdit(name string, data string) (adaptix.ListenerData, []byte, error) {
	for _, value := range ListenersObject {
		listenerData, customData, ok := m.HandlerEditListenerData(name, value, data)
		if ok {
			return listenerData, customData, nil
		}
	}
	return adaptix.ListenerData{}, nil, errors.New("listener not found")
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
	for _, value := range ListenersObject {
		profile, ok := m.HandlerListenerGetProfile(name, value)
		if ok {
			return profile, nil
		}
	}
	return nil, errors.New("listener not found")
}

func (m *ModuleExtender) ListenerInteralHandler(name string, data []byte) (string, error) {
	return "", errors.New("listener not found")
}
