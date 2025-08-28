package extender

import "github.com/Adaptix-Framework/axc2"

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
}

/// Info

type ListenerInfo struct {
	Name     string
	Protocol string
	Type     string
	AX       string
}

type AgentInfo struct {
	Name      string
	Watermark string
	AX        string
	Listeners []string
}

type Teamserver interface {
	TsListenerReg(listenerInfo ListenerInfo) error
	TsListenerRegByName(listenerName string) (string, error)
	TsAgentReg(agentInfo AgentInfo) error
}

type ExtListener interface {
	ListenerValid(config string) error
	ListenerStart(name string, data string, listenerCustomData []byte) (adaptix.ListenerData, []byte, error)
	ListenerEdit(name string, data string) (adaptix.ListenerData, []byte, error)
	ListenerStop(name string) error
	ListenerGetProfile(name string) ([]byte, error)
	ListenerInteralHandler(name string, data []byte) (string, error)
}

type ExtAgent interface {
	AgentGenerate(config string, listenerWM string, listenerProfile []byte) ([]byte, string, error)
	AgentCreate(beat []byte) (adaptix.AgentData, error)
	AgentCommand(agentData adaptix.AgentData, args map[string]any) (adaptix.TaskData, adaptix.ConsoleMessageData, error)
	AgentProcessData(agentData adaptix.AgentData, packedData []byte) ([]byte, error)
	AgentPackData(agentData adaptix.AgentData, tasks []adaptix.TaskData) ([]byte, error)
	AgentPivotPackData(pivotId string, data []byte) (adaptix.TaskData, error)

	AgentTunnelCallbacks() (func(int, string, int) adaptix.TaskData, func(int, string, int) adaptix.TaskData, func(int, []byte) adaptix.TaskData, func(int, []byte) adaptix.TaskData, func(int) adaptix.TaskData, func(int, int) adaptix.TaskData, error)
	AgentTerminalCallbacks() (func(int, string, int, int) (adaptix.TaskData, error), func(int, []byte) (adaptix.TaskData, error), func(int) (adaptix.TaskData, error), error)
}

type AdaptixExtender struct {
	ts              Teamserver
	listenerModules map[string]ExtListener
	agentModules    map[string]ExtAgent
}
