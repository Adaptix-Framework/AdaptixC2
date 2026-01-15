package extender

import (
	"errors"

	adaptix "github.com/Adaptix-Framework/axc2"
)

var (
	ErrModuleNotFound   = errors.New("module not found")
	ErrListenerNotFound = errors.New("listener not found")
)

/// ExConfig Listener

type ExConfigListener struct {
	ExtenderType string `json:"extender_type"`
	ExtenderFile string `json:"extender_file"`
	AxFile       string `json:"ax_file"`
	ListenerName string `json:"listener_name"`
	ListenerType string `json:"listener_type"`
	Protocol     string `json:"protocol"`
}

/// ExConfig Agent

type ExConfigAgent struct {
	ExtenderType   string   `json:"extender_type"`
	ExtenderFile   string   `json:"extender_file"`
	AxFile         string   `json:"ax_file"`
	AgentName      string   `json:"agent_name"`
	AgentWatermark string   `json:"agent_watermark"`
	Listeners      []string `json:"listeners"`
	MultiListeners bool     `json:"multi_listeners"`
}

/// Info

type ListenerInfo struct {
	Name     string
	Protocol string
	Type     string
	AX       string
}

type AgentInfo struct {
	Name           string
	Watermark      string
	AX             string
	Listeners      []string
	MultiListeners bool
}

type Teamserver interface {
	TsListenerReg(listenerInfo ListenerInfo) error
	TsListenerRegByName(listenerName string) (string, error)
	TsAgentReg(agentInfo AgentInfo) error
}

type AdaptixExtender struct {
	ts              Teamserver
	listenerModules map[string]adaptix.PluginListener
	agentModules    map[string]adaptix.PluginAgent
	activeListeners map[string]adaptix.ExtenderListener
}

/// Helper methods

func (ex *AdaptixExtender) getListenerModule(configType string) (adaptix.PluginListener, error) {
	module, ok := ex.listenerModules[configType]
	if !ok {
		return nil, ErrModuleNotFound
	}
	return module, nil
}

func (ex *AdaptixExtender) getActiveListener(name string) (adaptix.ExtenderListener, error) {
	listener, ok := ex.activeListeners[name]
	if !ok {
		return nil, ErrListenerNotFound
	}
	return listener, nil
}

func (ex *AdaptixExtender) getAgentModule(agentName string) (adaptix.PluginAgent, error) {
	module, ok := ex.agentModules[agentName]
	if !ok {
		return nil, ErrModuleNotFound
	}
	return module, nil
}
