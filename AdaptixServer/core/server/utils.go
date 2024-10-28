package server

import (
	"AdaptixServer/core/database"
	"AdaptixServer/core/extender"
	"AdaptixServer/core/httphandler"
	"AdaptixServer/core/profile"
	"AdaptixServer/core/utils/safe"
)

// TeamServer

type Teamserver struct {
	Profile       *profile.AdaptixProfile
	DBMS          *database.DBMS
	AdaptixServer *httphandler.TsHttpHandler
	Extender      *extender.AdaptixExtender

	listener_configs safe.Map
	agent_configs    safe.Map

	clients     safe.Map
	syncpackets safe.Map
	listeners   safe.Map

	agent_types safe.Map
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
	Type       string   `json:"a_type"`
	Id         string   `json:"a_id"`
	Name       string   `json:"a_name"`
	SessionKey []byte   `json:"a_session_key"`
	Listener   string   `json:"a_listener"`
	Async      bool     `json:"a_async"`
	ExternalIP string   `json:"a_external_ip"`
	InternalIP string   `json:"a_internal_ip"`
	GmtOffset  int      `json:"a_gmt_offset"`
	Sleep      string   `json:"a_sleep"`
	Jitter     string   `json:"a_jitter"`
	Pid        string   `json:"a_pid"`
	Tid        string   `json:"a_tid"`
	Arch       string   `json:"a_arch"`
	Elevated   bool     `json:"a_elevated"`
	Process    string   `json:"a_process"`
	Os         int      `json:"a_os"`
	OsDesc     string   `json:"a_os_desc"`
	Domain     string   `json:"a_domain"`
	Computer   string   `json:"a_computer"`
	Username   string   `json:"a_username"`
	OemCP      int      `json:"a_oemcp"`
	ACP        int      `json:"a_acp"`
	CreateTime int64    `json:"a_create_time"`
	Tags       []string `json:"a_tags"`
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

type SyncPackerListenerNew struct {
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

type SyncPackerAgentNew struct {
	store  string
	SpType int `json:"type"`

	Agent    string `json:"agent"`
	Listener string `json:"listener"`
	AgentUI  string `json:"ui"`
}
