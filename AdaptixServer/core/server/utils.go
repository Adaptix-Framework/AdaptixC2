package server

import (
	"AdaptixServer/core/connector"
	"AdaptixServer/core/database"
	"AdaptixServer/core/extender"
	"AdaptixServer/core/profile"
	"AdaptixServer/core/utils/safe"
	"os"
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
	TYPE_TASK    = 1
	TYPE_BROWSER = 2
	TYPE_JOB     = 3
)

// TeamServer

type Teamserver struct {
	Profile       *profile.AdaptixProfile
	DBMS          *database.DBMS
	AdaptixServer *connector.TsConnector
	Extender      *extender.AdaptixExtender

	listener_configs safe.Map // listenerFullName string : listenerInfo extender.ListenerInfo
	agent_configs    safe.Map // agentName string        : agentInfo extender.AgentInfo

	clients     safe.Map // username string,    : socket *websocket.Conn
	syncpackets safe.Map // store string        : sync_packet interface{}
	listeners   safe.Map // listenerName string : listenerData ListenerData
	agents      safe.Map // agentId string      : agent *Agent
	downloads   safe.Map // dileId string       : downloadData DownloadData

	agent_types safe.Map // agentMark string : agentName string
}

type Agent struct {
	Data        AgentData
	TasksQueue  *safe.Slice // taskData TaskData
	Tasks       safe.Map    // taskId string, taskData TaskData
	ClosedTasks safe.Map    // taskId string, taskData TaskData
}

// Data

type ListenerData struct {
	Name      string `json:"l_name"`
	Type      string `json:"l_type"`
	BindHost  string `json:"l_bind_host"`
	BindPort  string `json:"l_bind_port"`
	AgentHost string `json:"l_agent_host"`
	AgentPort string `json:"l_agent_port"`
	Status    string `json:"l_status"`
	Data      string `json:"l_data"`
}

type AgentData struct {
	Crc        string `json:"a_crc"`
	Id         string `json:"a_id"`
	Name       string `json:"a_name"`
	SessionKey []byte `json:"a_session_key"`
	Listener   string `json:"a_listener"`
	Async      bool   `json:"a_async"`
	ExternalIP string `json:"a_external_ip"`
	InternalIP string `json:"a_internal_ip"`
	GmtOffset  int    `json:"a_gmt_offset"`
	Sleep      uint   `json:"a_sleep"`
	Jitter     uint   `json:"a_jitter"`
	Pid        string `json:"a_pid"`
	Tid        string `json:"a_tid"`
	Arch       string `json:"a_arch"`
	Elevated   bool   `json:"a_elevated"`
	Process    string `json:"a_process"`
	Os         int    `json:"a_os"`
	OsDesc     string `json:"a_os_desc"`
	Domain     string `json:"a_domain"`
	Computer   string `json:"a_computer"`
	Username   string `json:"a_username"`
	OemCP      int    `json:"a_oemcp"`
	ACP        int    `json:"a_acp"`
	CreateTime int64  `json:"a_create_time"`
	LastTick   int    `json:"a_last_tick"`
	Tags       string `json:"a_tags"`
}

type TaskData struct {
	Type        int    `json:"t_type"`
	TaskId      string `json:"t_task_id"`
	AgentId     string `json:"t_agent_id"`
	User        string `json:"t_user"`
	StartDate   int64  `json:"t_start_date"`
	FinishDate  int64  `json:"t_finish_date"`
	Data        []byte `json:"t_data"`
	CommandLine string `json:"t_command_line"`
	MessageType int    `json:"t_message_type"`
	Message     string `json:"t_message"`
	ClearText   string `json:"t_clear_text"`
	Completed   bool   `json:"t_completed"`
	Sync        bool   `json:"t_sync"`
}

type DownloadData struct {
	FileId     string `json:"d_file_id"`
	AgentId    string `json:"d_agent_id"`
	AgentName  string `json:"d_agent_name"`
	Computer   string `json:"d_computer"`
	RemotePath string `json:"d_remote_path"`
	LocalPath  string `json:"d_local_path"`
	TotalSize  int    `json:"d_total_size"`
	RecvSize   int    `json:"d_recv_size"`
	Date       int64  `json:"d_date"`
	State      int    `json:"d_state"`
	File       *os.File
}

type ListingFileData struct {
	IsDir    bool   `json:"b_is_dir"`
	Size     int64  `json:"b_size"`
	Date     int64  `json:"b_date"`
	Filename string `json:"b_filename"`
}

type ListingProcessData struct {
	Pid         uint   `json:"b_pid"`
	Ppid        uint   `json:"b_ppid"`
	SessionId   uint   `json:"b_session_id"`
	Arch        string `json:"b_arch"`
	Context     string `json:"b_context"`
	ProcessName string `json:"b_process_name"`
}

type ListingDrivesData struct {
	Name string `json:"b_name"`
	Type string `json:"b_type"`
}

// SyncPacket

type SyncPackerStart struct {
	SpType int `json:"type"`

	Count int `json:"count"`
}

type SyncPackerFinish struct {
	SpType int `json:"type"`
}

