package httphandler

import (
	"errors"
	"github.com/gin-gonic/gin"
	"net/http"
)

type ListenerConfig struct {
	ListenerName string `json:"name"`
	ConfigType   string `json:"type"`
	Config       string `json:"config"`
}

func (th *TsHttpHandler) ListenerStart(ctx *gin.Context) {
	var (
		listener ListenerConfig
		err      error
	)

	err = ctx.ShouldBindJSON(&listener)
	if err != nil {
		_ = ctx.Error(errors.New("invalid listener"))
		return
	}

	err = th.teamserver.ListenerStart(listener.ListenerName, listener.ConfigType, listener.Config)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "Listener started", "ok": true})
}

func (th *TsHttpHandler) ListenerStop(ctx *gin.Context) {
	var (
		listener ListenerConfig
		err      error
	)

	err = ctx.ShouldBindJSON(&listener)
	if err != nil {
		_ = ctx.Error(errors.New("invalid listener"))
		return
	}

	err = th.teamserver.ListenerStop(listener.ListenerName, listener.ConfigType)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "Listener stopped", "ok": true})
}

func (th *TsHttpHandler) ListenerEdit(ctx *gin.Context) {
	var (
		listener ListenerConfig
		err      error
	)

	err = ctx.ShouldBindJSON(&listener)
	if err != nil {
		_ = ctx.Error(errors.New("invalid listener"))
		return
	}

	err = th.teamserver.ListenerEdit(listener.ListenerName, listener.ConfigType, listener.Config)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "Listener Edited", "ok": true})
}
