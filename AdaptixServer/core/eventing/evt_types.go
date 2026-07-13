package eventing

import "github.com/Adaptix-Framework/axc2/v2"

type EventType string

const (
	EventClientConnect    EventType = "client.connect"
	EventClientDisconnect EventType = "client.disconnect"

	EventAgentNew       EventType = "agent.new"
	EventAgentGenerate  EventType = "agent.generate"
	EventAgentCheckin   EventType = "agent.checkin" // todo
	EventAgentActivate  EventType = "agent.activate"
	EventAgentUpdate    EventType = "agent.update" //todo
	EventAgentTerminate EventType = "agent.terminate"
	EventAgentRemove    EventType = "agent.remove"

	EventListenerStart EventType = "listener.start"
	EventListenerStop  EventType = "listener.stop"

	EventCredsAdd    EventType = "credentials.add"
	EventCredsEdit   EventType = "credentials.edit"
	EventCredsRemove EventType = "credentials.remove"

	EventTaskCreate    EventType = "task.create"
	EventTaskStart     EventType = "task.start" // todo
	EventTaskUpdateJob EventType = "task.update_job"
	EventTaskComplete  EventType = "task.complete"

	EventDownloadStart  EventType = "download.start"
	EventDownloadFinish EventType = "download.finish"
	EventDownloadRemove EventType = "download.remove"

	EventUploadStart  EventType = "upload.start"
	EventUploadFinish EventType = "upload.finish"
	EventUploadRemove EventType = "upload.remove"

	EventScreenshotAdd    EventType = "screenshot.add"
	EventScreenshotRemove EventType = "screenshot.remove"

	EventTunnelStart EventType = "tunnel.start"
	EventTunnelStop  EventType = "tunnel.stop"

	EventTargetAdd    EventType = "target.add"
	EventTargetEdit   EventType = "target.edit"
	EventTargetRemove EventType = "target.remove"

	EventPivotCreate EventType = "pivot.create"
	EventPivotRemove EventType = "pivot.remove"
)

type HookPhase int

const (
	HookPre HookPhase = iota
	HookPost
)

type BaseEvent struct {
	Type      EventType
	Phase     HookPhase
	Cancelled bool
	Error     error
}

func (e *BaseEvent) Cancel(err error) {
	e.Cancelled = true
	e.Error = err
}

/// CREDENTIALS

type EventCredentialsAdd struct {
	BaseEvent
	Credentials []adaptix.CredsData
}

type EventCredentialsEdit struct {
	BaseEvent
	CredId  int64
	OldCred adaptix.CredsData
	NewCred adaptix.CredsData
}

type EventCredentialsRemove struct {
	BaseEvent
	CredIds []int64
}

/// AGENT

type EventDataAgentNew struct {
	BaseEvent
	Agent   adaptix.AgentData
	Restore bool
}

type EventDataAgentGenerate struct {
	BaseEvent
	BuilderId     string
	AgentName     string
	ListenersName []string
	Config        string
	FileName      string
	FileContent   []byte
}

type EventDataAgentCheckin struct {
	BaseEvent
	Agent adaptix.AgentData
}

type EventDataAgentUpdate struct {
	BaseEvent
	Agent adaptix.AgentData
}

type EventDataAgentActivate struct {
	BaseEvent
	Agent adaptix.AgentData
}

type EventDataAgentTerminate struct {
	BaseEvent
	AgentId int64
	TaskId  int64
}

type EventDataAgentRemove struct {
	BaseEvent
	Agent adaptix.AgentData
}

/// TASK

type EventDataTaskCreate struct {
	BaseEvent
	AgentId int64
	Task    adaptix.TaskData
	Cmdline string
	Client  string
}

type EventDataTaskStart struct {
	BaseEvent
	AgentId int64
	Task    adaptix.TaskData
}

type EventDataTaskUpdateJob struct {
	BaseEvent
	AgentId int64
	Task    adaptix.TaskData
}

type EventDataTaskComplete struct {
	BaseEvent
	AgentId int64
	Task    adaptix.TaskData
}

/// LISTENER

type EventDataListenerStart struct {
	BaseEvent
	ListenerName string
	ListenerType string
	Config       string
}

type EventDataListenerStop struct {
	BaseEvent
	ListenerName string
	ListenerType string
}

/// DOWNLOAD

type EventDataDownloadStart struct {
	BaseEvent
	AgentId  int64
	FileId   int64
	FileName string
	FileSize int64
}

type EventDataDownloadFinish struct {
	BaseEvent
	Download adaptix.TransferData
	Canceled bool
}

type EventDataDownloadRemove struct {
	BaseEvent
	FileIds []int64
}

/// UPLOAD

type EventDataUploadStart struct {
	BaseEvent
	AgentId    int64
	FileId     int64
	FileName   string
	RemotePath string
	FileSize   int64
}

type EventDataUploadFinish struct {
	BaseEvent
	FileId   int64
	Canceled bool
}

type EventDataUploadRemove struct {
	BaseEvent
	FileIds []int64
}

/// SCREENSHOT

type EventDataScreenshotAdd struct {
	BaseEvent
	AgentId int64
	Note    string
	Content []byte
}

type EventDataScreenshotRemove struct {
	BaseEvent
	ScreenId int64
}

/// TUNNEL

type EventDataTunnelStart struct {
	BaseEvent
	AgentId    int64
	TunnelId   int64
	TunnelType int
	Port       int
	Info       string
}

type EventDataTunnelStop struct {
	BaseEvent
	AgentId    int64
	TunnelId   int64
	TunnelType int
	Port       int
}

/// CLIENT

type EventDataClientConnect struct {
	BaseEvent
	Username string
}

type EventDataClientDisconnect struct {
	BaseEvent
	Username string
}

/// TARGET

type EventDataTargetAdd struct {
	BaseEvent
	Targets []adaptix.TargetData
}

type EventDataTargetEdit struct {
	BaseEvent
	Target adaptix.TargetData
}

type EventDataTargetRemove struct {
	BaseEvent
	TargetIds []int64
}

/// PIVOT

type EventDataPivotCreate struct {
	BaseEvent
	PivotId       string
	ParentAgentId int64
	ChildAgentId  int64
	PivotName     string
}

type EventDataPivotRemove struct {
	BaseEvent
	PivotId string
}
