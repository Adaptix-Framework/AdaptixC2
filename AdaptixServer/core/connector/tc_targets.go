package connector

import (
	"net/http"
	"strconv"

	"github.com/gin-gonic/gin"
)

func (tc *TsConnector) TcTargetsList(ctx *gin.Context) {
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

	jsonData, err := tc.teamserver.TsTargetsGetPage(offset, limit, filterExpr, sortCol, sortOrder)
	if err != nil {
		respondError(ctx, http.StatusBadRequest, err.Error())
		return
	}

	ctx.Data(http.StatusOK, "application/json; charset=utf-8", jsonData)
}

func (tc *TsConnector) TcTargetsAdd(ctx *gin.Context) {
	var m map[string]interface{}
	var targets []map[string]interface{}

	if err := ctx.ShouldBindJSON(&m); err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}
	arr, ok := m["targets"].([]interface{})
	if !ok {
		respondError(ctx, http.StatusOK, "invalid JSON structure")
		return
	}
	for _, v := range arr {
		if obj, ok := v.(map[string]interface{}); ok {
			targets = append(targets, obj)
		}
	}

	err := tc.teamserver.TsTargetsAdd(targets)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOK(ctx)
}

type TargetEdit struct {
	TargetId int64  `json:"t_target_id"`
	Computer string `json:"t_computer"`
	Domain   string `json:"t_domain"`
	Address  string `json:"t_address"`
	Os       int    `json:"t_os"`
	OsDesk   string `json:"t_os_desk"`
	Tag      string `json:"t_tag"`
	Info     string `json:"t_info"`
	Alive    bool   `json:"t_alive"`
}

func (tc *TsConnector) TcTargetEdit(ctx *gin.Context) {
	var targetEdit TargetEdit
	err := ctx.ShouldBindJSON(&targetEdit)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	err = tc.teamserver.TsTargetsEdit(targetEdit.TargetId, targetEdit.Computer, targetEdit.Domain, targetEdit.Address, targetEdit.Os, targetEdit.OsDesk, targetEdit.Tag, targetEdit.Info, targetEdit.Alive)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOK(ctx)
}

type TargetsTag struct {
	TargetIdArray []int64 `json:"id_array"`
	Tag           string  `json:"tag"`
}

func (tc *TsConnector) TcTargetSetTag(ctx *gin.Context) {
	var (
		targetsTag TargetsTag
		err        error
	)

	err = ctx.ShouldBindJSON(&targetsTag)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	err = tc.teamserver.TsTargetSetTag(targetsTag.TargetIdArray, targetsTag.Tag)

	respondOK(ctx)
}

type TargetRemove struct {
	TargetsId []int64 `json:"target_id_array"`
}

func (tc *TsConnector) TcTargetRemove(ctx *gin.Context) {
	var targetRemove TargetRemove
	err := ctx.ShouldBindJSON(&targetRemove)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	err = tc.teamserver.TsTargetDelete(targetRemove.TargetsId)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOK(ctx)
}
