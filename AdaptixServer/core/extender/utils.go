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
	Os      string `json:"operating_system"`
	Handler string `json:"handler"`
	Ui      any    `json:"generate_ui"`
}

type ExConfAgentListeners struct {
	ListenerName string                 `json:"listener_name"`
	OsConfigs    []ExConfAgentOsConfigs `json:"configs"`
}

type ExConfAgentSupportedBrowsers struct {
	FileBrowser         bool `json:"file_browser"`
	FileBrowserDisks    bool `json:"file_browser_disks"`
	FileBrowserDownload bool `json:"file_browser_download"`
	FileBrowserUpload   bool `json:"file_browser_upload"`
	ProcessBrowser      bool `json:"process_browser"`
	DownloadsState      bool `json:"downloads_state"`
	TasksJobKill        bool `json:"tasks_job_kill"`
	SessionsMenuExit    bool `json:"sessions_menu_exit"`
}

type ExConfAgentHandlers struct {
	Id       string `json:"id"`
	Commands any    `json:"commands"`
	Browsers any    `json:"browsers"`
}

type ExConfigAgent struct {
	ExtenderType   string                 `json:"extender_type"`
	ExtenderFile   string                 `json:"extender_file"`
	AgentName      string                 `json:"agent_name"`
	AgentWatermark string                 `json:"agent_watermark"`
	Listeners      []ExConfAgentListeners `json:"listeners"`
	Handlers       []ExConfAgentHandlers  `json:"handlers"`
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
	HandlersJson  string
}

type Teamserver interface {
	TsListenerReg(listenerInfo ListenerInfo) error
	TsListenerTypeByName(listenerName string) (string, error)
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

type ExtAgentFunc interface {
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

type ExtAgent struct {
	Supports map[string]map[int]ExConfAgentSupportedBrowsers
	F        ExtAgentFunc
}

type AdaptixExtender struct {
	ts              Teamserver
	listenerModules map[string]ExtListener
	agentModules    map[string]ExtAgent
}
