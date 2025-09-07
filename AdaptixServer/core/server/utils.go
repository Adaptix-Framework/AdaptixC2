package server

import (
	"AdaptixServer/core/connector"
	"AdaptixServer/core/database"
	"AdaptixServer/core/extender"
	"AdaptixServer/core/profile"
	"AdaptixServer/core/utils/safe"
	"io"
	"net"
	"sync"

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

const (
	TYPE_TASK       = 1
	TYPE_BROWSER    = 2
	TYPE_JOB        = 3
	TYPE_TUNNEL     = 4
	TYPE_PROXY_DATA = 5
)

const (
	TUNNEL_SOCKS4      = 1
	TUNNEL_SOCKS5      = 2
	TUNNEL_SOCKS5_AUTH = 3
	TUNNEL_LPORTFWD    = 4
	TUNNEL_RPORTFWD    = 5
)

// TeamServer

type Client struct {
	username   string
	synced     bool
	lockSocket *sync.Mutex
	socket     *websocket.Conn
	tmp_store  *safe.Slice
}

type TsParameters struct {
	Interfaces []string
}

type Teamserver struct {
	Profile       *profile.AdaptixProfile
	DBMS          *database.DBMS
	AdaptixServer *connector.TsConnector
	Extender      *extender.AdaptixExtender
	Parameters    TsParameters

	listener_configs safe.Map // listenerFullName string : listenerInfo extender.ListenerInfo
	agent_configs    safe.Map // agentName string        : agentInfo extender.AgentInfo

	wm_agent_types map[string]string   // agentMark string : agentName string
	wm_listeners   map[string][]string // watermark string : ListenerName string, ListenerType string

	events      *safe.Slice // 			           : sync_packet interface{}
	clients     safe.Map    // username string     : socket *websocket.Conn
	agents      safe.Map    // agentId string      : agent *Agent
	listeners   safe.Map    // listenerName string : listenerData ListenerData
	messages    *safe.Slice //  				   : chatData ChatData
	downloads   safe.Map    // fileId string       : downloadData DownloadData
	tmp_uploads safe.Map    // fileId string       : uploadData UploadData
	screenshots safe.Map    // screeId string      : screenData ScreenDataData
	credentials *safe.Slice
	targets     *safe.Slice
	tunnels     safe.Map    // tunnelId string     : tunnel Tunnel
	terminals   safe.Map    // terminalId string   : terminal Terminal
	pivots      *safe.Slice // 			           : PivotData
	otps        safe.Map    // otp string		   : Id string
}

type Agent struct {
	Data   adaptix.AgentData
	Tick   bool
	Active bool

	OutConsole *safe.Slice //  sync_packet interface{}

	HostedTasks       *safe.Queue // taskData TaskData
	HostedTunnelTasks *safe.Queue // taskData TaskData
	HostedTunnelData  *safe.Queue // taskData TaskDataTunnel

	RunningTasks   safe.Map // taskId string, taskData TaskData
	RunningJobs    safe.Map // taskId string, list []TaskData
	CompletedTasks safe.Map // taskId string, taskData TaskData

	PivotParent *adaptix.PivotData
	PivotChilds *safe.Slice
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
}

type Tunnel struct {
	TaskId string
	Active bool
	Type   int
	Data   adaptix.TunnelData

	listener    net.Listener
	connections safe.Map

	handlerConnectTCP func(channelId int, addr string, port int) adaptix.TaskData
	handlerConnectUDP func(channelId int, addr string, port int) adaptix.TaskData
	handlerWriteTCP   func(channelId int, data []byte) adaptix.TaskData
	handlerWriteUDP   func(channelId int, data []byte) adaptix.TaskData
	handlerClose      func(channelId int) adaptix.TaskData
	handlerReverse    func(tunnelId int, port int) adaptix.TaskData
}

type Terminal struct {
	TaskId     string
	TerminalId int

	agent *Agent

	wsconn *websocket.Conn

	pwSrv *io.PipeWriter
	prSrv *io.PipeReader

	pwTun *io.PipeWriter
	prTun *io.PipeReader

	handlerStart func(terminalId int, program string, sizeH int, sizeW int) (adaptix.TaskData, error)
	handlerWrite func(terminalId int, data []byte) (adaptix.TaskData, error)
	handlerClose func(terminalId int) (adaptix.TaskData, error)
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

type SpEvent struct {
	Type int `json:"type"`

	EventType int    `json:"event_type"`
	Date      int64  `json:"date"`
	Message   string `json:"message"`
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

	Agent     string   `json:"agent"`
	AX        string   `json:"ax"`
	Listeners []string `json:"listeners"`
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
	LastTick     int    `json:"a_last_tick"`
	Tags         string `json:"a_tags"`
	Mark         string `json:"a_mark"`
	Color        string `json:"a_color"`
}

type SyncPackerAgentUpdate struct {
	SpType int `json:"type"`

	Id           string `json:"a_id"`
	Sleep        uint   `json:"a_sleep"`
	Jitter       uint   `json:"a_jitter"`
	WorkingTime  int    `json:"a_workingtime"`
	KillDate     int    `json:"a_killdate"`
	Impersonated string `json:"a_impersonated"`
	Tags         string `json:"a_tags"`
	Mark         string `json:"a_mark"`
	Color        string `json:"a_color"`
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

	FileId string `json:"d_file_id"`
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
