package server

import (
	"AdaptixServer/core/axscript"
	"AdaptixServer/core/connector"
	"AdaptixServer/core/database"
	"AdaptixServer/core/eventing"
	"AdaptixServer/core/extender"
	"AdaptixServer/core/profile"
	"AdaptixServer/core/utils/idgen"
	"AdaptixServer/core/utils/token"
	"context"
	"fmt"
	"io"
	"net"
	"os"
	"sync"
	"sync/atomic"

	"github.com/Adaptix-Framework/axc2/v2"
	"github.com/Adaptix-Framework/axsafe"
)

const (
	CONSOLE_OUT_LOCAL         = 1
	CONSOLE_OUT_LOCAL_INFO    = 2
	CONSOLE_OUT_LOCAL_ERROR   = 3
	CONSOLE_OUT_LOCAL_SUCCESS = 4
	CONSOLE_OUT_INFO          = 5
	CONSOLE_OUT_ERROR         = 6
	CONSOLE_OUT_SUCCESS       = 7
	CONSOLE_OUT               = 10
)

type Paths struct {
	Path           string
	DataPath       string
	DbPath         string
	ListenerPath   string
	DownloadPath   string
	UploadPath     string
	ScreenshotPath string
	PayloadPath    string
}

func initPaths() (Paths, error) {
	cwd, err := os.Getwd()
	if err != nil {
		return Paths{}, fmt.Errorf("getwd: %w", err)
	}

	p := Paths{
		Path:           cwd,
		DataPath:       cwd + "/data",
		DbPath:         cwd + "/data/adaptixserver.db",
		ListenerPath:   cwd + "/data/listener",
		DownloadPath:   cwd + "/data/download",
		UploadPath:     cwd + "/data/tmp_upload",
		ScreenshotPath: cwd + "/data/screenshot",
		PayloadPath:    cwd + "/data/payloads",
	}

	if err := ensureDir(p.DataPath); err != nil {
		return Paths{}, err
	}
	if err := ensureDir(p.ListenerPath); err != nil {
		return Paths{}, err
	}
	if err := ensureDir(p.DownloadPath); err != nil {
		return Paths{}, err
	}

	_ = os.RemoveAll(p.UploadPath)
	if err := ensureDir(p.UploadPath); err != nil {
		return Paths{}, err
	}

	if err := ensureDir(p.ScreenshotPath); err != nil {
		return Paths{}, err
	}
	if err := ensureDir(p.PayloadPath); err != nil {
		return Paths{}, err
	}
	return p, nil
}

func ensureDir(path string) error {
	if _, err := os.Stat(path); os.IsNotExist(err) {
		if err := os.MkdirAll(path, 0o750); err != nil {
			return fmt.Errorf("create %s: %w", path, err)
		}
	}
	return nil
}

type TsParameters struct {
	Interfaces []string
}

type Teamserver struct {
	ctx    context.Context
	cancel context.CancelFunc

	Profile       *profile.AdaptixProfile
	DBMS          *database.DBMS
	AdaptixServer *connector.TsConnector
	Extender      *extender.AdaptixExtender
	Parameters    TsParameters
	Paths         Paths

	TaskManager   *TaskManager
	Broker        *MessageBroker
	TunnelManager *TunnelManager
	FrameManager  *FrameManager
	EventManager  *eventing.EventManager
	ScriptManager *axscript.ScriptManager
	EventHandlers *EventHandlerRegistry
	LogManager    *LogManager

	listener_configs axsafe.Map[string, extender.ListenerInfo] // listenerFullName
	agent_configs    axsafe.Map[string, extender.AgentInfo]    // agentName
	service_configs  axsafe.Map[string, extender.ServiceInfo]  // serviceName

	serviceWaiters sync.Map

	wm_agent_types axsafe.Map[string, string]   // agentMark string : agentName string
	wm_listeners   axsafe.Map[string, []string] // watermark string : ListenerName string, ListenerType string

	notifications *axsafe.Slice                            // sync_packet interface{}
	Agents        axsafe.Map[int64, *adaptix.Agent]        // agentId
	agentsUid     axsafe.Map[string, int64]                // hex(uid) → agentId
	listeners     axsafe.Map[string, adaptix.ListenerData] // listenerName
	downloads     axsafe.Map[int64, adaptix.TransferData]  // fileId
	uploads       axsafe.Map[int64, adaptix.TransferData]  // fileId
	tmp_uploads   axsafe.Map[int64, string]                // fileId
	terminals     axsafe.Map[int64, *Terminal]             // terminalId
	pivots        *axsafe.Slice                            // PivotData
	groups        axsafe.Map[int64, adaptix.GroupData]     // GroupId → GroupData
	OTPManager    *token.OTPManager
	builders      axsafe.Map[string, *AgentBuilder] // buildId

	tickedAgents axsafe.Set[int64] // agentIds pending tick broadcast
	tickNotify   chan struct{}     // signal for tick

	IdGen *idgen.Generator
}

