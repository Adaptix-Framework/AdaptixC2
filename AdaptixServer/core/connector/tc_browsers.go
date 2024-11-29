package connector

import (
	"encoding/base64"
	"errors"
	"github.com/gin-gonic/gin"
	"net/http"
	"strings"
)

type DownloadAction struct {
	Action string `json:"action"`
	File   string `json:"file"`
}

func (tc *TsConnector) TcBrowserDownload(ctx *gin.Context) {
	var (
		downloadAction DownloadAction
		answer         gin.H
		username       string
		ok             bool
		err            error
	)

	err = ctx.ShouldBindJSON(&downloadAction)
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

	if downloadAction.Action == "sync" {

		filename, content, err := tc.teamserver.TsDownloadSync(downloadAction.File)
		if err != nil {
			answer = gin.H{"message": err.Error(), "ok": false}
		} else {
			encodedContent := base64.StdEncoding.EncodeToString(content)
			filename = strings.Split(filename, "_")[1]

			answer = gin.H{"ok": true, "filename": filename, "content": encodedContent}
		}

	} else if downloadAction.Action == "delete" {

		err = tc.teamserver.TsDownloadDelete(downloadAction.File)
		if err != nil {
			answer = gin.H{"message": err.Error(), "ok": false}
		} else {
			answer = gin.H{"message": "file delete", "ok": true}
		}

	} else if downloadAction.Action == "cancel" || downloadAction.Action == "stop" || downloadAction.Action == "start" {

		err = tc.teamserver.TsDownloadChangeState(downloadAction.File, username, downloadAction.Action)
		if err != nil {
			answer = gin.H{"message": err.Error(), "ok": false}
		} else {
			answer = gin.H{"message": "file " + downloadAction.Action, "ok": true}
		}

	} else {
		answer = gin.H{"message": "Unknown action", "ok": false}
	}

	ctx.JSON(http.StatusOK, answer)
}

///

type DisksAction struct {
	AgentId string `json:"agent_id"`
}

func (tc *TsConnector) TcBrowserDisks(ctx *gin.Context) {
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

	err = tc.teamserver.TsAgentBrowserDisks(disksAction.AgentId, username)
	if err != nil {
		answer = gin.H{"message": err.Error(), "ok": false}
	} else {
		answer = gin.H{"message": "Wait...", "ok": true}
	}
	ctx.JSON(http.StatusOK, answer)
}
