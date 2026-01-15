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
	ExtenderType string `yaml:"extender_type"`
	ExtenderFile string `yaml:"extender_file"`
	AxFile       string `yaml:"ax_file"`
	ListenerName string `yaml:"listener_name"`
	ListenerType string `yaml:"listener_type"`
	Protocol     string `yaml:"protocol"`
}

/// ExConfig Agent

type ExConfigAgent struct {
	ExtenderType   string   `yaml:"extender_type"`
	ExtenderFile   string   `yaml:"extender_file"`
	AxFile         string   `yaml:"ax_file"`
	AgentName      string   `yaml:"agent_name"`
	AgentWatermark string   `yaml:"agent_watermark"`
	Listeners      []string `yaml:"listeners"`
	MultiListeners bool     `yaml:"multi_listeners"`
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