type SyncPackerClientConnect struct {
	store        string
	SpCreateTime int64 `json:"time"`
	SpType       int   `json:"type"`

	Username string `json:"username"`
}

type SyncPackerClientDisconnect struct {
	store        string
	SpCreateTime int64 `json:"time"`
	SpType       int   `json:"type"`

	Username string `json:"username"`
}

/// LISTENER

type SyncPackerListenerReg struct {
	store  string
	SpType int `json:"type"`

	ListenerFN string `json:"fn"`
	ListenerUI string `json:"ui"`
}

type SyncPackerListenerStart struct {
	store        string
	SpCreateTime int64 `json:"time"`
	SpType       int   `json:"type"`

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
	store        string
	SpCreateTime int64 `json:"time"`
	SpType       int   `json:"type"`

	ListenerName string `json:"l_name"`
}

/// AGENT

type SyncPackerAgentReg struct {
	store  string
	SpType int `json:"type"`

	Agent    string `json:"agent"`
	Listener string `json:"listener"`
	AgentUI  string `json:"ui"`
	AgentCmd string `json:"cmd"`
}

type SyncPackerAgentNew struct {
	store        string
	SpCreateTime int64 `json:"time"`
	SpType       int   `json:"type"`

	Id         string `json:"a_id"`
	Name       string `json:"a_name"`
	Listener   string `json:"a_listener"`
	Async      bool   `json:"a_async"`
	ExternalIP string `json:"a_external_ip"`
	InternalIP string `json:"a_internal_ip"`
	GmtOffset  int    `json:"a_gmt_offset"`
	Sleep      uint   `json:"a_sleep"`
	Jitter     uint   `json:"a_jitter"`
	Pid        string `json:"a_pid"`
	Tid        string `json:"a_tid"`
	Arch       string `json:"a_arch"`
	Elevated   bool   `json:"a_elevated"`
	Process    string `json:"a_process"`
	Os         int    `json:"a_os"`
	OsDesc     string `json:"a_os_desc"`
	Domain     string `json:"a_domain"`
	Computer   string `json:"a_computer"`
	Username   string `json:"a_username"`
	LastTick   int    `json:"a_last_tick"`
	Tags       string `json:"a_tags"`
}

type SyncPackerAgentUpdate struct {
	store  string
	SpType int `json:"type"`

	Id       string `json:"a_id"`
	Sleep    uint   `json:"a_sleep"`
	Jitter   uint   `json:"a_jitter"`
	Elevated bool   `json:"a_elevated"`
	Username string `json:"a_username"`
	Tags     string `json:"a_tags"`
}

type SyncPackerAgentTick struct {
	store  string
	SpType int `json:"type"`

	Id string `json:"a_id"`
}

type SyncPackerAgentConsoleOutput struct {
	store        string
	SpCreateTime int64 `json:"time"`
	SpType       int   `json:"type"`

	AgentId     string `json:"a_id"`
	MessageType int    `json:"a_msg_type"`
	Message     string `json:"a_message"`
	ClearText   string `json:"a_text"`
}

type SyncPackerAgentTaskCreate struct {
	store        string
	SpCreateTime int64 `json:"time"`
	SpType       int   `json:"type"`

	AgentId   string `json:"a_id"`
	TaskId    string `json:"a_task_id"`
	TaskType  int    `json:"a_task_type"`
	StartTime int64  `json:"a_start_time"`
	CmdLine   string `json:"a_cmdline"`
	User      string `json:"a_user"`
}

type SyncPackerAgentTaskUpdate struct {
	store        string
	SpCreateTime int64 `json:"time"`
	SpType       int   `json:"type"`

	AgentId     string `json:"a_id"`
	TaskId      string `json:"a_task_id"`
	TaskType    int    `json:"a_task_type"`
	FinishTime  int64  `json:"a_finish_time"`
	MessageType int    `json:"a_msg_type"`
	Message     string `json:"a_message"`
	Text        string `json:"a_text"`
	Completed   bool   `json:"a_completed"`
}

type SyncPackerAgentRemove struct {
	store  string
	SpType int `json:"type"`

	AgentId string `json:"a_id"`
}

/// DOWNLOAD

type SyncPackerDownloadCreate struct {
	store  string
	SpType int `json:"type"`

	FileId    string `json:"d_file_id"`
	AgentId   string `json:"d_agent_id"`
	AgentName string `json:"d_agent_name"`
	Computer  string `json:"d_computer"`
	File      string `json:"d_file"`
	Size      int    `json:"d_size"`
	Date      int64  `json:"d_date"`
}

type SyncPackerDownloadUpdate struct {
	store  string
	SpType int `json:"type"`

	FileId   string `json:"d_file_id"`
	RecvSize int    `json:"d_recv_size"`
	State    int    `json:"d_state"`
}

type SyncPackerDownloadDelete struct {
	store  string
	SpType int `json:"type"`

	FileId string `json:"d_file_id"`
}

/// BROWSER

type SyncPacketBrowserDisks struct {
	store  string
	SpType int `json:"type"`

	AgentId     string `json:"b_agent_id"`
	Time        int64  `json:"b_time"`
	MessageType int    `json:"b_msg_type"`
	Message     string `json:"b_message"`
	Data        string `json:"b_data"`
}