type TunnelChannel struct {
	channelId int64
	protocol  string

	wsconn    adaptix.WebSocketConn
	wsWriteMu sync.Mutex
	conn      net.Conn

	pwSrv *io.PipeWriter
	prSrv *io.PipeReader

	pwTun *io.PipeWriter
	prTun *io.PipeReader

	ingressChan    chan []byte
	ingressOnce    sync.Once
	ingressClosed  atomic.Bool
	paused         atomic.Bool
	flowPaused     atomic.Bool
	resumed        atomic.Bool
	skipAgentClose atomic.Bool
}

func (tc *TunnelChannel) CloseIngress() {
	tc.ingressOnce.Do(func() {
		tc.ingressClosed.Store(true)
		if tc.ingressChan != nil {
			close(tc.ingressChan)
		}
	})
}

type Tunnel struct {
	mu     sync.RWMutex
	TaskId int64
	Active bool
	Type   int
	Data   adaptix.TunnelData

	listener net.Listener

	Callbacks adaptix.TunnelCallbacks

	BytesSent atomic.Int64
	BytesRecv atomic.Int64
}

type Terminal struct {
	TaskId     int64
	TerminalId int64
	CodePage   int

	agentId int64
	mu      sync.Mutex
	closed  bool

	resumed        atomic.Bool
	skipAgentClose atomic.Bool

	wsconn    adaptix.WebSocketConn
	wsWriteMu sync.Mutex

	pwSrv *io.PipeWriter
	prSrv *io.PipeReader

	pwTun *io.PipeWriter
	prTun *io.PipeReader

	Callbacks adaptix.TerminalCallbacks
}

type AgentBuilder struct {
	Id            string
	Name          string
	ListenersName []string
	Config        string

	wsconn adaptix.WebSocketConn
	mu     sync.Mutex
	closed bool
	cancel context.CancelFunc
}

////////////////////////////////////////////////////////////////////////////////////////////

// SyncPacket

type SyncPackerStart struct {
	SpType int `json:"type"`

	Count     int      `json:"count"`
	Addresses []string `json:"interfaces"`
}

type SyncPackerFinish struct {
	SpType int `json:"type"`
}

type SyncPackerBatch struct {
	SpType  int           `json:"type"`
	Packets []interface{} `json:"packets"`
}

type SyncPackerCategoryBatch struct {
	SpType   int           `json:"type"`
	Category string        `json:"category"` // "agents", "events", "console", "tasks", etc.
	Packets  []interface{} `json:"packets"`
}

type SpNotification struct {
	Type int `json:"type"`

	NotifyType int    `json:"event_type"`
	Date       int64  `json:"date"`
	Message    string `json:"message"`
}

/// LISTENER

type SyncPackerListenerReg struct {
	SpType int `json:"type"`

	Name     string `json:"l_name"`
	Protocol string `json:"l_protocol"`
	Type     string `json:"l_type"`
	AX       string `json:"ax"`
}

