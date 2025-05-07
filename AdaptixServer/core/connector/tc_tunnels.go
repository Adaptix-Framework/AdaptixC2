package connector

import (
	"errors"
	"github.com/gin-gonic/gin"
	"net/http"
)

type TunnelStartSocks5Action struct {
	AgentId     string `json:"agent_id"`
	Description string `json:"desc"`
	Lhost       string `json:"l_host"`
	Lport       int    `json:"l_port"`
	UseAuth     bool   `json:"use_auth"`
	Username    string `json:"username"`
	Password    string `json:"password"`
}

func (tc *TsConnector) TcTunnelStartSocks5(ctx *gin.Context) {
	var (
		ta  TunnelStartSocks5Action
		err error
	)

	err = ctx.ShouldBindJSON(&ta)
	if err != nil {
		_ = ctx.Error(errors.New("invalid tunnel"))
		return
	}

	err = tc.teamserver.TsTunnelTaskStartSocks5(ta.AgentId, ta.Description, ta.Lhost, ta.Lport, ta.UseAuth, ta.Username, ta.Password)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "Tunnel stopped", "ok": true})
}

type TunnelStartSocks4Action struct {
	AgentId     string `json:"agent_id"`
	Description string `json:"desc"`
	Lhost       string `json:"l_host"`
	Lport       int    `json:"l_port"`
}

func (tc *TsConnector) TcTunnelStartSocks4(ctx *gin.Context) {
	var (
		ta  TunnelStartSocks4Action
		err error
	)

	err = ctx.ShouldBindJSON(&ta)
	if err != nil {
		_ = ctx.Error(errors.New("invalid tunnel"))
		return
	}

	err = tc.teamserver.TsTunnelTaskStartSocks4(ta.AgentId, ta.Description, ta.Lhost, ta.Lport)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "Tunnel stopped", "ok": true})
}

type TunnelStartLpfAction struct {
	AgentId     string `json:"agent_id"`
	Description string `json:"desc"`
	Lhost       string `json:"l_host"`
	Lport       int    `json:"l_port"`
	Thost       string `json:"t_host"`
	Tport       int    `json:"t_port"`
}

func (tc *TsConnector) TcTunnelStartLpf(ctx *gin.Context) {
	var (
		ta  TunnelStartLpfAction
		err error
	)

	err = ctx.ShouldBindJSON(&ta)
	if err != nil {
		_ = ctx.Error(errors.New("invalid tunnel"))
		return
	}

	err = tc.teamserver.TsTunnelTaskStartLpf(ta.AgentId, ta.Description, ta.Lhost, ta.Lport, ta.Thost, ta.Tport)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "Tunnel stopped", "ok": true})
}

type TunnelStartRpfAction struct {
	AgentId     string `json:"agent_id"`
	Description string `json:"desc"`
	Port        int    `json:"port"`
	Thost       string `json:"t_host"`
	Tport       int    `json:"t_port"`
}

func (tc *TsConnector) TcTunnelStartRpf(ctx *gin.Context) {
	var (
		ta  TunnelStartRpfAction
		err error
	)

	err = ctx.ShouldBindJSON(&ta)
	if err != nil {
		_ = ctx.Error(errors.New("invalid tunnel"))
		return
	}

	err = tc.teamserver.TsTunnelTaskStartRpf(ta.AgentId, ta.Description, ta.Port, ta.Thost, ta.Tport)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "Tunnel stopped", "ok": true})
}

type TunnelStopAction struct {
	TunnelId string `json:"p_tunnel_id"`
}

func (tc *TsConnector) TcTunnelStop(ctx *gin.Context) {
	var (
		tunnelAction TunnelStopAction
		err          error
	)

	err = ctx.ShouldBindJSON(&tunnelAction)
	if err != nil {
		_ = ctx.Error(errors.New("invalid tunnel"))
		return
	}

	err = tc.teamserver.TsTunnelTaskStop(tunnelAction.TunnelId)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "Tunnel stopped", "ok": true})
}

type TunnelSetItemAction struct {
	TunnelId string `json:"p_tunnel_id"`
	Info     string `json:"p_info"`
}

func (tc *TsConnector) TcTunnelSetIno(ctx *gin.Context) {
	var (
		tunnelAction TunnelSetItemAction
		err          error
	)

	err = ctx.ShouldBindJSON(&tunnelAction)
	if err != nil {
		_ = ctx.Error(errors.New("invalid tunnel"))
		return
	}

	err = tc.teamserver.TsTunnelSetInfo(tunnelAction.TunnelId, tunnelAction.Info)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "Tunnel stopped", "ok": true})
}
