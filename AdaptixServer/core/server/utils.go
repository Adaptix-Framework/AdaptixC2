package server

import (
	"AdaptixServer/core/httphandler"
	"AdaptixServer/core/profile"
	"AdaptixServer/core/utils/safe"
)

// TeamServer

type Teamserver struct {
	Profile       *profile.AdaptixProfile
	AdaptixServer *httphandler.TsHttpHandler

	clients     safe.Map
	syncpackets safe.Map
}

// SyncPacket

type SyncPackerStart struct {
	SpType    string `json:"type"`
	SpSubType string `json:"subtype"`

	Count int `json:"count"`
}

type SyncPackerFinish struct {
	SpType    string `json:"type"`
	SpSubType string `json:"subtype"`
}

type SyncPackerClientConnect struct {
	store        string
	SpCreateTime int64  `json:"time"`
	SpType       string `json:"type"`
	SpSubType    string `json:"subtype"`

	Username string `json:"username"`
}

type SyncPackerClientDisconnect struct {
	store        string
	SpCreateTime int64  `json:"time"`
	SpType       string `json:"type"`
	SpSubType    string `json:"subtype"`

	Username string `json:"username"`
}
