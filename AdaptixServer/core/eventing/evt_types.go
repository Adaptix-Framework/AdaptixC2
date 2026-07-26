package eventing

import (
	"fmt"
	"strconv"

	"github.com/Adaptix-Framework/axc2/v2"
)

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

var KnownEventTypes = map[EventType]bool{
	EventClientConnect: true, EventClientDisconnect: true,
	EventAgentNew: true, EventAgentGenerate: true, EventAgentCheckin: true,
	EventAgentActivate: true, EventAgentUpdate: true, EventAgentTerminate: true, EventAgentRemove: true,
	EventListenerStart: true, EventListenerStop: true,
	EventCredsAdd: true, EventCredsEdit: true, EventCredsRemove: true,
	EventTaskCreate: true, EventTaskStart: true, EventTaskUpdateJob: true, EventTaskComplete: true,
	EventDownloadStart: true, EventDownloadFinish: true, EventDownloadRemove: true,
	EventUploadStart: true, EventUploadFinish: true, EventUploadRemove: true,
	EventScreenshotAdd: true, EventScreenshotRemove: true,
	EventTunnelStart: true, EventTunnelStop: true,
	EventTargetAdd: true, EventTargetEdit: true, EventTargetRemove: true,
	EventPivotCreate: true, EventPivotRemove: true,
}

type HookPhase int

const (
	HookPre HookPhase = iota
	HookPost
)

type BaseEvent struct {
	Type      EventType `json:"type,omitempty"`
	Phase     HookPhase `json:"phase,omitempty"`
	Cancelled bool      `json:"cancelled,omitempty"`
	Error     error     `json:"-"`
}

func (e *BaseEvent) Cancel(err error) {
	e.Cancelled = true
	e.Error = err
}

/// CREDENTIALS

type EventCredentialsAdd struct {
	BaseEvent
	Credentials []adaptix.CredsData `json:"credentials"`
}

type EventCredentialsEdit struct {
	BaseEvent
	CredId  int64             `json:"credId"`
	OldCred adaptix.CredsData `json:"oldCred"`
	NewCred adaptix.CredsData `json:"newCred"`
}

type EventCredentialsRemove struct {
	BaseEvent
	CredIds []int64 `json:"credIds"`
}

/// AGENT

type EventDataAgentNew struct {
	BaseEvent
	Agent   adaptix.AgentData `json:"agent"`
	Restore bool              `json:"restore"`
}

type EventDataAgentGenerate struct {
	BaseEvent
	BuilderId     string   `json:"builderId"`
	AgentName     string   `json:"agentName"`
	ListenersName []string `json:"listenersName"`
	Config        string   `json:"config"`
	FileName      string   `json:"fileName"`
	FileContent   []byte   `json:"fileContent,omitempty"`
}

type EventDataAgentCheckin struct {
	BaseEvent
	Agent adaptix.AgentData `json:"agent"`
}

type EventDataAgentUpdate struct {
	BaseEvent
	Agent adaptix.AgentData `json:"agent"`
}

type EventDataAgentActivate struct {
	BaseEvent
	Agent adaptix.AgentData `json:"agent"`
}

type EventDataAgentTerminate struct {
	BaseEvent
	AgentId int64 `json:"agentId"`
	TaskId  int64 `json:"taskId"`
}

type EventDataAgentRemove struct {
	BaseEvent
	Agent adaptix.AgentData `json:"agent"`
}

/// TASK

type EventDataTaskCreate struct {
	BaseEvent
	AgentId int64            `json:"agentId"`
	Task    adaptix.TaskData `json:"task"`
	Cmdline string           `json:"cmdline"`
	Client  string           `json:"client"`
}

type EventDataTaskStart struct {
	BaseEvent
	AgentId int64            `json:"agentId"`
	Task    adaptix.TaskData `json:"task"`
}

type EventDataTaskUpdateJob struct {
	BaseEvent
	AgentId int64            `json:"agentId"`
	Task    adaptix.TaskData `json:"task"`
}

type EventDataTaskComplete struct {
	BaseEvent
	AgentId int64            `json:"agentId"`
	Task    adaptix.TaskData `json:"task"`
}

/// LISTENER

type EventDataListenerStart struct {
	BaseEvent
	ListenerName string `json:"listenerName"`
	ListenerType string `json:"listenerType"`
	Config       string `json:"config"`
}

