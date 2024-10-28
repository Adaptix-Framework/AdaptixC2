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
	ListenerType     string
	ListenerProtocol string
	ListenerName     string
	ListenerUI       string
}

type AgentInfo struct {
	AgentName    string
	ListenerName string
	AgentUI      string
}

type Teamserver interface {
	ListenerNew(listenerInfo ListenerInfo) error
	AgentNew(agentInfo AgentInfo) error
}

type CommonFunctions interface {
	InitPlugin(ts any) ([]byte, error)
}

type ListenerFunctions interface {
	ListenerInit(path string) ([]byte, error)
	ListenerValid(config string) error
	ListenerStart(name string, data string) ([]byte, error)
	ListenerEdit(name string, data string) ([]byte, error)
	ListenerStop(name string) error
}

type AgentFunctions interface {
	AgentInit() ([]byte, error)
	AgentValid(config string) error
	AgentExists(agentId string) bool
	AgentCreate(beat []byte) ([]byte, error)
	AgentProcess(agentID string, beat []byte) ([]byte, error)
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
