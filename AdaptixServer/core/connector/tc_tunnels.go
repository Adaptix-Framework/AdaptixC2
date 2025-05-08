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

	value, exists := ctx.Get("username")
	if !exists {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: username not found in context", "ok": false})
		return
	}
	clientName, ok := value.(string)
	if !ok {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: invalid username type in context", "ok": false})
		return
	}

	if ta.Lhost == "" {
		_ = ctx.Error(errors.New("l_host is required"))
		return
	}
	if ta.Lport < 1 || ta.Lport > 65535 {
		_ = ctx.Error(errors.New("l_port must be from 1 to 65535"))
		return
	}
	if ta.UseAuth {
		if ta.Username == "" {
			_ = ctx.Error(errors.New("username is required"))
			return
		}
		if ta.Password == "" {
			_ = ctx.Error(errors.New("password is required"))
			return
		}
	}

	err = tc.teamserver.TsTunnelTaskStartSocks5(ta.AgentId, clientName, ta.Description, ta.Lhost, ta.Lport, ta.UseAuth, ta.Username, ta.Password)
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

	value, exists := ctx.Get("username")
	if !exists {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: username not found in context", "ok": false})
		return
	}
	clientName, ok := value.(string)
	if !ok {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: invalid username type in context", "ok": false})
		return
	}

	if ta.Lhost == "" {
		_ = ctx.Error(errors.New("l_host is required"))
		return
	}
	if ta.Lport < 1 || ta.Lport > 65535 {
		_ = ctx.Error(errors.New("l_port must be from 1 to 65535"))
		return
	}

	err = tc.teamserver.TsTunnelTaskStartSocks4(ta.AgentId, clientName, ta.Description, ta.Lhost, ta.Lport)
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

	value, exists := ctx.Get("username")
	if !exists {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: username not found in context", "ok": false})
		return
	}
	clientName, ok := value.(string)
	if !ok {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: invalid username type in context", "ok": false})
		return
	}

	if ta.Lhost == "" {
		_ = ctx.Error(errors.New("l_host is required"))
		return
	}
	if ta.Lport < 1 || ta.Lport > 65535 {
		_ = ctx.Error(errors.New("l_port must be from 1 to 65535"))
		return
	}
	if ta.Thost == "" {
		_ = ctx.Error(errors.New("t_host is required"))
		return
	}
	if ta.Tport < 1 || ta.Tport > 65535 {
		_ = ctx.Error(errors.New("t_port must be from 1 to 65535"))
		return
	}

	err = tc.teamserver.TsTunnelTaskStartLpf(ta.AgentId, clientName, ta.Description, ta.Lhost, ta.Lport, ta.Thost, ta.Tport)
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

	value, exists := ctx.Get("username")
	if !exists {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: username not found in context", "ok": false})
		return
	}
	clientName, ok := value.(string)
	if !ok {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: invalid username type in context", "ok": false})
		return
	}

	if ta.Port < 1 || ta.Port > 65535 {
		_ = ctx.Error(errors.New("port must be from 1 to 65535"))
		return
	}
	if ta.Thost == "" {
		_ = ctx.Error(errors.New("t_host is required"))
		return
	}
	if ta.Tport < 1 || ta.Tport > 65535 {
		_ = ctx.Error(errors.New("t_port must be from 1 to 65535"))
		return
	}

	err = tc.teamserver.TsTunnelTaskStartRpf(ta.AgentId, clientName, ta.Description, ta.Port, ta.Thost, ta.Tport)
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
