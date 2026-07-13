package connector

import (
	"encoding/base64"
	"errors"
	"net/http"
	"strconv"
	"strings"

	"github.com/gin-gonic/gin"
)

func (tc *TsConnector) TcDownloadList(ctx *gin.Context) {
	agentId := int64(0)
	if raw := ctx.Query("agent_id"); raw != "" {
		v, err := strconv.ParseInt(raw, 10, 64)
		if err != nil {
			respondError(ctx, http.StatusBadRequest, "agent_id must be an integer")
			return
		}
		agentId = v
	}
	limit := 100
	offset := 0
	filterExpr := ctx.Query("q")
	sortCol := ctx.Query("sort")
	sortOrder := ctx.Query("order")

	if raw := ctx.Query("limit"); raw != "" {
		v, err := strconv.Atoi(raw)
		if err != nil || v < 0 {
			respondError(ctx, http.StatusBadRequest, "limit must be a non-negative integer")
			return
		}
		if v > 1000 {
			v = 1000
		}
		limit = v
	}

	if raw := ctx.Query("offset"); raw != "" {
		v, err := strconv.Atoi(raw)
		if err != nil || v < 0 {
			respondError(ctx, http.StatusBadRequest, "offset must be a non-negative integer")
			return
		}
		offset = v
	}

	jsonData, err := tc.teamserver.TsDownloadsGetPage(agentId, offset, limit, filterExpr, sortCol, sortOrder)
	if err != nil {
		respondError(ctx, http.StatusBadRequest, err.Error())
		return
	}

	ctx.Data(http.StatusOK, "application/json; charset=utf-8", jsonData)
}

type DownloadFileId struct {
	File int64 `json:"file_id"`
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
		parts := strings.SplitN(filename, "_", 2)
		if len(parts) > 1 {
			filename = parts[1]
		}
		answer = gin.H{"ok": true, "filename": filename, "content": encodedContent}
	}

	ctx.JSON(http.StatusOK, answer)
}

type DownloadsDelete struct {
	FilesId []int64 `json:"file_id_array"`
}

func (tc *TsConnector) TcGuiDownloadDelete(ctx *gin.Context) {
	var downloadsDelete DownloadsDelete
	err := ctx.ShouldBindJSON(&downloadsDelete)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	err = tc.teamserver.TsDownloadDelete(downloadsDelete.FilesId)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOK(ctx)
}

type DownloadsTag struct {
	FilesId []int64 `json:"id_array"`
	Tag     string  `json:"tag"`
}

func (tc *TsConnector) TcDownloadSetTag(ctx *gin.Context) {
	var req DownloadsTag
	if err := ctx.ShouldBindJSON(&req); err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	if err := tc.teamserver.TsDownloadSetTag(req.FilesId, req.Tag); err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOK(ctx)
}