type SyncPackerListenerStart struct {
	SpType int `json:"type"`

	ListenerName     string `json:"l_name"`
	ListenerRegName  string `json:"l_reg_name"`
	ListenerProtocol string `json:"l_protocol"`
	ListenerType     string `json:"l_type"`
	BindHost         string `json:"l_bind_host"`
	BindPort         string `json:"l_bind_port"`
	AgentAddrs       string `json:"l_agent_addr"`
	CreateTime       int64  `json:"l_create_time"`
	ListenerStatus   string `json:"l_status"`
	Tags             string `json:"l_tags"`
	Data             string `json:"l_data"`
}

type SyncPackerListenerStop struct {
	SpType int `json:"type"`

	ListenerName string `json:"l_name"`
}

/// AGENT

type SyncPackerAgentReg struct {
	SpType int `json:"type"`

	Agent          string           `json:"agent"`
	AX             string           `json:"ax"`
	Listeners      []string         `json:"listeners"`
	MultiListeners bool             `json:"multi_listeners"`
	Groups         []AxCommandBatch `json:"groups"`
}

/// SERVICE

type SyncPackerServiceReg struct {
	SpType int `json:"type"`

	Name string `json:"service"`
	AX   string `json:"ax"`
}

type SyncPackerPluginServiceData struct {
	SpType int `json:"type"`

	Service string `json:"service"`
	Data    string `json:"data"`
}

type SyncPackerPluginAgentData struct {
	SpType int `json:"type"`

	AgentId   int64  `json:"agent_id"`
	AgentType string `json:"agent_type"`
	Data      string `json:"data"`
}

type SyncPackerPluginListenerData struct {
	SpType int `json:"type"`

	Listener     string `json:"listener"`
	ListenerType string `json:"listener_type"`
	Data         string `json:"data"`
}

type SyncPackerAgentNew struct {
	SpType int `json:"type"`

	Id           int64  `json:"a_id"`
	Name         string `json:"a_name"`
	Listener     string `json:"a_listener"`
	Async        bool   `json:"a_async"`
	ExternalIP   string `json:"a_external_ip"`
	InternalIP   string `json:"a_internal_ip"`
	GmtOffset    int    `json:"a_gmt_offset"`
	WorkingTime  int    `json:"a_workingtime"`
	KillDate     int    `json:"a_killdate"`
	Sleep        uint   `json:"a_sleep"`
	Jitter       uint   `json:"a_jitter"`
	ACP          int    `json:"a_acp"`
	OemCP        int    `json:"a_oemcp"`
	Pid          string `json:"a_pid"`
	Tid          string `json:"a_tid"`
	Arch         string `json:"a_arch"`
	Elevated     bool   `json:"a_elevated"`
	Process      string `json:"a_process"`
	Os           int    `json:"a_os"`
	OsDesc       string `json:"a_os_desc"`
	Domain       string `json:"a_domain"`
	Computer     string `json:"a_computer"`
	Username     string `json:"a_username"`
	Impersonated string `json:"a_impersonated"`
	CreateTime   int64  `json:"a_create_time"`
	LastTick     int    `json:"a_last_tick"`
	Tags         string `json:"a_tags"`
	Mark         string `json:"a_mark"`
	Color        string `json:"a_color"`
}

