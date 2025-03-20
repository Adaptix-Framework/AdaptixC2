package adaptix

import (
	"os"
)

type ListenerData struct {
	Name      string `json:"l_name"`
	Type      string `json:"l_type"`
	BindHost  string `json:"l_bind_host"`
	BindPort  string `json:"l_bind_port"`
	AgentHost string `json:"l_agent_host"`
	AgentPort string `json:"l_agent_port"`
	Status    string `json:"l_status"`
	Data      string `json:"l_data"`
	Watermark string `json:"l_watermark"`
}

type AgentData struct {
	Crc          string `json:"a_crc"`
	Id           string `json:"a_id"`
	Name         string `json:"a_name"`
	SessionKey   []byte `json:"a_session_key"`
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
	OemCP        int    `json:"a_oemcp"`
	ACP          int    `json:"a_acp"`
	CreateTime   int64  `json:"a_create_time"`
	LastTick     int    `json:"a_last_tick"`
	Tags         string `json:"a_tags"`
	Mark         string `json:"a_mark"`
	Color        string `json:"a_color"`
}

type TaskData struct {
	Type        int    `json:"t_type"`
	TaskId      string `json:"t_task_id"`
	AgentId     string `json:"t_agent_id"`
	Client      string `json:"t_client"`
	User        string `json:"t_user"`
	Computer    string `json:"t_computer"`
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

type ConsoleMessageData struct {
	Message string `json:"m_message"`
	Status  int    `json:"m_status"`
	Text    string `json:"m_text"`
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

type DownloadData struct {
	FileId     string `json:"d_file_id"`
	AgentId    string `json:"d_agent_id"`
	AgentName  string `json:"d_agent_name"`
	User       string `json:"d_user"`
	Computer   string `json:"d_computer"`
	RemotePath string `json:"d_remote_path"`
	LocalPath  string `json:"d_local_path"`
	TotalSize  int    `json:"d_total_size"`
	RecvSize   int    `json:"d_recv_size"`
	Date       int64  `json:"d_date"`
	State      int    `json:"d_state"`
	File       *os.File
}

type TunnelData struct {
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

type PivotData struct {
	PivotId       string `json:"p_pivot_id"`
	PivotName     string `json:"p_pivot_name"`
	ParentAgentId string `json:"p_parent_agent_id"`
	ChildAgentId  string `json:"p_child_agent_id"`
}
