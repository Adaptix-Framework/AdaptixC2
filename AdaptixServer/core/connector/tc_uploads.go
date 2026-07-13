package connector

import (
	"errors"
	"net/http"
	"strconv"

	"github.com/gin-gonic/gin"
)

func (tc *TsConnector) TcUploadList(ctx *gin.Context) {
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

	jsonData, err := tc.teamserver.TsUploadsGetPage(agentId, offset, limit, filterExpr, sortCol, sortOrder)
	if err != nil {
		respondError(ctx, http.StatusBadRequest, err.Error())
		return
	}

	ctx.Data(http.StatusOK, "application/json; charset=utf-8", jsonData)
}

type UploadFileIdsRequest struct {
	IdArray []int64 `json:"id_array"`
}

func (tc *TsConnector) TcUploadDelete(ctx *gin.Context) {
	var req UploadFileIdsRequest
	if err := ctx.ShouldBindJSON(&req); err != nil {
		_ = ctx.Error(errors.New("invalid action"))
		return
	}

	if err := tc.teamserver.TsUploadDelete(req.IdArray); err != nil {
		respondError(ctx, http.StatusBadRequest, err.Error())
		return
	}
	respondOK(ctx)
}
