package connector

import (
	"errors"
	"net/http"

	"github.com/gin-gonic/gin"
)

func (tc *TsConnector) TcTunnelList(ctx *gin.Context) {
	jsonTunnels, err := tc.teamserver.TsTunnelList()
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	ctx.Data(http.StatusOK, "application/json; charset=utf-8", []byte(jsonTunnels))
}

type TunnelStartSocks5Action struct {
	AgentId     int64  `json:"agent_id"`
	Listen      bool   `json:"listen"`
	Description string `json:"desc"`
	Lhost       string `json:"l_host"`
	Lport       int    `json:"l_port"`
	UseAuth     bool   `json:"use_auth"`
	Username    string `json:"username"`
	Password    string `json:"password"`
}

func (tc *TsConnector) TcTunnelStartSocks5(ctx *gin.Context) {
	var (
		ta         TunnelStartSocks5Action
		ok         bool
		clientName string
		tunnelId   int64
	)

	err := ctx.ShouldBindJSON(&ta)
	if err != nil {
		err = errors.New("invalid JSON data")
		goto ERR
	}

	clientName, ok = mustUsername(ctx)
	if !ok {
		return
	}

	if ta.Lhost == "" {
		err = errors.New("l_host is required")
		goto ERR
	}
	if ta.Lport < 1 || ta.Lport > 65535 {
		err = errors.New("l_port must be from 1 to 65535")
		goto ERR
	}
	if ta.UseAuth {
		if ta.Username == "" {
			err = errors.New("username is required")
			goto ERR
		}
		if ta.Password == "" {
			err = errors.New("password is required")
			goto ERR
		}
	}

	if ta.UseAuth {
		tunnelId, err = tc.teamserver.TsTunnelClientStart(ta.AgentId, ta.Listen, 3, ta.Description, ta.Lhost, ta.Lport, clientName, "", 0, ta.Username, ta.Password)
	} else {
		tunnelId, err = tc.teamserver.TsTunnelClientStart(ta.AgentId, ta.Listen, 2, ta.Description, ta.Lhost, ta.Lport, clientName, "", 0, ta.Username, ta.Password)
	}
	if err != nil {
		goto ERR
	}

	respondOKMessage(ctx, tunnelId)
	return

ERR:
	respondError(ctx, http.StatusOK, err.Error())
}

type TunnelStartSocks4Action struct {
	AgentId     int64  `json:"agent_id"`
	Listen      bool   `json:"listen"`
	Description string `json:"desc"`
	Lhost       string `json:"l_host"`
	Lport       int    `json:"l_port"`
}

func (tc *TsConnector) TcTunnelStartSocks4(ctx *gin.Context) {
	var (
		ta         TunnelStartSocks4Action
		ok         bool
		clientName string
		tunnelId   int64
	)

	err := ctx.ShouldBindJSON(&ta)
	if err != nil {
		err = errors.New("invalid JSON data")
		goto ERR
	}

	clientName, ok = mustUsername(ctx)
	if !ok {
		return
	}

	if ta.Lhost == "" {
		err = errors.New("l_host is required")
		goto ERR
	}
	if ta.Lport < 1 || ta.Lport > 65535 {
		err = errors.New("l_port must be from 1 to 65535")
		goto ERR
	}

	tunnelId, err = tc.teamserver.TsTunnelClientStart(ta.AgentId, ta.Listen, 1, ta.Description, ta.Lhost, ta.Lport, clientName, "", 0, "", "")
	if err != nil {
		goto ERR
	}

	respondOKMessage(ctx, tunnelId)
	return

ERR:
	respondError(ctx, http.StatusOK, err.Error())
}

type TunnelStartLpfAction struct {
	AgentId     int64  `json:"agent_id"`
	Listen      bool   `json:"listen"`
	Description string `json:"desc"`
	Lhost       string `json:"l_host"`
	Lport       int    `json:"l_port"`
	Thost       string `json:"t_host"`
	Tport       int    `json:"t_port"`
}

