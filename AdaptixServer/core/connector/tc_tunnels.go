package connector

import (
	"errors"
	"github.com/gin-gonic/gin"
	"net/http"
)

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

	err = tc.teamserver.TsTunnelStop(tunnelAction.TunnelId)
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
