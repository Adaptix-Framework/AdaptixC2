package server

import (
	"AdaptixServer/core/extender"
	"AdaptixServer/core/httphandler"
	"AdaptixServer/core/profile"
	"AdaptixServer/core/utils/safe"
)

// TeamServer

type Teamserver struct {
	Profile       *profile.AdaptixProfile
	AdaptixServer *httphandler.TsHttpHandler
	Extender      *extender.AdaptixExtender

	clients      safe.Map
	syncpackets  safe.Map
	ex_listeners safe.Map
}

// SyncPacket

type SyncPackerStart struct {
	SpType    int `json:"type"`
	SpSubType int `json:"subtype"`

	Count int `json:"count"`
}

type SyncPackerFinish struct {
	SpType    int `json:"type"`
	SpSubType int `json:"subtype"`
}

type SyncPackerClientConnect struct {
	store        string
	SpCreateTime int64 `json:"time"`
	SpType       int   `json:"type"`
	SpSubType    int   `json:"subtype"`

	Username string `json:"username"`
}

type SyncPackerClientDisconnect struct {
	store        string
	SpCreateTime int64 `json:"time"`
	SpType       int   `json:"type"`
	SpSubType    int   `json:"subtype"`

	Username string `json:"username"`
}

type SyncPackerListenerNew struct {
	store     string
	SpType    int `json:"type"`
	SpSubType int `json:"subtype"`

	ListenerFN string `json:"fn"`
	ListenerUI string `json:"ui"`
}
