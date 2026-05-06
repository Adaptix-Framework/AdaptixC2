package connector

import (
	"AdaptixServer/core/utils/krypt"
	"net/http"

	"github.com/gin-gonic/gin"
)

type AxScriptLoadData struct {
	Name           string `json:"name"`
	Script         string `json:"script"`
	ManagePassword string `json:"manage_password"`
}

type AxScriptUnloadData struct {
	Name           string `json:"name"`
	ManagePassword string `json:"manage_password"`
}

func (tc *TsConnector) TcAxScriptList(ctx *gin.Context) {
	jsonScripts, err := tc.teamserver.TsAxScriptList()
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}
	ctx.Data(http.StatusOK, "application/json; charset=utf-8", []byte(jsonScripts))
}

func (tc *TsConnector) TcAxScriptCommands(ctx *gin.Context) {
	jsonCommands, err := tc.teamserver.TsAxScriptCommands()
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}
	ctx.Data(http.StatusOK, "application/json; charset=utf-8", []byte(jsonCommands))
}

func (tc *TsConnector) TcAxScriptLoad(ctx *gin.Context) {
	var data AxScriptLoadData

	err := ctx.ShouldBindJSON(&data)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
		return
	}

	if tc.ManagePasswordHash != "" {
		if data.ManagePassword == "" || krypt.SHA256([]byte(data.ManagePassword)) != tc.ManagePasswordHash {
			ctx.JSON(http.StatusOK, gin.H{"message": "invalid manage_password", "ok": false})
			return
		}
	}

	if data.Name == "" || data.Script == "" {
		ctx.JSON(http.StatusOK, gin.H{"message": "name and script are required", "ok": false})
		return
	}

	err = tc.teamserver.TsAxScriptLoadUser(data.Name, data.Script)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}

func (tc *TsConnector) TcAxScriptUnload(ctx *gin.Context) {
	var data AxScriptUnloadData

	err := ctx.ShouldBindJSON(&data)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
		return
	}

	if tc.ManagePasswordHash != "" {
		if data.ManagePassword == "" || krypt.SHA256([]byte(data.ManagePassword)) != tc.ManagePasswordHash {
			ctx.JSON(http.StatusOK, gin.H{"message": "invalid manage_password", "ok": false})
			return
		}
	}

	if data.Name == "" {
		ctx.JSON(http.StatusOK, gin.H{"message": "name is required", "ok": false})
		return
	}

	err = tc.teamserver.TsAxScriptUnloadUser(data.Name)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}
