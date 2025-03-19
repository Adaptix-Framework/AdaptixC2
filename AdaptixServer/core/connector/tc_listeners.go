package connector

import (
	isvalid "AdaptixServer/core/utils/valid"
	"errors"
	"github.com/gin-gonic/gin"
	"net/http"
)

type ListenerConfig struct {
	ListenerName string `json:"name"`
	ConfigType   string `json:"type"`
	Config       string `json:"config"`
}

func (tc *TsConnector) TcListenerStart(ctx *gin.Context) {
	var (
		listener ListenerConfig
		err      error
	)

	err = ctx.ShouldBindJSON(&listener)
	if err != nil {
		_ = ctx.Error(errors.New("invalid listener"))
		return
	}

	if isvalid.ValidListenerName(listener.ListenerName) == false {
		ctx.JSON(http.StatusOK, gin.H{"message": "Invalid listener name", "ok": false})
		return
	}

	err = tc.teamserver.TsListenerStart(listener.ListenerName, listener.ConfigType, listener.Config, "", nil)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "Listener started", "ok": true})
}

func (tc *TsConnector) TcListenerStop(ctx *gin.Context) {
	var (
		listener ListenerConfig
		err      error
	)

	err = ctx.ShouldBindJSON(&listener)
	if err != nil {
		_ = ctx.Error(errors.New("invalid listener"))
		return
	}

	err = tc.teamserver.TsListenerStop(listener.ListenerName, listener.ConfigType)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "Listener stopped", "ok": true})
}

func (tc *TsConnector) TcListenerEdit(ctx *gin.Context) {
	var (
		listener ListenerConfig
		err      error
	)

	err = ctx.ShouldBindJSON(&listener)
	if err != nil {
		_ = ctx.Error(errors.New("invalid listener"))
		return
	}

	err = tc.teamserver.TsListenerEdit(listener.ListenerName, listener.ConfigType, listener.Config)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "Listener Edited", "ok": true})
}
