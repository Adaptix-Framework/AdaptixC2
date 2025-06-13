package connector

import (
	"encoding/base64"
	"errors"
	"github.com/gin-gonic/gin"
	"net/http"
	"os"
	"strings"
)

func (tc *TsConnector) TcDownloadSyncOTP(ctx *gin.Context) {

	value, exists := ctx.Get("objectId")
	if !exists {
		ctx.String(http.StatusNotFound, "Server error: objectId not found in context")
		return
	}

	fileId, ok := value.(string)
	if !ok {
		ctx.String(http.StatusNotFound, "Server error: invalid fileId type in context")
		return
	}

	path, err := tc.teamserver.TsDownloadGetFilepath(fileId)
	if err != nil {
		ctx.String(http.StatusNotFound, err.Error())
		return
	}

	file, err := os.Open(path)
	if err != nil {
		ctx.String(http.StatusNotFound, "File not found")
		return
	}
	defer func(file *os.File) {
		_ = file.Close()
	}(file)

	fileInfo, err := file.Stat()
	if err != nil {
		ctx.String(http.StatusInternalServerError, "Cannot get file info")
		return
	}

	ctx.Header("Content-Disposition", "attachment; filename="+fileInfo.Name())
	ctx.Header("Content-Type", "application/octet-stream")
	ctx.Header("Content-Length", string(rune(fileInfo.Size())))

	ctx.File(path)
}

type DownloadFileId struct {
	File string `json:"file_id"`
}

func (tc *TsConnector) TcGuiDownloadSync(ctx *gin.Context) {
	var (
		downloadFid DownloadFileId
		answer      gin.H
		err         error
	)

	err = ctx.ShouldBindJSON(&downloadFid)
	if err != nil {
		_ = ctx.Error(errors.New("invalid action"))
		return
	}

	filename, content, err := tc.teamserver.TsDownloadSync(downloadFid.File)
	if err != nil {
		answer = gin.H{"message": err.Error(), "ok": false}
	} else {
		encodedContent := base64.StdEncoding.EncodeToString(content)
		filename = strings.Split(filename, "_")[1]

		answer = gin.H{"ok": true, "filename": filename, "content": encodedContent}
	}

	ctx.JSON(http.StatusOK, answer)
}

func (tc *TsConnector) TcGuiDownloadDelete(ctx *gin.Context) {
	var (
		downloadFid DownloadFileId
		answer      gin.H
		err         error
	)

	err = ctx.ShouldBindJSON(&downloadFid)
	if err != nil {
		_ = ctx.Error(errors.New("invalid action"))
		return
	}

	err = tc.teamserver.TsDownloadDelete(downloadFid.File)
	if err != nil {
		answer = gin.H{"message": err.Error(), "ok": false}
	} else {
		answer = gin.H{"message": "file delete", "ok": true}
	}

	ctx.JSON(http.StatusOK, answer)
}

type DownloadStartAction struct {
	AgentId string `json:"agent_id"`
	Path    string `json:"path"`
}

func (tc *TsConnector) TcGuiDownloadStart(ctx *gin.Context) {
	var (
		downloadAction DownloadStartAction
		username       string
		ok             bool
		err            error
		answer         gin.H
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

	err = tc.teamserver.TsDownloadTaskStart(downloadAction.AgentId, downloadAction.Path, username)
	if err != nil {
		answer = gin.H{"message": err.Error(), "ok": false}
	} else {
		answer = gin.H{"message": "Downloading...", "ok": true}
	}
	ctx.JSON(http.StatusOK, answer)
}

func (tc *TsConnector) TcGuiDownloadCancel(ctx *gin.Context) {
	var (
		downloadFid DownloadFileId
		answer      gin.H
		username    string
		ok          bool
		err         error
	)

	err = ctx.ShouldBindJSON(&downloadFid)
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

	err = tc.teamserver.TsDownloadTaskCancel(downloadFid.File, username)
	if err != nil {
		answer = gin.H{"message": err.Error(), "ok": false}
	} else {
		answer = gin.H{"message": "file cancel", "ok": true}
	}

	ctx.JSON(http.StatusOK, answer)
}

func (tc *TsConnector) TcGuiDownloadResume(ctx *gin.Context) {
	var (
		downloadFid DownloadFileId
		answer      gin.H
		username    string
		ok          bool
		err         error
	)

	err = ctx.ShouldBindJSON(&downloadFid)
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

	err = tc.teamserver.TsDownloadTaskResume(downloadFid.File, username)
	if err != nil {
		answer = gin.H{"message": err.Error(), "ok": false}
	} else {
		answer = gin.H{"message": "file resume", "ok": true}
	}

	ctx.JSON(http.StatusOK, answer)
}

func (tc *TsConnector) TcGuiDownloadPause(ctx *gin.Context) {
	var (
		downloadFid DownloadFileId
		answer      gin.H
		username    string
		ok          bool
		err         error
	)

	err = ctx.ShouldBindJSON(&downloadFid)
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

	err = tc.teamserver.TsDownloadTaskPause(downloadFid.File, username)
	if err != nil {
		answer = gin.H{"message": err.Error(), "ok": false}
	} else {
		answer = gin.H{"message": "file pause", "ok": true}
	}

	ctx.JSON(http.StatusOK, answer)
}
