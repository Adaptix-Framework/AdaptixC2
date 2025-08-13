package connector

import (
	"github.com/gin-gonic/gin"
	"net/http"
)

func (tc *TsConnector) TcTargetsAdd(ctx *gin.Context) {
	var m map[string]interface{}
	var targets []map[string]interface{}

	if err := ctx.ShouldBindJSON(&m); err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
		return
	}
	arr, ok := m["targets"].([]interface{})
	if !ok {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON structure", "ok": false})
		return
	}
	for _, v := range arr {
		if obj, ok := v.(map[string]interface{}); ok {
			targets = append(targets, obj)
		}
	}

	err := tc.teamserver.TsTargetsAdd(targets)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}

type TargetEdit struct {
	TargetId string `json:"t_target_id"`
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
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
		return
	}

	err = tc.teamserver.TsTargetsEdit(targetEdit.TargetId, targetEdit.Computer, targetEdit.Domain, targetEdit.Address, targetEdit.Os, targetEdit.OsDesk, targetEdit.Tag, targetEdit.Info, targetEdit.Alive)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}

type TargetRemove struct {
	TargetId string `json:"t_target_id"`
}

func (tc *TsConnector) TcTargetRemove(ctx *gin.Context) {
	var targetRemove TargetRemove
	err := ctx.ShouldBindJSON(&targetRemove)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
		return
	}

	err = tc.teamserver.TsTargetDelete(targetRemove.TargetId)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}
