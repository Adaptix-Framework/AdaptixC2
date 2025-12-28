package main

import (
	"errors"
	"slices"

	"github.com/Adaptix-Framework/axc2"
)

// =============================================================================
// Teamserver Interface
// =============================================================================

type Teamserver interface {
	TsAgentIsExists(agentId string) bool
	TsAgentCreate(agentCrc string, agentId string, beat []byte, listenerName string, ExternalIP string, Async bool) (adaptix.AgentData, error)
	TsAgentProcessData(agentId string, bodyData []byte) error
	TsAgentSetTick(agentId string) error
	TsAgentGetHostedAll(agentId string, maxDataSize int) ([]byte, error)
}

// =============================================================================
// Module
// =============================================================================

type ModuleExtender struct {
	ts Teamserver
}

var (
	ModuleObject    *ModuleExtender
	ModuleDir       string
	ListenerDataDir string
	ListenersObject []*DNSListener
)

// =============================================================================
// Plugin Entry Point
// =============================================================================

func InitPlugin(ts any, moduleDir string, listenerDir string) any {
	ModuleDir = moduleDir
	ListenerDataDir = listenerDir

	ModuleObject = new(ModuleExtender)
	ModuleObject.ts = ts.(Teamserver)

	return ModuleObject
}

// =============================================================================
// Listener Management
// =============================================================================

func (m *ModuleExtender) ListenerValid(data string) error {
	return m.HandlerListenerValid(data)
}

func (m *ModuleExtender) ListenerStart(name string, data string, listenerCustomData []byte) (adaptix.ListenerData, []byte, error) {
	listenerData, customData, listener, err := m.HandlerCreateListenerDataAndStart(name, data, listenerCustomData)
	if err != nil {
		return listenerData, customData, err
	}

	if l, ok := listener.(*DNSListener); ok {
		ListenersObject = append(ListenersObject, l)
	}

	return listenerData, customData, nil
}

func (m *ModuleExtender) ListenerEdit(name string, data string) (adaptix.ListenerData, []byte, error) {
	for _, listener := range ListenersObject {
		listenerData, customData, ok := m.HandlerEditListenerData(name, listener, data)
		if ok {
			return listenerData, customData, nil
		}
	}
	return adaptix.ListenerData{}, nil, errors.New("listener not found")
}

func (m *ModuleExtender) ListenerStop(name string) error {
	idx := slices.IndexFunc(ListenersObject, func(l *DNSListener) bool {
		return l.Name == name
	})

	if idx == -1 {
		return errors.New("listener not found")
	}

	err := ListenersObject[idx].Stop()
	ListenersObject = slices.Delete(ListenersObject, idx, idx+1)

	return err
}

func (m *ModuleExtender) ListenerGetProfile(name string) ([]byte, error) {
	for _, listener := range ListenersObject {
		profile, ok := m.HandlerListenerGetProfile(name, listener)
		if ok {
			return profile, nil
		}
	}
	return nil, errors.New("listener not found")
}

func (m *ModuleExtender) ListenerInteralHandler(name string, data []byte) (string, error) {
	_ = name
	_ = data
	return "", errors.New("listener not found")
}
