package extender

import "AdaptixServer/core/utils/safe"

const (
	TYPE_LISTENER = "listener"
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

type Teamserver interface {
	ListenerNew(listenerInfo ListenerInfo) error
}

type CommonFunctions interface {
	InitPlugin(ts any) ([]byte, error)
}

type ListenerFunctions interface {
	ListenerInit(path string) ([]byte, error)
	ListenerValid(config string) error
	ListenerStart(name string, data string) ([]byte, error)
	ListenerStop(name string) error
}

type ModuleExtender struct {
	Info ModuleInfo
	CommonFunctions
	ListenerFunctions
}

type AdaptixExtender struct {
	ts              Teamserver
	listenerModules safe.Map
}