type SyncPackerAgentUpdate struct {
	SpType int `json:"type"`

	Id           int64   `json:"a_id"`
	Sleep        *uint   `json:"a_sleep,omitempty"`
	Jitter       *uint   `json:"a_jitter,omitempty"`
	WorkingTime  *int    `json:"a_workingtime,omitempty"`
	KillDate     *int    `json:"a_killdate,omitempty"`
	Impersonated *string `json:"a_impersonated,omitempty"`
	Tags         *string `json:"a_tags,omitempty"`
	Mark         *string `json:"a_mark,omitempty"`
	Color        *string `json:"a_color,omitempty"`
	InternalIP   *string `json:"a_internal_ip,omitempty"`
	ExternalIP   *string `json:"a_external_ip,omitempty"`
	GmtOffset    *int    `json:"a_gmt_offset,omitempty"`
	ACP          *int    `json:"a_acp,omitempty"`
	OemCP        *int    `json:"a_oemcp,omitempty"`
	Pid          *string `json:"a_pid,omitempty"`
	Tid          *string `json:"a_tid,omitempty"`
	Arch         *string `json:"a_arch,omitempty"`
	Elevated     *bool   `json:"a_elevated,omitempty"`
	Process      *string `json:"a_process,omitempty"`
	Os           *int    `json:"a_os,omitempty"`
	OsDesc       *string `json:"a_os_desc,omitempty"`
	Domain       *string `json:"a_domain,omitempty"`
	Computer     *string `json:"a_computer,omitempty"`
	Username     *string `json:"a_username,omitempty"`
	Listener     *string `json:"a_listener,omitempty"`
}

type SyncPackerAgentTick struct {
	SpType int `json:"type"`

	Id []int64 `json:"a_id"`
}

type SyncPackerAgentTaskRemove struct {
	SpType int `json:"type"`

	TaskId int64 `json:"a_task_id"`
}

type SyncPackerAgentRemove struct {
	SpType int `json:"type"`

	AgentId int64 `json:"a_id"`
}

type SyncPackerAgentTaskSync struct {
	SpType int `json:"type"`

	TaskType    int    `json:"a_task_type"`
	TaskId      int64  `json:"a_task_id"`
	AgentId     int64  `json:"a_id"`
	Client      string `json:"a_client"`
	User        string `json:"a_user"`
	Computer    string `json:"a_computer"`
	CmdLine     string `json:"a_cmdline"`
	StartTime   int64  `json:"a_start_time"`
	FinishTime  int64  `json:"a_finish_time"`
	MessageType int    `json:"a_msg_type"`
	Message     string `json:"a_message"`
	Text        string `json:"a_text"`
	Completed   bool   `json:"a_completed"`
}

type SyncPackerAgentTaskUpdate struct {
	SpType int `json:"type"`

	AgentId     int64  `json:"a_id"`
	TaskId      int64  `json:"a_task_id"`
	HandlerId   string `json:"a_handler_id"`
	TaskType    int    `json:"a_task_type"`
	FinishTime  int64  `json:"a_finish_time"`
	MessageType int    `json:"a_msg_type"`
	Message     string `json:"a_message"`
	Text        string `json:"a_text"`
	Completed   bool   `json:"a_completed"`
}

type SyncPackerAgentTaskHook struct {
	SpType int `json:"type"`

	AgentId     int64  `json:"a_id"`
	TaskId      int64  `json:"a_task_id"`
	HookId      string `json:"a_hook_id"`
	JobIndex    int    `json:"a_job_index"`
	MessageType int    `json:"a_msg_type"`
	Message     string `json:"a_message"`
	Text        string `json:"a_text"`
	Completed   bool   `json:"a_completed"`
}

type SyncPackerAgentTaskSend struct {
	SpType int `json:"type"`

	TaskId []int64 `json:"a_task_id"`
}

type SyncPackerAgentConsoleOutput struct {
	SpCreateTime int64 `json:"time"`
	SpType       int   `json:"type"`

	AgentId     int64  `json:"a_id"`
	MessageType int    `json:"a_msg_type"`
	Message     string `json:"a_message"`
	ClearText   string `json:"a_text"`
}

type SyncPackerAgentErrorCommand struct {
	SpType int `json:"type"`

	AgentId   int64  `json:"a_id"`
	Cmdline   string `json:"a_cmdline"`
	Message   string `json:"a_message"`
	HookId    string `json:"ax_hook_id"`
	HandlerId string `json:"ax_handler_id"`
}

type SyncPackerAgentLocalCommand struct {
	SpCreateTime int64 `json:"time"`
	SpType       int   `json:"type"`

	AgentId int64  `json:"a_id"`
	Cmdline string `json:"a_cmdline"`
	Message string `json:"a_message"`
	Text    string `json:"a_text"`
}