type EventDataListenerStop struct {
	BaseEvent
	ListenerName string `json:"listenerName"`
	ListenerType string `json:"listenerType"`
}

/// DOWNLOAD

type EventDataDownloadStart struct {
	BaseEvent
	AgentId  int64  `json:"agentId"`
	FileId   int64  `json:"fileId"`
	FileName string `json:"fileName"`
	FileSize int64  `json:"fileSize"`
}

type EventDataDownloadFinish struct {
	BaseEvent
	Download adaptix.TransferData `json:"download"`
	Canceled bool                 `json:"canceled"`
}

type EventDataDownloadRemove struct {
	BaseEvent
	FileIds []int64 `json:"fileIds"`
}

/// UPLOAD

type EventDataUploadStart struct {
	BaseEvent
	AgentId    int64  `json:"agentId"`
	FileId     int64  `json:"fileId"`
	FileName   string `json:"fileName"`
	RemotePath string `json:"remotePath"`
	FileSize   int64  `json:"fileSize"`
}

type EventDataUploadFinish struct {
	BaseEvent
	FileId   int64 `json:"fileId"`
	Canceled bool  `json:"canceled"`
}

type EventDataUploadRemove struct {
	BaseEvent
	FileIds []int64 `json:"fileIds"`
}

/// SCREENSHOT

type EventDataScreenshotAdd struct {
	BaseEvent
	AgentId int64  `json:"agentId"`
	Note    string `json:"note"`
	Content []byte `json:"content,omitempty"`
}

type EventDataScreenshotRemove struct {
	BaseEvent
	ScreenId int64 `json:"screenId"`
}

/// TUNNEL

type EventDataTunnelStart struct {
	BaseEvent
	AgentId    int64  `json:"agentId"`
	TunnelId   int64  `json:"tunnelId"`
	TunnelType int    `json:"tunnelType"`
	Port       int    `json:"port"`
	Info       string `json:"info"`
}

type EventDataTunnelStop struct {
	BaseEvent
	AgentId    int64 `json:"agentId"`
	TunnelId   int64 `json:"tunnelId"`
	TunnelType int   `json:"tunnelType"`
	Port       int   `json:"port"`
}

/// CLIENT

type EventDataClientConnect struct {
	BaseEvent
	Username string `json:"username"`
}

type EventDataClientDisconnect struct {
	BaseEvent
	Username string `json:"username"`
}

/// TARGET

type EventDataTargetAdd struct {
	BaseEvent
	Targets []adaptix.TargetData `json:"targets"`
}

type EventDataTargetEdit struct {
	BaseEvent
	Target adaptix.TargetData `json:"target"`
}

type EventDataTargetRemove struct {
	BaseEvent
	TargetIds []int64 `json:"targetIds"`
}

/// PIVOT

type EventDataPivotCreate struct {
	BaseEvent
	PivotId       string `json:"pivotId"`
	ParentAgentId int64  `json:"parentAgentId"`
	ChildAgentId  int64  `json:"childAgentId"`
	PivotName     string `json:"pivotName"`
}

type EventDataPivotRemove struct {
	BaseEvent
	PivotId string `json:"pivotId"`
}

type Event interface {
	Base() *BaseEvent
}

type FactSource interface {
	FillFacts(f *EventFacts)
}

type Summarizable interface {
	Summary() string
}

