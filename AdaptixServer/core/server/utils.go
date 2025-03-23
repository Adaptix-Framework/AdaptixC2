package server

import (
	"AdaptixServer/core/adaptix"
	"AdaptixServer/core/connector"
	"AdaptixServer/core/database"
	"AdaptixServer/core/extender"
	"AdaptixServer/core/profile"
	"AdaptixServer/core/utils/safe"
	"context"
	"github.com/gorilla/websocket"
	"net"
	"sync"
)

const (
	CONSOLE_OUT_LOCAL         = 1
	CONSOLE_OUT_LOCAL_INFO    = 2
	CONSOLE_OUT_LOCAL_ERROR   = 3
	CONSOLE_OUT_LOCAL_SUCCESS = 4
	CONSOLE_OUT_INFO          = 5
	CONSOLE_OUT_ERROR         = 6
	CONSOLE_OUT_SUCCESS       = 7
)

const (
	TYPE_TASK       = 1
	TYPE_BROWSER    = 2
	TYPE_JOB        = 3
	TYPE_TUNNEL     = 4
	TYPE_PROXY_DATA = 5
)

// TeamServer

type Client struct {
	username   string
	synced     bool
	lockSocket *sync.Mutex
	socket     *websocket.Conn
	tmp_store  *safe.Slice
}

type Teamserver struct {
	Profile       *profile.AdaptixProfile
	DBMS          *database.DBMS
	AdaptixServer *connector.TsConnector
	Extender      *extender.AdaptixExtender

	listener_configs safe.Map // listenerFullName string : listenerInfo extender.ListenerInfo
	agent_configs    safe.Map // agentName string        : agentInfo extender.AgentInfo

	wm_agent_types map[string]string   // agentMark string : agentName string
	wm_listeners   map[string][]string // watermark string : ListenerName string, ListenerType string

	events    *safe.Slice // 			         : sync_packet interface{}
	clients   safe.Map    // username string     : socket *websocket.Conn
	agents    safe.Map    // agentId string      : agent *Agent
	listeners safe.Map    // listenerName string : listenerData ListenerData
	downloads safe.Map    // dileId string       : downloadData DownloadData
	tunnels   safe.Map    // tunnelId string     : tunnel Tunnel
	pivots    *safe.Slice // 			         : PivotData
}

type Agent struct {
	Data   adaptix.AgentData
	Tick   bool
	Active bool

	OutConsole *safe.Slice //  sync_packet interface{}

	TunnelQueue *safe.Slice // taskData TaskData
	TasksQueue  *safe.Slice // taskData TaskData

	RunningTasks   safe.Map // taskId string, taskData TaskData
	CompletedTasks safe.Map // taskId string, taskData TaskData

	PivotParent *adaptix.PivotData
	PivotChilds *safe.Slice
}

type TunnelConnection struct {
	channelId    int
	protocol     string
	conn         net.Conn
	ctx          context.Context
	handleCancel context.CancelFunc
}

type Tunnel struct {
	TaskId string
	Data   adaptix.TunnelData

	listener    net.Listener
	connections safe.Map

	handlerConnectTCP func(channelId int, addr string, port int) []byte
	handlerConnectUDP func(channelId int, addr string, port int) []byte
	handlerWriteTCP   func(channelId int, data []byte) []byte
	handlerWriteUDP   func(channelId int, data []byte) []byte
	handlerClose      func(channelId int) []byte
}

////////////////////////////////////////////////////////////////////////////////////////////

// SyncPacket

type SyncPackerStart struct {
	SpType int `json:"type"`

	Count int `json:"count"`
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

	ListenerFN string `json:"fn"`
	ListenerUI string `json:"ui"`
}

type SyncPackerListenerStart struct {
	SpType int `json:"type"`

	ListenerName   string `json:"l_name"`
	ListenerType   string `json:"l_type"`
	BindHost       string `json:"l_bind_host"`
	BindPort       string `json:"l_bind_port"`
	AgentHost      string `json:"l_agent_host"`
	AgentPort      string `json:"l_agent_port"`
	ListenerStatus string `json:"l_status"`
	Data           string `json:"l_data"`
}

type SyncPackerListenerStop struct {
	SpType int `json:"type"`

	ListenerName string `json:"l_name"`
}

/// AGENT

type SyncPackerAgentReg struct {
	SpType int `json:"type"`

	Agent     string   `json:"agent"`
	Listeners []string `json:"listener"`
	AgentUI   string   `json:"ui"`
	AgentCmd  string   `json:"cmd"`
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
