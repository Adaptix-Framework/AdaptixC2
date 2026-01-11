package eventing

import (
	adaptix "github.com/Adaptix-Framework/axc2"
)

type EventType string

const (
	EventClientConnect    EventType = "client.connect"
	EventClientDisconnect EventType = "client.disconnect"

	EventAgentNew       EventType = "agent.new"
	EventAgentGenerate  EventType = "agent.generate"
	EventAgentCheckin   EventType = "agent.checkin" // todo
	EventAgentUpdate    EventType = "agent.update"  //todo
	EventAgentTerminate EventType = "agent.terminate"
	EventAgentRemove    EventType = "agent.remove"

	EventListenerCreate EventType = "listener.create"
	EventListenerStart  EventType = "listener.start"
	EventListenerStop   EventType = "listener.stop"
	EventListenerPause  EventType = "listener.pause"
	EventListenerResume EventType = "listener.resume"
	EventListenerRemove EventType = "listener.remove"

	EventCredsAdd    EventType = "credentials.add"
	EventCredsEdit   EventType = "credentials.edit"
	EventCredsRemove EventType = "credentials.remove"

	EventTaskCreate    EventType = "task.create"
	EventTaskStart     EventType = "task.start"      // todo
	EventTaskUpdateJob EventType = "task.update_job"
	EventTaskComplete  EventType = "task.complete"

	EventDownloadStart  EventType = "download.start"
	EventDownloadFinish EventType = "download.finish"
	EventDownloadRemove EventType = "download.remove"

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
	CredId  string
	OldCred adaptix.CredsData
	NewCred adaptix.CredsData
}

type EventCredentialsRemove struct {
	BaseEvent
	CredIds []string
}

/// AGENT

type EventDataAgentNew struct {
	BaseEvent
	Agent   adaptix.AgentData
	Restore bool
}

type EventDataAgentGenerate struct {
	BaseEvent
	AgentName    string
	ListenerName string
	ListenerType string
	Config       string
	FileName     string
	FileContent  []byte
}

type EventDataAgentCheckin struct {
	BaseEvent
	Agent adaptix.AgentData
}

type EventDataAgentUpdate struct {
	BaseEvent
	Agent adaptix.AgentData
}

type EventDataAgentTerminate struct {
	BaseEvent
	AgentId string
	TaskId  string
}

type EventDataAgentRemove struct {
	BaseEvent
	Agent adaptix.AgentData
}

/// TASK

type EventDataTaskCreate struct {
	BaseEvent
	AgentId string
	Task    adaptix.TaskData
	Cmdline string
	Client  string
}

type EventDataTaskStart struct {
	BaseEvent
	AgentId string
	Task    adaptix.TaskData
}

type EventDataTaskUpdateJob struct {
	BaseEvent
	AgentId string
	Task    adaptix.TaskData
}

type EventDataTaskComplete struct {
	BaseEvent
	AgentId string
	Task    adaptix.TaskData
}

/// LISTENER

type EventDataListenerCreate struct {
	BaseEvent
	ListenerName string
	ListenerType string
	Config       string
}

type EventDataListenerStart struct {
	BaseEvent
	ListenerName string
	ListenerType string
	Config       string
	Restart      bool
}

type EventDataListenerStop struct {
	BaseEvent
	ListenerName string
	ListenerType string
}

type EventDataListenerPause struct {
	BaseEvent
	ListenerName string
	ListenerType string
}

type EventDataListenerResume struct {
	BaseEvent
	ListenerName string
	ListenerType string
}

type EventDataListenerRemove struct {
	BaseEvent
	ListenerName string
	ListenerType string
}

/// DOWNLOAD

type EventDataDownloadStart struct {
	BaseEvent
	AgentId  string
	FileId   string
	FileName string
	FileSize int
}

type EventDataDownloadFinish struct {
	BaseEvent
	Download adaptix.DownloadData
	Canceled bool
}

type EventDataDownloadRemove struct {
	BaseEvent
	FileIds []string
}

/// SCREENSHOT

type EventDataScreenshotAdd struct {
	BaseEvent
	AgentId string
	Note    string
	Content []byte
}

type EventDataScreenshotRemove struct {
	BaseEvent
	ScreenId string
}

/// TUNNEL

type EventDataTunnelStart struct {
	BaseEvent
	AgentId    string
	TunnelId   string
	TunnelType int
	Port       int
	Info       string
}

type EventDataTunnelStop struct {
	BaseEvent
	AgentId    string
	TunnelId   string
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
	TargetIds []string
}

/// PIVOT

type EventDataPivotCreate struct {
	BaseEvent
	PivotId       string
	ParentAgentId string
	ChildAgentId  string
	PivotName     string
}

type EventDataPivotRemove struct {
	BaseEvent
	PivotId string
}
