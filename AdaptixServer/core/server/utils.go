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

	clients          safe.Map
	syncpackets      safe.Map
	listener_configs safe.Map
	listeners        safe.Map
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
