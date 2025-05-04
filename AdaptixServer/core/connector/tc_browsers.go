package connector

import (
	"errors"
	"github.com/gin-gonic/gin"
	"net/http"
)

///

type DisksAction struct {
	AgentId string `json:"agent_id"`
}

func (tc *TsConnector) TcGuiDisks(ctx *gin.Context) {
	var (
		disksAction DisksAction
		username    string
		ok          bool
		err         error
		answer      gin.H
	)

	err = ctx.ShouldBindJSON(&disksAction)
	if err != nil {
		_ = ctx.Error(errors.New("invalid action"))
		return
	}

	value, exists := ctx.Get("username")
	if !exists {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: username not found in context", "ok": false})
		return
	}

	username, ok = value.(string)
	if !ok {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: invalid username type in context", "ok": false})
		return
	}

	err = tc.teamserver.TsAgentGuiDisks(disksAction.AgentId, username)
	if err != nil {
		answer = gin.H{"message": err.Error(), "ok": false}
	} else {
		answer = gin.H{"message": "Wait...", "ok": true}
	}
	ctx.JSON(http.StatusOK, answer)
}

///

type FilesAction struct {
	AgentId string `json:"agent_id"`
	Path    string `json:"path"`
}

func (tc *TsConnector) TcGuiFiles(ctx *gin.Context) {
	var (
		filesAction FilesAction
		username    string
		ok          bool
		err         error
		answer      gin.H
	)

	err = ctx.ShouldBindJSON(&filesAction)
	if err != nil {
		_ = ctx.Error(errors.New("invalid action"))
		return
	}

	value, exists := ctx.Get("username")
	if !exists {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: username not found in context", "ok": false})
		return
	}

	username, ok = value.(string)
	if !ok {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: invalid username type in context", "ok": false})
		return
	}

	err = tc.teamserver.TsAgentGuiFiles(filesAction.AgentId, filesAction.Path, username)
	if err != nil {
		answer = gin.H{"message": err.Error(), "ok": false}
	} else {
		answer = gin.H{"message": "Wait...", "ok": true}
	}
	ctx.JSON(http.StatusOK, answer)
}

///

type UploadAction struct {
	AgentId    string `json:"agent_id"`
	RemotePath string `json:"remote_path"`
	Content    []byte `json:"content"`
}

func (tc *TsConnector) TcGuiUpload(ctx *gin.Context) {
	var (
		uploadAction UploadAction
		username     string
		ok           bool
		err          error
		answer       gin.H
	)

	err = ctx.ShouldBindJSON(&uploadAction)
	if err != nil {
		_ = ctx.Error(errors.New("invalid action"))
		return
	}

	value, exists := ctx.Get("username")
	if !exists {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: username not found in context", "ok": false})
		return
	}

	username, ok = value.(string)
	if !ok {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: invalid username type in context", "ok": false})
		return
	}

	err = tc.teamserver.TsAgentGuiUpload(uploadAction.AgentId, uploadAction.RemotePath, uploadAction.Content, username)
	if err != nil {
		answer = gin.H{"message": err.Error(), "ok": false}
	} else {
		answer = gin.H{"message": "Uploading...", "ok": true}
	}
	ctx.JSON(http.StatusOK, answer)
}

///

type ProcessAction struct {
	AgentId string `json:"agent_id"`
}

func (tc *TsConnector) TcGuiProcess(ctx *gin.Context) {
	var (
		processAction ProcessAction
		username      string
		ok            bool
		err           error
		answer        gin.H
	)

	err = ctx.ShouldBindJSON(&processAction)
	if err != nil {
		_ = ctx.Error(errors.New("invalid action"))
		return
	}

	value, exists := ctx.Get("username")
	if !exists {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: username not found in context", "ok": false})
		return
	}

	username, ok = value.(string)
	if !ok {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: invalid username type in context", "ok": false})
		return
	}

	err = tc.teamserver.TsAgentGuiProcess(processAction.AgentId, username)
	if err != nil {
		answer = gin.H{"message": err.Error(), "ok": false}
	} else {
		answer = gin.H{"message": "Wait...", "ok": true}
	}
	ctx.JSON(http.StatusOK, answer)
}