var (
	_ Event = (*EventCredentialsAdd)(nil)
	_ Event = (*EventCredentialsEdit)(nil)
	_ Event = (*EventCredentialsRemove)(nil)
	_ Event = (*EventDataAgentNew)(nil)
	_ Event = (*EventDataAgentGenerate)(nil)
	_ Event = (*EventDataAgentCheckin)(nil)
	_ Event = (*EventDataAgentActivate)(nil)
	_ Event = (*EventDataAgentTerminate)(nil)
	_ Event = (*EventDataAgentRemove)(nil)
	_ Event = (*EventDataAgentUpdate)(nil)
	_ Event = (*EventDataTaskCreate)(nil)
	_ Event = (*EventDataTaskStart)(nil)
	_ Event = (*EventDataTaskUpdateJob)(nil)
	_ Event = (*EventDataTaskComplete)(nil)
	_ Event = (*EventDataListenerStart)(nil)
	_ Event = (*EventDataListenerStop)(nil)
	_ Event = (*EventDataDownloadStart)(nil)
	_ Event = (*EventDataDownloadFinish)(nil)
	_ Event = (*EventDataDownloadRemove)(nil)
	_ Event = (*EventDataUploadStart)(nil)
	_ Event = (*EventDataUploadFinish)(nil)
	_ Event = (*EventDataUploadRemove)(nil)
	_ Event = (*EventDataScreenshotAdd)(nil)
	_ Event = (*EventDataScreenshotRemove)(nil)
	_ Event = (*EventDataTunnelStart)(nil)
	_ Event = (*EventDataTunnelStop)(nil)
	_ Event = (*EventDataClientConnect)(nil)
	_ Event = (*EventDataClientDisconnect)(nil)
	_ Event = (*EventDataTargetAdd)(nil)
	_ Event = (*EventDataTargetEdit)(nil)
	_ Event = (*EventDataTargetRemove)(nil)
	_ Event = (*EventDataPivotCreate)(nil)
	_ Event = (*EventDataPivotRemove)(nil)

	_ FactSource = (*EventCredentialsAdd)(nil)
	_ FactSource = (*EventDataAgentNew)(nil)
	_ FactSource = (*EventDataTaskCreate)(nil)
	_ FactSource = (*EventDataDownloadStart)(nil)
	_ FactSource = (*EventDataTunnelStart)(nil)
	_ FactSource = (*EventDataClientConnect)(nil)
	_ FactSource = (*EventDataTargetAdd)(nil)
	_ FactSource = (*EventDataPivotCreate)(nil)

	_ Summarizable = (*EventDataAgentNew)(nil)
	_ Summarizable = (*EventDataTaskCreate)(nil)
	_ Summarizable = (*EventCredentialsAdd)(nil)
	_ Summarizable = (*EventDataListenerStart)(nil)
)

func getBaseEvent(event any) *BaseEvent {
	if event == nil {
		return nil
	}
	if e, ok := event.(Event); ok {
		return e.Base()
	}
	return nil
}

func ExtractFacts(event any) EventFacts {
	var f EventFacts
	if e, ok := event.(Event); ok {
		if be := e.Base(); be != nil {
			f.Type = string(be.Type)
		}
	}
	if fs, ok := event.(FactSource); ok {
		fs.FillFacts(&f)
	}
	return f
}

func (e *EventCredentialsAdd) Base() *BaseEvent     { return &e.BaseEvent }
func (e *EventCredentialsEdit) Base() *BaseEvent    { return &e.BaseEvent }
func (e *EventCredentialsRemove) Base() *BaseEvent  { return &e.BaseEvent }
func (e *EventDataAgentNew) Base() *BaseEvent       { return &e.BaseEvent }
func (e *EventDataAgentGenerate) Base() *BaseEvent  { return &e.BaseEvent }
func (e *EventDataAgentCheckin) Base() *BaseEvent   { return &e.BaseEvent }
func (e *EventDataAgentActivate) Base() *BaseEvent  { return &e.BaseEvent }
func (e *EventDataAgentTerminate) Base() *BaseEvent { return &e.BaseEvent }
func (e *EventDataAgentRemove) Base() *BaseEvent    { return &e.BaseEvent }
func (e *EventDataAgentUpdate) Base() *BaseEvent    { return &e.BaseEvent }
func (e *EventDataTaskCreate) Base() *BaseEvent     { return &e.BaseEvent }
func (e *EventDataTaskStart) Base() *BaseEvent      { return &e.BaseEvent }
func (e *EventDataTaskUpdateJob) Base() *BaseEvent  { return &e.BaseEvent }
func (e *EventDataTaskComplete) Base() *BaseEvent   { return &e.BaseEvent }
func (e *EventDataListenerStart) Base() *BaseEvent  { return &e.BaseEvent }
func (e *EventDataListenerStop) Base() *BaseEvent   { return &e.BaseEvent }
func (e *EventDataDownloadStart) Base() *BaseEvent  { return &e.BaseEvent }
func (e *EventDataDownloadFinish) Base() *BaseEvent { return &e.BaseEvent }
func (e *EventDataDownloadRemove) Base() *BaseEvent { return &e.BaseEvent }
func (e *EventDataUploadStart) Base() *BaseEvent    { return &e.BaseEvent }
func (e *EventDataUploadFinish) Base() *BaseEvent   { return &e.BaseEvent }
func (e *EventDataUploadRemove) Base() *BaseEvent   { return &e.BaseEvent }
func (e *EventDataScreenshotAdd) Base() *BaseEvent  { return &e.BaseEvent }
func (e *EventDataScreenshotRemove) Base() *BaseEvent {
	return &e.BaseEvent
}
func (e *EventDataTunnelStart) Base() *BaseEvent      { return &e.BaseEvent }
func (e *EventDataTunnelStop) Base() *BaseEvent       { return &e.BaseEvent }
func (e *EventDataClientConnect) Base() *BaseEvent    { return &e.BaseEvent }
func (e *EventDataClientDisconnect) Base() *BaseEvent { return &e.BaseEvent }
func (e *EventDataTargetAdd) Base() *BaseEvent        { return &e.BaseEvent }
func (e *EventDataTargetEdit) Base() *BaseEvent       { return &e.BaseEvent }
func (e *EventDataTargetRemove) Base() *BaseEvent     { return &e.BaseEvent }
func (e *EventDataPivotCreate) Base() *BaseEvent      { return &e.BaseEvent }
func (e *EventDataPivotRemove) Base() *BaseEvent      { return &e.BaseEvent }

