package adaptix

import "os"

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

type ConsoleMessageData struct {
	Message string `json:"m_message"`
	Status  int    `json:"m_status"`
	Text    string `json:"m_text"`
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
