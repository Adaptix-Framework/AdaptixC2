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
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	ctx.Data(http.StatusOK, "application/json; charset=utf-8", []byte(jsonListeners))
}

func (tc *TsConnector) TcPluginListenerCall(ctx *gin.Context) {
	var jsonData struct {
		Listener string `json:"listener"`
		Command  string `json:"command"`
		Args     string `json:"args"`
	}

	username, ok := mustUsername(ctx)
	if !ok {
		return
	}

	if err := ctx.ShouldBindJSON(&jsonData); err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "error", "error": err.Error()})
		return
	}
	if jsonData.Listener == "" {
		ctx.JSON(http.StatusOK, gin.H{"message": "error", "error": "listener is required"})
		return
	}
	if jsonData.Command == "" {
		ctx.JSON(http.StatusOK, gin.H{"message": "error", "error": "command is required"})
		return
	}

	go tc.teamserver.TsPluginListenerCall(jsonData.Listener, username, jsonData.Command, jsonData.Args)
	ctx.JSON(http.StatusOK, gin.H{"message": "success", "result": "ok"})
}

type ListenerConfig struct {
	ListenerName string `json:"name"`
	ConfigType   string `json:"type"`
	Config       string `json:"config"`
	Tags         string `json:"tags"`
}

func (tc *TsConnector) TcListenerStart(ctx *gin.Context) {
	var listener ListenerConfig
	err := ctx.ShouldBindJSON(&listener)
	if err != nil {
		_ = ctx.Error(errors.New("invalid listener"))
		return
	}

	if isvalid.ValidListenerName(listener.ListenerName) == false {
		respondError(ctx, http.StatusOK, "Invalid listener name")
		return
	}

	err = tc.teamserver.TsListenerStart(listener.ListenerName, listener.ConfigType, listener.Config, 0, "", nil, listener.Tags)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOKMessage(ctx, "Listener started")
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
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOKMessage(ctx, "Listener stopped")
}

func (tc *TsConnector) TcListenerEdit(ctx *gin.Context) {
	var listener ListenerConfig
	err := ctx.ShouldBindJSON(&listener)
	if err != nil {
		_ = ctx.Error(errors.New("invalid listener"))
		return
	}

	err = tc.teamserver.TsListenerEdit(listener.ListenerName, listener.ConfigType, listener.Config, listener.Tags)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOKMessage(ctx, "Listener Edited")
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
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOKMessage(ctx, "Listener paused")
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
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOKMessage(ctx, "Listener resumed")
}

type ListenerConnector struct {
	ListenerName string `json:"listener_name"`
	Data         string `json:"data"`
}

func (tc *TsConnector) TcListenerConnector(ctx *gin.Context) {
	var req ListenerConnector
	err := ctx.ShouldBindJSON(&req)
	if err != nil {
		_ = ctx.Error(errors.New("invalid agent connector"))
		return
	}
	if req.ListenerName == "" {
		respondError(ctx, http.StatusOK, "listener_name is required")
		return
	}

	agentId, errConnector := tc.teamserver.TsListenerConnector(req.ListenerName, []byte(req.Data))
	if errConnector != nil {
		respondError(ctx, http.StatusOK, errConnector.Error())
		return
	}

	respondOKMessage(ctx, agentId)
}

type ListenerTagsConfig struct {
	ListenerName string `json:"name"`
	Tags         string `json:"tags"`
}

func (tc *TsConnector) TcListenerSetTags(ctx *gin.Context) {
	var req ListenerTagsConfig
	err := ctx.ShouldBindJSON(&req)
	if err != nil {
		_ = ctx.Error(errors.New("invalid request"))
		return
	}

	err = tc.teamserver.TsListenerSetTags(req.ListenerName, req.Tags)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOKMessage(ctx, "Tags updated")
}
