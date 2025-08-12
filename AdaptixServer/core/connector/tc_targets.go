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
