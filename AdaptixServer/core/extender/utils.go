package extender

import "AdaptixServer/core/utils/safe"

const (
	TYPE_LISTENER = "listener"
	TYPE_AGENT    = "agent"
)

type ModuleInfo struct {
	ModuleName string
	ModuleType string
}

type ListenerInfo struct {
	Type             string
	ListenerProtocol string
	ListenerName     string
	ListenerUI       string
}

type AgentInfo struct {
	AgentName    string
	ListenerName []string
	AgentUI      string
	AgentCmd     string
}

type Teamserver interface {
	TsListenerReg(listenerInfo ListenerInfo) error
	TsAgentReg(agentInfo AgentInfo) error
}

type CommonFunctions interface {
	InitPlugin(ts any) ([]byte, error)
}

type ListenerFunctions interface {
	ListenerInit(pluginPath string, listenerDataPath string) ([]byte, error)
	ListenerValid(config string) error
	ListenerStart(name string, data string, listenerCustomData []byte) ([]byte, []byte, error)
	ListenerEdit(name string, data string) ([]byte, []byte, error)
	ListenerStop(name string) error
	ListenerGetProfile(name string) ([]byte, error)
	ListenerInteralHandler(name string, data []byte) (string, error)
}

type AgentFunctions interface {
	AgentInit(pluginPath string) ([]byte, error)
	AgentGenerate(config string, listenerWM string, listenerProfile []byte) ([]byte, string, error)
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

type ModuleExtender struct {
	Info ModuleInfo
	CommonFunctions
	ListenerFunctions
	AgentFunctions
}

type AdaptixExtender struct {
	ts              Teamserver
	listenerModules safe.Map
	agentModules    safe.Map
}