type SyncPackerAgentConsoleTaskSync struct {
	SpType int `json:"type"`

	TaskId      int64  `json:"a_task_id"`
	AgentId     int64  `json:"a_id"`
	Client      string `json:"a_client"`
	CmdLine     string `json:"a_cmdline"`
	StartTime   int64  `json:"a_start_time"`
	FinishTime  int64  `json:"a_finish_time"`
	MessageType int    `json:"a_msg_type"`
	Message     string `json:"a_message"`
	Text        string `json:"a_text"`
	Completed   bool   `json:"a_completed"`
}

type SyncPackerAgentConsoleTaskUpd struct {
	SpType int `json:"type"`

	AgentId     int64  `json:"a_id"`
	TaskId      int64  `json:"a_task_id"`
	FinishTime  int64  `json:"a_finish_time"`
	MessageType int    `json:"a_msg_type"`
	Message     string `json:"a_message"`
	Text        string `json:"a_text"`
	Completed   bool   `json:"a_completed"`
}

/// PIVOT

type SyncPackerPivotCreate struct {
	SpType int `json:"type"`

	PivotId       string `json:"p_pivot_id"`
	PivotName     string `json:"p_pivot_name"`
	ParentAgentId int64  `json:"p_parent_agent_id"`
	ChildAgentId  int64  `json:"p_child_agent_id"`
}

type SyncPackerPivotDelete struct {
	SpType int `json:"type"`

	PivotId string `json:"p_pivot_id"`
}

/// CHAT

type SyncPackerChatMessage struct {
	SpType int `json:"type"`

	Id          int64  `json:"c_id"`
	Username    string `json:"c_username"`
	Message     string `json:"c_message"`
	Date        int64  `json:"c_date"`
	Edited      bool   `json:"c_edited"`
	Deleted     bool   `json:"c_deleted"`
	DeletedDate int64  `json:"c_deleted_date"`
	Reactions   string `json:"c_reactions"`
	ReplyToId   int64  `json:"c_reply_to_id"`
	ReplyToName string `json:"c_reply_to_name"`
}

type SyncPackerChatEdit struct {
	SpType int `json:"type"`

	Id      int64  `json:"c_id"`
	Message string `json:"c_message"`
}

type SyncPackerChatDelete struct {
	SpType int `json:"type"`

	Id int64 `json:"c_id"`
}

type SyncPackerChatReaction struct {
	SpType int `json:"type"`

	Id        int64  `json:"c_id"`
	Reactions string `json:"c_reactions"`
}

type SyncPackerChatTodo struct {
	SpType int `json:"type"`

	Content   string `json:"c_content"`
	UpdatedBy string `json:"c_updated_by"`
	UpdatedAt int64  `json:"c_updated_at"`
}

/// TRANSFER (download / upload)

const (
	TRANSFER_DOWNLOAD = 1
	TRANSFER_UPLOAD   = 2
)

type SyncPackerTransferCreate struct {
	SpType       int `json:"type"`
	TransferType int `json:"t_type"`

	FileId       int64  `json:"t_file_id"`
	AgentId      int64  `json:"t_agent_id"`
	AgentName    string `json:"t_agent_name"`
	User         string `json:"t_user"`
	Computer     string `json:"t_computer"`
	File         string `json:"t_file"`
	Size         int64  `json:"t_size"`
	Date         int64  `json:"t_date"`
	Tag          string `json:"t_tag"`
	Cancellable  bool   `json:"t_cancellable"`
	Kind         int    `json:"t_kind"`
	ArtifactName string `json:"t_artifact_name,omitempty"`
	ArtifactType string `json:"t_artifact_type,omitempty"`
}

type SyncPackerTransferUpdate struct {
	SpType       int `json:"type"`
	TransferType int `json:"t_type"`

	FileId   int64 `json:"t_file_id"`
	Progress int64 `json:"t_progress"`
	State    int   `json:"t_state"`
}

