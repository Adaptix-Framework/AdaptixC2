package connector

import (
	"errors"
	"net/http"

	"github.com/gin-gonic/gin"
)

type TunnelStartSocks5Action struct {
	AgentId     string `json:"agent_id"`
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
		value      any
		exists     bool
		ok         bool
		clientName string
		tunnelId   string
	)

	err := ctx.ShouldBindJSON(&ta)
	if err != nil {
		err = errors.New("invalid JSON data")
		goto ERR
	}

	value, exists = ctx.Get("username")
	if !exists {
		err = errors.New("Server error: username not found in context")
		goto ERR
	}
	clientName, ok = value.(string)
	if !ok {
		err = errors.New("Server error: invalid username type in context")
		goto ERR
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

	ctx.JSON(http.StatusOK, gin.H{"message": tunnelId, "ok": true})
	return

ERR:
	ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
}

type TunnelStartSocks4Action struct {
	AgentId     string `json:"agent_id"`
	Listen      bool   `json:"listen"`
	Description string `json:"desc"`
	Lhost       string `json:"l_host"`
	Lport       int    `json:"l_port"`
}

func (tc *TsConnector) TcTunnelStartSocks4(ctx *gin.Context) {
	var (
		ta         TunnelStartSocks4Action
		value      any
		exists     bool
		ok         bool
		clientName string
		tunnelId   string
	)

	err := ctx.ShouldBindJSON(&ta)
	if err != nil {
		err = errors.New("invalid JSON data")
		goto ERR
	}

	value, exists = ctx.Get("username")
	if !exists {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: username not found in context", "ok": false})
		return
	}
	clientName, ok = value.(string)
	if !ok {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: invalid username type in context", "ok": false})
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

	ctx.JSON(http.StatusOK, gin.H{"message": tunnelId, "ok": true})
	return

ERR:
	ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
}

type TunnelStartLpfAction struct {
	AgentId     string `json:"agent_id"`
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
		value      any
		exists     bool
		ok         bool
		clientName string
		tunnelId   string
	)

	err := ctx.ShouldBindJSON(&ta)
	if err != nil {
		err = errors.New("invalid JSON data")
		goto ERR
	}

	value, exists = ctx.Get("username")
	if !exists {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: username not found in context", "ok": false})
		return
	}
	clientName, ok = value.(string)
	if !ok {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: invalid username type in context", "ok": false})
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

	ctx.JSON(http.StatusOK, gin.H{"message": tunnelId, "ok": true})
	return

ERR:
	ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
}

type TunnelStartRpfAction struct {
	AgentId     string `json:"agent_id"`
	Listen      bool   `json:"listen"`
	Description string `json:"desc"`
	Port        int    `json:"port"`
	Thost       string `json:"t_host"`
	Tport       int    `json:"t_port"`
}

func (tc *TsConnector) TcTunnelStartRpf(ctx *gin.Context) {
	var (
		ta         TunnelStartRpfAction
		value      any
		exists     bool
		ok         bool
		clientName string
		tunnelId   string
	)

	err := ctx.ShouldBindJSON(&ta)
	if err != nil {
		err = errors.New("invalid JSON data")
		goto ERR
	}

	value, exists = ctx.Get("username")
	if !exists {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: username not found in context", "ok": false})
		return
	}
	clientName, ok = value.(string)
	if !ok {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: invalid username type in context", "ok": false})
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

	_, err = tc.teamserver.TsTunnelClientStart(ta.AgentId, ta.Listen, 5, ta.Description, "", ta.Port, clientName, ta.Thost, ta.Tport, "", "")
	if err != nil {
		goto ERR
	}

	ctx.JSON(http.StatusOK, gin.H{"message": tunnelId, "ok": true})
	return

ERR:
	ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
}

type TunnelStopAction struct {
	TunnelId string `json:"p_tunnel_id"`
}

func (tc *TsConnector) TcTunnelStop(ctx *gin.Context) {
	var tunnelAction TunnelStopAction
	err := ctx.ShouldBindJSON(&tunnelAction)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
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

	err = tc.teamserver.TsTunnelClientStop(tunnelAction.TunnelId, clientName)
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
	var tunnelAction TunnelSetItemAction
	err := ctx.ShouldBindJSON(&tunnelAction)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
		return
	}

	err = tc.teamserver.TsTunnelClientSetInfo(tunnelAction.TunnelId, tunnelAction.Info)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "Tunnel stopped", "ok": true})
}