func (tc *TsConnector) TcTunnelStartLpf(ctx *gin.Context) {
	var (
		ta         TunnelStartLpfAction
		ok         bool
		clientName string
		tunnelId   int64
	)

	err := ctx.ShouldBindJSON(&ta)
	if err != nil {
		err = errors.New("invalid JSON data")
		goto ERR
	}

	clientName, ok = mustUsername(ctx)
	if !ok {
		return
	}

	if ta.Lhost == "" {
		err = errors.New("l_host is required")
		goto ERR
	}
	if ta.Lport < 1 || ta.Lport > 65535 {
		err = errors.New("l_port must be from 1 to 65535")
		goto ERR
	}
	if ta.Thost == "" {
		err = errors.New("t_host is required")
		goto ERR
	}
	if ta.Tport < 1 || ta.Tport > 65535 {
		err = errors.New("t_port must be from 1 to 65535")
		goto ERR
	}

	tunnelId, err = tc.teamserver.TsTunnelClientStart(ta.AgentId, ta.Listen, 4, ta.Description, ta.Lhost, ta.Lport, clientName, ta.Thost, ta.Tport, "", "")
	if err != nil {
		goto ERR
	}

	respondOKMessage(ctx, tunnelId)
	return

ERR:
	respondError(ctx, http.StatusOK, err.Error())
}

type TunnelStartRpfAction struct {
	AgentId     int64  `json:"agent_id"`
	Listen      bool   `json:"listen"`
	Description string `json:"desc"`
	Port        int    `json:"port"`
	Thost       string `json:"t_host"`
	Tport       int    `json:"t_port"`
}

func (tc *TsConnector) TcTunnelStartRpf(ctx *gin.Context) {
	var (
		ta         TunnelStartRpfAction
		ok         bool
		clientName string
		tunnelId   int64
	)

	err := ctx.ShouldBindJSON(&ta)
	if err != nil {
		err = errors.New("invalid JSON data")
		goto ERR
	}

	clientName, ok = mustUsername(ctx)
	if !ok {
		return
	}

	if ta.Port < 1 || ta.Port > 65535 {
		err = errors.New("l_port must be from 1 to 65535")
		goto ERR
	}
	if ta.Thost == "" {
		err = errors.New("t_host is required")
		goto ERR
	}
	if ta.Tport < 1 || ta.Tport > 65535 {
		err = errors.New("t_port must be from 1 to 65535")
		goto ERR
	}

	tunnelId, err = tc.teamserver.TsTunnelClientStart(ta.AgentId, ta.Listen, 5, ta.Description, "", ta.Port, clientName, ta.Thost, ta.Tport, "", "")
	if err != nil {
		goto ERR
	}

	respondOKMessage(ctx, tunnelId)
	return

ERR:
	respondError(ctx, http.StatusOK, err.Error())
}

type TunnelStopAction struct {
	TunnelId int64 `json:"p_tunnel_id"`
}

func (tc *TsConnector) TcTunnelStop(ctx *gin.Context) {
	var tunnelAction TunnelStopAction
	err := ctx.ShouldBindJSON(&tunnelAction)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	clientName, ok := mustUsername(ctx)
	if !ok {
		return
	}

	err = tc.teamserver.TsTunnelClientStop(tunnelAction.TunnelId, clientName)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOKMessage(ctx, "Tunnel stopped")
}

type TunnelSetItemAction struct {
	TunnelId int64  `json:"p_tunnel_id"`
	Info     string `json:"p_info"`
}

func (tc *TsConnector) TcTunnelSetIno(ctx *gin.Context) {
	var tunnelAction TunnelSetItemAction
	err := ctx.ShouldBindJSON(&tunnelAction)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	err = tc.teamserver.TsTunnelClientSetInfo(tunnelAction.TunnelId, tunnelAction.Info)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOKMessage(ctx, "Tunnel info updated")
}