type SyncPackerTransferDelete struct {
	SpType       int `json:"type"`
	TransferType int `json:"t_type"`

	FileId []int64 `json:"t_files_id"`
}

type SyncPackerTransferActual struct {
	SpType       int `json:"type"`
	TransferType int `json:"t_type"`

	FileId       int64  `json:"t_file_id"`
	AgentId      int64  `json:"t_agent_id"`
	AgentName    string `json:"t_agent_name"`
	User         string `json:"t_user"`
	Computer     string `json:"t_computer"`
	File         string `json:"t_file"`
	Size         int64  `json:"t_size"`
	Date         int64  `json:"t_date"`
	Progress     int64  `json:"t_progress"`
	State        int    `json:"t_state"`
	Tag          string `json:"t_tag"`
	Cancellable  bool   `json:"t_cancellable"`
	Kind         int    `json:"t_kind"`
	ArtifactName string `json:"t_artifact_name,omitempty"`
	ArtifactType string `json:"t_artifact_type,omitempty"`
}

type SyncPackerTransferTag struct {
	SpType       int `json:"type"`
	TransferType int `json:"t_type"`

	FileId []int64 `json:"t_files_id"`
	Tag    string  `json:"t_tag"`
}

/// SCREEN

type SyncPackerScreenshotCreate struct {
	SpType int `json:"type"`

	ScreenId int64  `json:"s_screen_id"`
	AgentId  int64  `json:"s_agent_id"`
	User     string `json:"s_user"`
	Computer string `json:"s_computer"`
	Note     string `json:"s_note"`
	Date     int64  `json:"s_date"`
	Content  []byte `json:"s_content"`
}

type SyncPackerScreenshotUpdate struct {
	SpType int `json:"type"`

	ScreenId int64  `json:"s_screen_id"`
	Note     string `json:"s_note"`
}

type SyncPackerScreenshotDelete struct {
	SpType int `json:"type"`

	ScreenId int64 `json:"s_screen_id"`
}

/// CREDS

type SyncPackerCredentials struct {
	CredId   int64  `json:"c_creds_id"`
	Username string `json:"c_username"`
	Password string `json:"c_password"`
	Realm    string `json:"c_realm"`
	Type     string `json:"c_type"`
	Tag      string `json:"c_tag"`
	Date     int64  `json:"c_date"`
	Storage  string `json:"c_storage"`
	AgentId  int64  `json:"c_agent_id"`
	Host     string `json:"c_host"`
}

type SyncPackerCredentialsAdd struct {
	SpType int `json:"type"`

	Creds []SyncPackerCredentials `json:"c_creds"`
}

type SyncPackerCredentialsUpdate struct {
	SpType int `json:"type"`

	CredId   int64  `json:"c_creds_id"`
	Username string `json:"c_username"`
	Password string `json:"c_password"`
	Realm    string `json:"c_realm"`
	Type     string `json:"c_type"`
	Tag      string `json:"c_tag"`
	Storage  string `json:"c_storage"`
	Host     string `json:"c_host"`
}

type SyncPackerCredentialsDelete struct {
	SpType int `json:"type"`

	CredsId []int64 `json:"c_creds_id"`
}

type SyncPackerCredentialsTag struct {
	SpType int `json:"type"`

	CredsId []int64 `json:"c_creds_id"`
	Tag     string  `json:"c_tag"`
}

/// TARGETS

type SyncPackerTarget struct {
	TargetId int64   `json:"t_target_id"`
	Computer string  `json:"t_computer"`
	Domain   string  `json:"t_domain"`
	Address  string  `json:"t_address"`
	Os       int     `json:"t_os"`
	OsDesk   string  `json:"t_os_desk"`
	Tag      string  `json:"t_tag"`
	Info     string  `json:"t_info"`
	Date     int64   `json:"t_date"`
	Alive    bool    `json:"t_alive"`
	Agents   []int64 `json:"t_agents"`
}

