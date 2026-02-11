package server

import (
	"AdaptixServer/core/connector"
	"AdaptixServer/core/database"
	"AdaptixServer/core/eventing"
	"AdaptixServer/core/extender"
	"AdaptixServer/core/profile"
	"AdaptixServer/core/utils/safe"
	"io"
	"net"
	"sync"
	"sync/atomic"

	"github.com/Adaptix-Framework/axc2"
	"github.com/gorilla/websocket"
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

type TsParameters struct {
	Interfaces []string
}

type Teamserver struct {
	Profile       *profile.AdaptixProfile
	DBMS          *database.DBMS
	AdaptixServer *connector.TsConnector
	Extender      *extender.AdaptixExtender
	Parameters    TsParameters

	TaskManager   *TaskManager
	Broker        *MessageBroker
	TunnelManager *TunnelManager
	EventManager  *eventing.EventManager

	listener_configs safe.Map // listenerFullName string : listenerInfo extender.ListenerInfo
	agent_configs    safe.Map // agentName string        : agentInfo extender.AgentInfo
	service_configs  safe.Map // serviceName string      : serviceInfo extender.ServiceInfo

	wm_agent_types map[string]string   // agentMark string : agentName string
	wm_listeners   map[string][]string // watermark string : ListenerName string, ListenerType string

	notifications *safe.Slice // 			       : sync_packet interface{}
	Agents        safe.Map    // agentId string      : agent *Agent
	listeners     safe.Map    // listenerName string : listenerData ListenerData
	downloads     safe.Map    // fileId string       : downloadData DownloadData (only active)
	tmp_uploads   safe.Map    // fileId string       : uploadData UploadData
	terminals     safe.Map    // terminalId string   : terminal Terminal
	pivots        *safe.Slice // 			           : PivotData
	otps          safe.Map    // otp string		   : Id string
	builders      safe.Map    // buildId string      : build Build
}

type Agent struct {
	mu       sync.RWMutex
	data     adaptix.AgentData
	Extender adaptix.ExtenderAgent
	Tick     bool
	Active   bool

	HostedTasks       *safe.Queue // taskData TaskData
	HostedTunnelTasks *safe.Queue // taskData TaskData
	HostedTunnelData  *safe.Queue // taskData TaskDataTunnel

	RunningTasks safe.Map // taskId string, taskData TaskData
	RunningJobs  safe.Map // taskId string, list []TaskData

	PivotParent *adaptix.PivotData
	PivotChilds *safe.Slice
}

func (a *Agent) GetData() adaptix.AgentData {
	a.mu.RLock()
	defer a.mu.RUnlock()
	return a.data
}

func (a *Agent) SetData(data adaptix.AgentData) {
	a.mu.Lock()
	defer a.mu.Unlock()
	a.data = data
}

func (a *Agent) UpdateData(fn func(*adaptix.AgentData)) {
	a.mu.Lock()
	defer a.mu.Unlock()
	fn(&a.data)
}

func (a *Agent) Command(args map[string]any) (adaptix.TaskData, adaptix.ConsoleMessageData, error) {
	return a.Extender.CreateCommand(a.GetData(), args)
}

func (a *Agent) ProcessData(packedData []byte) error {
	data := a.GetData()
	decrypted, err := a.Extender.Decrypt(packedData, data.SessionKey)
	if err != nil {
		return err
	}
	return a.Extender.ProcessData(data, decrypted)
}

func (a *Agent) PackData(tasks []adaptix.TaskData) ([]byte, error) {
	data := a.GetData()
	packed, err := a.Extender.PackTasks(data, tasks)
	if err != nil {
		return nil, err
	}
	return a.Extender.Encrypt(packed, data.SessionKey)
}

func (a *Agent) PivotPackData(pivotId string, data []byte) (adaptix.TaskData, error) {
	return a.Extender.PivotPackData(pivotId, data)
}

func (a *Agent) TunnelCallbacks() adaptix.TunnelCallbacks {
	return a.Extender.TunnelCallbacks()
}

func (a *Agent) TerminalCallbacks() adaptix.TerminalCallbacks {
	return a.Extender.TerminalCallbacks()
}

type HookJob struct {
	Sent      bool
	Processed bool
	Job       adaptix.TaskData
	mu        sync.Mutex
}

type TunnelChannel struct {
	channelId int
	protocol  string

	wsconn *websocket.Conn
	conn   net.Conn

	pwSrv *io.PipeWriter
	prSrv *io.PipeReader

	pwTun *io.PipeWriter
	prTun *io.PipeReader

	ingressChan chan []byte
	paused      atomic.Bool
	flowPaused  atomic.Bool
}

type Tunnel struct {
	TaskId string
	Active bool
	Type   int
	Data   adaptix.TunnelData

	listener    net.Listener
	connections safe.Map

	Callbacks adaptix.TunnelCallbacks
}

type Terminal struct {
	TaskId     string
	TerminalId int
	CodePage   int

	agent  *Agent
	mu     sync.Mutex
	closed bool

	wsconn *websocket.Conn

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

	wsconn *websocket.Conn
	mu     sync.Mutex
	closed bool
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
	Data             string `json:"l_data"`
}

type SyncPackerListenerStop struct {
	SpType int `json:"type"`

	ListenerName string `json:"l_name"`
}

/// AGENT

type SyncPackerAgentReg struct {
	SpType int `json:"type"`

	Agent          string   `json:"agent"`
	AX             string   `json:"ax"`
	Listeners      []string `json:"listeners"`
	MultiListeners bool     `json:"multi_listeners"`
}

/// SERVICE

type SyncPackerServiceReg struct {
	SpType int `json:"type"`

	Name string `json:"service"`
	AX   string `json:"ax"`
}

type SyncPackerServiceData struct {
	SpType int `json:"type"`

	Service string `json:"service"`
	Data    string `json:"data"`
}

type SyncPackerAgentNew struct {
	SpType int `json:"type"`

	Id           string `json:"a_id"`
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

	Id           string  `json:"a_id"`
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

	Id []string `json:"a_id"`
}

type SyncPackerAgentTaskRemove struct {
	SpType int `json:"type"`

	TaskId string `json:"a_task_id"`
}

type SyncPackerAgentRemove struct {
	SpType int `json:"type"`

	AgentId string `json:"a_id"`
}

type SyncPackerAgentTaskSync struct {
	SpType int `json:"type"`

	TaskType    int    `json:"a_task_type"`
	TaskId      string `json:"a_task_id"`
	AgentId     string `json:"a_id"`
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

	AgentId     string `json:"a_id"`
	TaskId      string `json:"a_task_id"`
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

	AgentId     string `json:"a_id"`
	TaskId      string `json:"a_task_id"`
	HookId      string `json:"a_hook_id"`
	JobIndex    int    `json:"a_job_index"`
	MessageType int    `json:"a_msg_type"`
	Message     string `json:"a_message"`
	Text        string `json:"a_text"`
	Completed   bool   `json:"a_completed"`
}

type SyncPackerAgentTaskSend struct {
	SpType int `json:"type"`

	TaskId []string `json:"a_task_id"`
}

type SyncPackerAgentConsoleOutput struct {
	SpCreateTime int64 `json:"time"`
	SpType       int   `json:"type"`

	AgentId     string `json:"a_id"`
	MessageType int    `json:"a_msg_type"`
	Message     string `json:"a_message"`
	ClearText   string `json:"a_text"`
}

type SyncPackerAgentErrorCommand struct {
	SpType int `json:"type"`

	AgentId   string `json:"a_id"`
	Cmdline   string `json:"a_cmdline"`
	Message   string `json:"a_message"`
	HookId    string `json:"ax_hook_id"`
	HandlerId string `json:"ax_handler_id"`
}

type SyncPackerAgentLocalCommand struct {
	SpCreateTime int64 `json:"time"`
	SpType       int   `json:"type"`

	AgentId string `json:"a_id"`
	Cmdline string `json:"a_cmdline"`
	Message string `json:"a_message"`
	Text    string `json:"a_text"`
}

type SyncPackerAgentConsoleTaskSync struct {
	SpType int `json:"type"`

	TaskId      string `json:"a_task_id"`
	AgentId     string `json:"a_id"`
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

	AgentId     string `json:"a_id"`
	TaskId      string `json:"a_task_id"`
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
	ParentAgentId string `json:"p_parent_agent_id"`
	ChildAgentId  string `json:"p_child_agent_id"`
}

type SyncPackerPivotDelete struct {
	SpType int `json:"type"`

	PivotId string `json:"p_pivot_id"`
}

/// CHAT

type SyncPackerChatMessage struct {
	SpType int `json:"type"`

	Username string `json:"c_username"`
	Message  string `json:"c_message"`
	Date     int64  `json:"c_date"`
}

/// DOWNLOAD

type SyncPackerDownloadCreate struct {
	SpType int `json:"type"`

	FileId    string `json:"d_file_id"`
	AgentId   string `json:"d_agent_id"`
	AgentName string `json:"d_agent_name"`
	User      string `json:"d_user"`
	Computer  string `json:"d_computer"`
	File      string `json:"d_file"`
	Size      int    `json:"d_size"`
	Date      int64  `json:"d_date"`
}

type SyncPackerDownloadUpdate struct {
	SpType int `json:"type"`

	FileId   string `json:"d_file_id"`
	RecvSize int    `json:"d_recv_size"`
	State    int    `json:"d_state"`
}

type SyncPackerDownloadDelete struct {
	SpType int `json:"type"`

	FileId []string `json:"d_files_id"`
}

type SyncPackerDownloadActual struct {
	SpType int `json:"type"`

	FileId    string `json:"d_file_id"`
	AgentId   string `json:"d_agent_id"`
	AgentName string `json:"d_agent_name"`
	User      string `json:"d_user"`
	Computer  string `json:"d_computer"`
	File      string `json:"d_file"`
	Size      int    `json:"d_size"`
	Date      int64  `json:"d_date"`
	RecvSize  int    `json:"d_recv_size"`
	State     int    `json:"d_state"`
}

/// SCREEN

type SyncPackerScreenshotCreate struct {
	SpType int `json:"type"`

	ScreenId string `json:"s_screen_id"`
	User     string `json:"s_user"`
	Computer string `json:"s_computer"`
	Note     string `json:"s_note"`
	Date     int64  `json:"s_date"`
	Content  []byte `json:"s_content"`
}

type SyncPackerScreenshotUpdate struct {
	SpType int `json:"type"`

	ScreenId string `json:"s_screen_id"`
	Note     string `json:"s_note"`
}

type SyncPackerScreenshotDelete struct {
	SpType int `json:"type"`

	ScreenId string `json:"s_screen_id"`
}

/// CREDS

type SyncPackerCredentials struct {
	CredId   string `json:"c_creds_id"`
	Username string `json:"c_username"`
	Password string `json:"c_password"`
	Realm    string `json:"c_realm"`
	Type     string `json:"c_type"`
	Tag      string `json:"c_tag"`
	Date     int64  `json:"c_date"`
	Storage  string `json:"c_storage"`
	AgentId  string `json:"c_agent_id"`
	Host     string `json:"c_host"`
}

type SyncPackerCredentialsAdd struct {
	SpType int `json:"type"`

	Creds []SyncPackerCredentials `json:"c_creds"`
}

type SyncPackerCredentialsUpdate struct {
	SpType int `json:"type"`

	CredId   string `json:"c_creds_id"`
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

	CredsId []string `json:"c_creds_id"`
}

type SyncPackerCredentialsTag struct {
	SpType int `json:"type"`

	CredsId []string `json:"c_creds_id"`
	Tag     string   `json:"c_tag"`
}

/// TARGETS

type SyncPackerTarget struct {
	TargetId string   `json:"t_target_id"`
	Computer string   `json:"t_computer"`
	Domain   string   `json:"t_domain"`
	Address  string   `json:"t_address"`
	Os       int      `json:"t_os"`
	OsDesk   string   `json:"t_os_desk"`
	Tag      string   `json:"t_tag"`
	Info     string   `json:"t_info"`
	Date     int64    `json:"t_date"`
	Alive    bool     `json:"t_alive"`
	Agents   []string `json:"t_agents"`
}

type SyncPackerTargetsAdd struct {
	SpType int `json:"type"`

	Targets []SyncPackerTarget `json:"t_targets"`
}

type SyncPackerTargetUpdate struct {
	SpType int `json:"type"`

	TargetId string   `json:"t_target_id"`
	Computer string   `json:"t_computer"`
	Domain   string   `json:"t_domain"`
	Address  string   `json:"t_address"`
	Os       int      `json:"t_os"`
	OsDesk   string   `json:"t_os_desk"`
	Tag      string   `json:"t_tag"`
	Info     string   `json:"t_info"`
	Date     int64    `json:"t_date"`
	Alive    bool     `json:"t_alive"`
	Agents   []string `json:"t_agents"`
}

type SyncPackerTargetDelete struct {
	SpType int `json:"type"`

	TargetsId []string `json:"t_target_id"`
}

type SyncPackerTargetTag struct {
	SpType int `json:"type"`

	TargetsId []string `json:"t_targets_id"`
	Tag       string   `json:"t_tag"`
}

/// BROWSER

type SyncPacketBrowserDisks struct {
	SpType int `json:"type"`

	AgentId     string `json:"b_agent_id"`
	Time        int64  `json:"b_time"`
	MessageType int    `json:"b_msg_type"`
	Message     string `json:"b_message"`
	Data        string `json:"b_data"`
}

type SyncPacketBrowserFiles struct {
	SpType int `json:"type"`

	AgentId     string `json:"b_agent_id"`
	Time        int64  `json:"b_time"`
	MessageType int    `json:"b_msg_type"`
	Message     string `json:"b_message"`
	Path        string `json:"b_path"`
	Data        string `json:"b_data"`
}

type SyncPacketBrowserFilesStatus struct {
	SpType int `json:"type"`

	AgentId     string `json:"b_agent_id"`
	Time        int64  `json:"b_time"`
	MessageType int    `json:"b_msg_type"`
	Message     string `json:"b_message"`
}

type SyncPacketBrowserProcess struct {
	SpType int `json:"type"`

	AgentId     string `json:"b_agent_id"`
	Time        int64  `json:"b_time"`
	MessageType int    `json:"b_msg_type"`
	Message     string `json:"b_message"`
	Data        string `json:"b_data"`
}

/// TUNNEL

type SyncPackerTunnelCreate struct {
	SpType int `json:"type"`

	TunnelId  string `json:"p_tunnel_id"`
	AgentId   string `json:"p_agent_id"`
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
}

type SyncPackerTunnelEdit struct {
	SpType int `json:"type"`

	TunnelId string `json:"p_tunnel_id"`
	Info     string `json:"p_info"`
}

type SyncPackerTunnelDelete struct {
	SpType int `json:"type"`

	TunnelId string `json:"p_tunnel_id"`
}
