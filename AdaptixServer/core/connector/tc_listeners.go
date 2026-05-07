package connector

import (
	isvalid "AdaptixServer/core/utils/valid"
	"errors"
	"net/http"

	"github.com/gin-gonic/gin"
)

func (tc *TsConnector) TcListenerList(ctx *gin.Context) {
	jsonListeners, err := tc.teamserver.TsListenerList()
	if err != nil {
		ctx.JSON(http.StatusOK, payload(false, err.Error()))
		return
	}

	ctx.Data(http.StatusOK, "application/json; charset=utf-8", []byte(jsonListeners))
}

type ListenerConfig struct {
	ListenerName string `json:"name"`
	ConfigType   string `json:"type"`
	Config       string `json:"config"`
}

func (tc *TsConnector) TcListenerStart(ctx *gin.Context) {
	var listener ListenerConfig
	err := ctx.ShouldBindJSON(&listener)
	if err != nil {
		_ = ctx.Error(errors.New("invalid listener"))
		return
	}

	if isvalid.ValidListenerName(listener.ListenerName) == false {
		ctx.JSON(http.StatusOK, payload(false, "Invalid listener name"))
		return
	}

	err = tc.teamserver.TsListenerStart(listener.ListenerName, listener.ConfigType, listener.Config, 0, "", nil)
	if err != nil {
		ctx.JSON(http.StatusOK, payload(false, err.Error()))
		return
	}

	ctx.JSON(http.StatusOK, payload(true, "Listener started"))
}

func (tc *TsConnector) TcListenerStop(ctx *gin.Context) {
	var listener ListenerConfig
	err := ctx.ShouldBindJSON(&listener)
	if err != nil {
		_ = ctx.Error(errors.New("invalid listener"))
		return
	}

	err = tc.teamserver.TsListenerStop(listener.ListenerName, listener.ConfigType)
	if err != nil {
		ctx.JSON(http.StatusOK, payload(false, err.Error()))
		return
	}

	ctx.JSON(http.StatusOK, payload(true, "Listener stopped"))
}

func (tc *TsConnector) TcListenerEdit(ctx *gin.Context) {
	var listener ListenerConfig
	err := ctx.ShouldBindJSON(&listener)
	if err != nil {
		_ = ctx.Error(errors.New("invalid listener"))
		return
	}

	err = tc.teamserver.TsListenerEdit(listener.ListenerName, listener.ConfigType, listener.Config)
	if err != nil {
		ctx.JSON(http.StatusOK, payload(false, err.Error()))
		return
	}

	ctx.JSON(http.StatusOK, payload(true, "Listener Edited"))
}

func (tc *TsConnector) TcListenerPause(ctx *gin.Context) {
	var listener ListenerConfig
	err := ctx.ShouldBindJSON(&listener)
	if err != nil {
		_ = ctx.Error(errors.New("invalid listener"))
		return
	}

	err = tc.teamserver.TsListenerPause(listener.ListenerName, listener.ConfigType)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "Listener paused", "ok": true})
}

func (tc *TsConnector) TcListenerResume(ctx *gin.Context) {
	var listener ListenerConfig
	err := ctx.ShouldBindJSON(&listener)
	if err != nil {
		_ = ctx.Error(errors.New("invalid listener"))
		return
	}

	err = tc.teamserver.TsListenerResume(listener.ListenerName, listener.ConfigType)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "Listener resumed", "ok": true})
}