func (e *EventDataAgentNew) FillFacts(f *EventFacts)      { applyAgent(f, e.Agent) }
func (e *EventDataAgentCheckin) FillFacts(f *EventFacts)  { applyAgent(f, e.Agent) }
func (e *EventDataAgentActivate) FillFacts(f *EventFacts) { applyAgent(f, e.Agent) }
func (e *EventDataAgentUpdate) FillFacts(f *EventFacts)   { applyAgent(f, e.Agent) }
func (e *EventDataAgentRemove) FillFacts(f *EventFacts)   { applyAgent(f, e.Agent) }

func (e *EventDataAgentTerminate) FillFacts(f *EventFacts) {
	f.HasAgent = true
	f.AgentID = e.AgentId
	if e.TaskId != 0 {
		f.HasTask = true
		f.TaskID = e.TaskId
	}
}

func (e *EventDataAgentGenerate) FillFacts(f *EventFacts) {
	if e.AgentName != "" {
		f.AgentName = e.AgentName
	}
	if len(e.ListenersName) > 0 {
		f.Listener = e.ListenersName[0]
	}
}

func (e *EventDataTaskCreate) FillFacts(f *EventFacts) {
	f.HasAgent = true
	f.AgentID = e.AgentId
	f.HasClient = true
	f.Client = e.Client
	if e.Task.AgentId != 0 {
		f.AgentID = e.Task.AgentId
	}
	if e.Task.Client != "" {
		f.Client = e.Task.Client
	}
	if e.Task.TaskId != 0 {
		f.HasTask = true
		f.TaskID = e.Task.TaskId
	}
}

func (e *EventDataTaskStart) FillFacts(f *EventFacts) {
	f.HasAgent = true
	f.AgentID = e.AgentId
	if e.Task.TaskId != 0 {
		f.HasTask = true
		f.TaskID = e.Task.TaskId
	}
	if e.Task.Client != "" {
		f.HasClient = true
		f.Client = e.Task.Client
	}
}

func (e *EventDataTaskUpdateJob) FillFacts(f *EventFacts) {
	f.HasAgent = true
	f.AgentID = e.AgentId
	if e.Task.TaskId != 0 {
		f.HasTask = true
		f.TaskID = e.Task.TaskId
	}
}

func (e *EventDataTaskComplete) FillFacts(f *EventFacts) {
	f.HasAgent = true
	f.AgentID = e.AgentId
	if e.Task.TaskId != 0 {
		f.HasTask = true
		f.TaskID = e.Task.TaskId
	}
	if e.Task.Client != "" {
		f.HasClient = true
		f.Client = e.Task.Client
	}
}

func (e *EventDataDownloadStart) FillFacts(f *EventFacts) {
	f.HasAgent = true
	f.AgentID = e.AgentId
	f.HasFile = true
	f.FileID = e.FileId
	f.Filename = e.FileName
}

func (e *EventDataDownloadFinish) FillFacts(f *EventFacts) {
	if e.Download.AgentId != 0 {
		f.HasAgent = true
		f.AgentID = e.Download.AgentId
	}
	if e.Download.FileId != 0 {
		f.HasFile = true
		f.FileID = e.Download.FileId
	}
	f.Filename = basenamePath(e.Download.RemotePath)
	if f.Filename == "" {
		f.Filename = e.Download.ArtifactName
	}
	if e.Download.Computer != "" {
		f.Computer = e.Download.Computer
	}
	if e.Download.AgentName != "" {
		f.AgentName = e.Download.AgentName
	}
}

