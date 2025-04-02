package extender

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
	ListenerStart(name string, data string, listenerCustomData []byte) ([]byte, []byte, error)
	ListenerEdit(name string, data string) ([]byte, []byte, error)
	ListenerStop(name string) error
	ListenerGetProfile(name string) ([]byte, error)
	ListenerInteralHandler(name string, data []byte) (string, error)
}

type ExtAgent interface {
	AgentGenerate(config string, operatingSystem string, listenerWM string, listenerProfile []byte) ([]byte, string, error)
	AgentCreate(beat []byte) ([]byte, error)
	AgentProcessData(agentObject []byte, packedData []byte) ([]byte, error)
	AgentPackData(agentObject []byte, maxDataSize int) ([]byte, error)
	AgentPivotPackData(pivotId string, data []byte) ([]byte, error)
	AgentCommand(client string, cmdline string, agentObject []byte, args map[string]any) error

	AgentDownloadChangeState(agentObject []byte, newState int, fileId string) ([]byte, error)
	AgentBrowserDisks(agentObject []byte) ([]byte, error)
	AgentBrowserProcess(agentObject []byte) ([]byte, error)
	AgentBrowserFiles(path string, agentObject []byte) ([]byte, error)
	AgentBrowserUpload(path string, content []byte, agentObject []byte) ([]byte, error)
	AgentBrowserDownload(path string, agentObject []byte) ([]byte, error)
	AgentBrowserJobKill(jobId string) ([]byte, error)
	AgentBrowserExit(agentObject []byte) ([]byte, error)
}

type AdaptixExtender struct {
	ts              Teamserver
	listenerModules map[string]ExtListener
	agentModules    map[string]ExtAgent
}