type SyncPackerTargetsAdd struct {
	SpType int `json:"type"`

	Targets []SyncPackerTarget `json:"t_targets"`
}

type SyncPackerTargetUpdate struct {
	SpType int `json:"type"`

	TargetId int64   `json:"t_target_id"`
	Computer string  `json:"t_computer"`
	Domain   string  `json:"t_domain"`
	Address  string  `json:"t_address"`
	Os       int     `json:"t_os"`
	OsDesk   string  `json:"t_os_desk"`
	Tag      string  `json:"t_tag"`
	Info     string  `json:"t_info"`
	Date     int64   `json:"t_date"`
	Alive    bool    `json:"t_alive"`
	Agents   []int64 `json:"t_agents"`
}

type SyncPackerTargetDelete struct {
	SpType int `json:"type"`

	TargetsId []int64 `json:"t_target_id"`
}

type SyncPackerTargetTag struct {
	SpType int `json:"type"`

	TargetsId []int64 `json:"t_targets_id"`
	Tag       string  `json:"t_tag"`
}

/// BROWSER

type SyncPacketBrowserDisks struct {
	SpType int `json:"type"`

	AgentId     int64  `json:"b_agent_id"`
	Time        int64  `json:"b_time"`
	MessageType int    `json:"b_msg_type"`
	Message     string `json:"b_message"`
	Data        string `json:"b_data"`
}

type SyncPacketBrowserFiles struct {
	SpType int `json:"type"`

	AgentId     int64  `json:"b_agent_id"`
	Time        int64  `json:"b_time"`
	MessageType int    `json:"b_msg_type"`
	Message     string `json:"b_message"`
	Path        string `json:"b_path"`
	Data        string `json:"b_data"`
}

type SyncPacketBrowserFilesStatus struct {
	SpType int `json:"type"`

	AgentId     int64  `json:"b_agent_id"`
	Time        int64  `json:"b_time"`
	MessageType int    `json:"b_msg_type"`
	Message     string `json:"b_message"`
}

type SyncPacketBrowserProcess struct {
	SpType int `json:"type"`

	AgentId     int64  `json:"b_agent_id"`
	Time        int64  `json:"b_time"`
	MessageType int    `json:"b_msg_type"`
	Message     string `json:"b_message"`
	Data        string `json:"b_data"`
}

/// TUNNEL

type SyncPackerTunnelCreate struct {
	SpType int `json:"type"`

	TunnelId  int64  `json:"p_tunnel_id"`
	AgentId   int64  `json:"p_agent_id"`
	Computer  string `json:"p_computer"`
	Username  string `json:"p_username"`
	Process   string `json:"p_process"`
	Type      string `json:"p_type"`
	Info      string `json:"p_info"`
	Interface string `json:"p_interface"`
	Port      string `json:"p_port"`
	Client    string `json:"p_client"`
	Fhost     string `json:"p_fhost"`
	Fport     string `json:"p_fport"`
	Date      int64  `json:"p_date"`
	BytesSent int64  `json:"p_bytes_sent"`
	BytesRecv int64  `json:"p_bytes_recv"`
	Active    bool   `json:"p_active"`
}

type SyncPackerTunnelEdit struct {
	SpType int `json:"type"`

	TunnelId int64  `json:"p_tunnel_id"`
	Info     string `json:"p_info"`
	Active   bool   `json:"p_active"`
}

type SyncPackerTunnelDelete struct {
	SpType int `json:"type"`

	TunnelId int64 `json:"p_tunnel_id"`
}

/// AXSCRIPT

type SyncPackerAxScriptData struct {
	SpType  int              `json:"type"`
	Name    string           `json:"name"`
	Content string           `json:"content"`
	Groups  []AxCommandBatch `json:"groups"`
}

type SyncPackerAxScriptList struct {
	SpType int                      `json:"type"`
	Items  []map[string]interface{} `json:"items"`
}

type SyncPackerEventHandlers struct {
	SpType int                      `json:"type"`
	Items  []map[string]interface{} `json:"items"`
}