func (e *EventDataDownloadRemove) FillFacts(f *EventFacts) {}

func (e *EventDataUploadStart) FillFacts(f *EventFacts) {
	f.HasAgent = true
	f.AgentID = e.AgentId
	f.HasFile = true
	f.FileID = e.FileId
	f.Filename = e.FileName
	if e.RemotePath != "" && f.Filename == "" {
		f.Filename = basenamePath(e.RemotePath)
	}
}

func (e *EventDataUploadFinish) FillFacts(f *EventFacts) {
	if e.FileId != 0 {
		f.HasFile = true
		f.FileID = e.FileId
	}
}

func (e *EventDataUploadRemove) FillFacts(f *EventFacts) {}

func (e *EventDataScreenshotAdd) FillFacts(f *EventFacts) {
	f.HasAgent = true
	f.AgentID = e.AgentId
}

func (e *EventDataScreenshotRemove) FillFacts(f *EventFacts) {}

func (e *EventDataTunnelStart) FillFacts(f *EventFacts) {
	f.HasAgent = true
	f.AgentID = e.AgentId
	f.HasPort = true
	f.Port = e.Port
	f.TunnelType = strconv.Itoa(e.TunnelType)
}

func (e *EventDataTunnelStop) FillFacts(f *EventFacts) {
	f.HasAgent = true
	f.AgentID = e.AgentId
	f.HasPort = true
	f.Port = e.Port
	f.TunnelType = strconv.Itoa(e.TunnelType)
}

func (e *EventDataListenerStart) FillFacts(f *EventFacts) {
	f.Listener = e.ListenerName
	f.ListenerType = e.ListenerType
}

func (e *EventDataListenerStop) FillFacts(f *EventFacts) {
	f.Listener = e.ListenerName
	f.ListenerType = e.ListenerType
}

func (e *EventDataClientConnect) FillFacts(f *EventFacts) {
	f.HasClient = true
	f.Client = e.Username
	f.User = e.Username
}

func (e *EventDataClientDisconnect) FillFacts(f *EventFacts) {
	f.HasClient = true
	f.Client = e.Username
	f.User = e.Username
}

func (e *EventCredentialsAdd) FillFacts(f *EventFacts) {
	if len(e.Credentials) > 0 {
		*f = mergeFacts(*f, factsFromCred(e.Credentials[0]))
	}
}

func (e *EventCredentialsEdit) FillFacts(f *EventFacts) {
	*f = mergeFacts(*f, factsFromCred(e.NewCred))
}

func (e *EventCredentialsRemove) FillFacts(f *EventFacts) {}

func (e *EventDataTargetAdd) FillFacts(f *EventFacts) {
	if len(e.Targets) > 0 {
		*f = mergeFacts(*f, factsFromTarget(e.Targets[0]))
	}
}

func (e *EventDataTargetEdit) FillFacts(f *EventFacts) {
	*f = mergeFacts(*f, factsFromTarget(e.Target))
}

func (e *EventDataTargetRemove) FillFacts(f *EventFacts) {}

func (e *EventDataPivotCreate) FillFacts(f *EventFacts) {
	f.HasAgent = true
	f.AgentID = e.ParentAgentId
}

func (e *EventDataPivotRemove) FillFacts(f *EventFacts) {}

func (e *EventDataAgentNew) Summary() string {
	return fmt.Sprintf("agent=%d name=%s computer=%s restore=%v", e.Agent.Id, e.Agent.Name, e.Agent.Computer, e.Restore)
}
func (e *EventDataAgentActivate) Summary() string {
	return fmt.Sprintf("agent=%d name=%s computer=%s", e.Agent.Id, e.Agent.Name, e.Agent.Computer)
}
func (e *EventDataAgentRemove) Summary() string {
	return fmt.Sprintf("agent=%d name=%s", e.Agent.Id, e.Agent.Name)
}
func (e *EventDataAgentUpdate) Summary() string {
	return fmt.Sprintf("agent=%d name=%s", e.Agent.Id, e.Agent.Name)
}
func (e *EventDataAgentCheckin) Summary() string {
	return fmt.Sprintf("agent=%d", e.Agent.Id)
}
func (e *EventDataAgentTerminate) Summary() string {
	return fmt.Sprintf("agent=%d task=%d", e.AgentId, e.TaskId)
}
func (e *EventDataAgentGenerate) Summary() string {
	return fmt.Sprintf("builder=%s agent=%s file=%s", e.BuilderId, e.AgentName, e.FileName)
}

