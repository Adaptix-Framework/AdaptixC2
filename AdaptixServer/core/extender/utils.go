package extender

import "github.com/Adaptix-Framework/axc2"

/// ExConfig Listener

type ExConfigListener struct {
	ExtenderType string `json:"extender_type"`
	ExtenderFile string `json:"extender_file"`
	ListenerName string `json:"listener_name"`
	ListenerType string `json:"listener_type"`
	Protocol     string `json:"protocol"`
	UI           any    `json:"ui"`
}

/// ExConfig Agent

type ExConfAgentOsConfigs struct {
	Os string `json:"operating_system"`
	Ui any    `json:"ui"`
}

type ExConfAgentUI struct {
	ListenerName string                 `json:"listener_name"`
	OsConfigs    []ExConfAgentOsConfigs `json:"os_configs"`
}

type ExConfigAgent struct {
	ExtenderType   string          `json:"extender_type"`
	ExtenderFile   string          `json:"extender_file"`
	AgentName      string          `json:"agent_name"`
	AgentWatermark string          `json:"agent_watermark"`
	Listeners      []ExConfAgentUI `json:"listeners"`
	Commands       any             `json:"commands"`
}

/// Info

type ListenerInfo struct {
	Name     string
	Protocol string
	Type     string
	UI       string
}

type AgentInfo struct {
	Name          string
	Watermark     string
	ListenersJson string
	CommandsJson  string
}

type Teamserver interface {
	TsListenerReg(listenerInfo ListenerInfo) error
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
	AgentGenerate(config string, operatingSystem string, listenerWM string, listenerProfile []byte) ([]byte, string, error)
	AgentCreate(beat []byte) (adaptix.AgentData, error)
	AgentCommand(client string, cmdline string, agentData adaptix.AgentData, args map[string]any) error
	AgentProcessData(agentData adaptix.AgentData, packedData []byte) ([]byte, error)
	AgentPackData(agentData adaptix.AgentData, maxDataSize int) ([]byte, error)
	AgentPivotPackData(pivotId string, data []byte) (adaptix.TaskData, error)

	AgentDownloadChangeState(agentData adaptix.AgentData, newState int, fileId string) (adaptix.TaskData, error)
	AgentBrowserDisks(agentData adaptix.AgentData) (adaptix.TaskData, error)
	AgentBrowserProcess(agentData adaptix.AgentData) (adaptix.TaskData, error)
	AgentBrowserFiles(path string, agentData adaptix.AgentData) (adaptix.TaskData, error)
	AgentBrowserUpload(path string, content []byte, agentData adaptix.AgentData) (adaptix.TaskData, error)
	AgentBrowserDownload(path string, agentData adaptix.AgentData) (adaptix.TaskData, error)
	AgentBrowserExit(agentData adaptix.AgentData) (adaptix.TaskData, error)
	AgentBrowserJobKill(jobId string) (adaptix.TaskData, error)
}

type AdaptixExtender struct {
	ts              Teamserver
	listenerModules map[string]ExtListener
	agentModules    map[string]ExtAgent
}