type AxCommandBatch struct {
	Agent    string `json:"agent"`
	Listener string `json:"listener"`
	Os       int    `json:"os"`
	Commands string `json:"commands"`
}

/// PAYLOADS

type SyncPackerPayloadCreate struct {
	SpType int `json:"type"`

	PayloadId int64    `json:"p_id"`
	Name      string   `json:"p_name"`
	AgentType string   `json:"p_type"`
	Artifact  string   `json:"p_artifact"`
	Arch      string   `json:"p_arch"`
	Listeners []string `json:"p_listeners"`
	Size      int64    `json:"p_size"`
	Sha1      string   `json:"p_sha1"`
	Sha256    string   `json:"p_sha256"`
	Md5       string   `json:"p_md5"`
	Creator   string   `json:"p_creator"`
	Created   int64    `json:"p_date"`
	Hidden    bool     `json:"p_hidden"`
	Filename  string   `json:"p_filename"`
	BuildId   string   `json:"p_build_id,omitempty"`
	Watermark string   `json:"p_watermark,omitempty"`
	Notes     string   `json:"p_notes,omitempty"`
	Uid       string   `json:"p_uid,omitempty"`
	Color     string   `json:"p_color,omitempty"`
	Missing   bool     `json:"p_missing,omitempty"`
}

type SyncPackerPayloadUpdate struct {
	SpType int `json:"type"`

	PayloadIds []int64 `json:"p_ids"`
	Hidden     bool    `json:"p_hidden"`
}

type SyncPackerPayloadDelete struct {
	SpType int `json:"type"`

	PayloadIds []int64 `json:"p_ids"`
}

type SyncPackerPayloadEdit struct {
	SpType int `json:"type"`

	PayloadId int64    `json:"p_id"`
	Name      string   `json:"p_name"`
	AgentType string   `json:"p_type"`
	Artifact  string   `json:"p_artifact"`
	Arch      string   `json:"p_arch"`
	Listeners []string `json:"p_listeners"`
	Size      int64    `json:"p_size"`
	Sha1      string   `json:"p_sha1"`
	Sha256    string   `json:"p_sha256"`
	Md5       string   `json:"p_md5"`
	Creator   string   `json:"p_creator"`
	Created   int64    `json:"p_date"`
	Hidden    bool     `json:"p_hidden"`
	Filename  string   `json:"p_filename"`
	BuildId   string   `json:"p_build_id,omitempty"`
	Watermark string   `json:"p_watermark,omitempty"`
	Notes     string   `json:"p_notes,omitempty"`
	Uid       string   `json:"p_uid,omitempty"`
	Color     string   `json:"p_color,omitempty"`
	Missing   bool     `json:"p_missing,omitempty"`
}

/// LOGS

type SyncPackerLogBatch struct {
	SpType int                `json:"type"`
	Items  []adaptix.LogEntry `json:"items"`
}

/// GROUPS

type SyncPackerGroupCreate struct {
	SpType        int     `json:"type"`
	GroupId       int64   `json:"g_group_id"`
	ParentGroupId int64   `json:"g_parent_group_id"`
	GroupName     string  `json:"g_group_name"`
	Scope         string  `json:"g_scope"`
	Members       []int64 `json:"g_members"`
}

type SyncPackerGroupRename struct {
	SpType    int    `json:"type"`
	GroupId   int64  `json:"g_group_id"`
	GroupName string `json:"g_group_name"`
}

type SyncPackerGroupDelete struct {
	SpType  int   `json:"type"`
	GroupId int64 `json:"g_group_id"`
}

type SyncPackerGroupMembers struct {
	SpType  int     `json:"type"`
	GroupId int64   `json:"g_group_id"`
	Add     []int64 `json:"g_add"`
	Remove  []int64 `json:"g_remove"`
}

type SyncPackerGroupReparent struct {
	SpType      int   `json:"type"`
	GroupId     int64 `json:"g_group_id"`
	NewParentId int64 `json:"g_new_parent_id"`
}