func (e *EventDataTaskCreate) Summary() string {
	return fmt.Sprintf("agent=%d client=%s cmdline=%s", e.AgentId, e.Client, truncate(e.Cmdline, 80))
}
func (e *EventDataTaskStart) Summary() string {
	return fmt.Sprintf("agent=%d task=%d", e.AgentId, e.Task.TaskId)
}
func (e *EventDataTaskUpdateJob) Summary() string {
	return fmt.Sprintf("agent=%d task=%d", e.AgentId, e.Task.TaskId)
}
func (e *EventDataTaskComplete) Summary() string {
	return fmt.Sprintf("agent=%d task=%d", e.AgentId, e.Task.TaskId)
}

func (e *EventDataListenerStart) Summary() string {
	return fmt.Sprintf("listener=%s type=%s", e.ListenerName, e.ListenerType)
}
func (e *EventDataListenerStop) Summary() string {
	return fmt.Sprintf("listener=%s type=%s", e.ListenerName, e.ListenerType)
}

func (e *EventCredentialsAdd) Summary() string {
	return fmt.Sprintf("count=%d", len(e.Credentials))
}
func (e *EventCredentialsEdit) Summary() string {
	return fmt.Sprintf("cred=%d", e.CredId)
}
func (e *EventCredentialsRemove) Summary() string {
	return fmt.Sprintf("count=%d", len(e.CredIds))
}

func (e *EventDataDownloadStart) Summary() string {
	return fmt.Sprintf("agent=%d file=%s size=%d", e.AgentId, e.FileName, e.FileSize)
}
func (e *EventDataDownloadFinish) Summary() string {
	name := e.Download.RemotePath
	if name == "" {
		name = e.Download.ArtifactName
	}
	return fmt.Sprintf("file=%d path=%s canceled=%v", e.Download.FileId, name, e.Canceled)
}
func (e *EventDataDownloadRemove) Summary() string {
	return fmt.Sprintf("count=%d", len(e.FileIds))
}

func (e *EventDataUploadStart) Summary() string {
	return fmt.Sprintf("agent=%d file=%s path=%s size=%d", e.AgentId, e.FileName, e.RemotePath, e.FileSize)
}
func (e *EventDataUploadFinish) Summary() string {
	return fmt.Sprintf("file=%d canceled=%v", e.FileId, e.Canceled)
}
func (e *EventDataUploadRemove) Summary() string {
	return fmt.Sprintf("count=%d", len(e.FileIds))
}

func (e *EventDataScreenshotAdd) Summary() string {
	return fmt.Sprintf("agent=%d note=%s size=%d", e.AgentId, truncate(e.Note, 40), len(e.Content))
}
func (e *EventDataScreenshotRemove) Summary() string {
	return fmt.Sprintf("screen=%d", e.ScreenId)
}

func (e *EventDataTunnelStart) Summary() string {
	return fmt.Sprintf("agent=%d tunnel=%d type=%d port=%d %s", e.AgentId, e.TunnelId, e.TunnelType, e.Port, truncate(e.Info, 40))
}
func (e *EventDataTunnelStop) Summary() string {
	return fmt.Sprintf("agent=%d tunnel=%d type=%d port=%d", e.AgentId, e.TunnelId, e.TunnelType, e.Port)
}

func (e *EventDataClientConnect) Summary() string {
	return fmt.Sprintf("user=%s", e.Username)
}
func (e *EventDataClientDisconnect) Summary() string {
	return fmt.Sprintf("user=%s", e.Username)
}

func (e *EventDataTargetAdd) Summary() string {
	return fmt.Sprintf("count=%d", len(e.Targets))
}
func (e *EventDataTargetEdit) Summary() string {
	return fmt.Sprintf("target=%d", e.Target.TargetId)
}
func (e *EventDataTargetRemove) Summary() string {
	return fmt.Sprintf("count=%d", len(e.TargetIds))
}

func (e *EventDataPivotCreate) Summary() string {
	return fmt.Sprintf("pivot=%s parent=%d child=%d name=%s", e.PivotId, e.ParentAgentId, e.ChildAgentId, e.PivotName)
}
func (e *EventDataPivotRemove) Summary() string {
	return fmt.Sprintf("pivot=%s", e.PivotId)
}
