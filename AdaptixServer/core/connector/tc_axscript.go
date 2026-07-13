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
		respondError(ctx, http.StatusOK, err.Error())
		return
	}
	ctx.Data(http.StatusOK, "application/json; charset=utf-8", []byte(jsonScripts))
}

func (tc *TsConnector) TcAxScriptCommands(ctx *gin.Context) {
	jsonCommands, err := tc.teamserver.TsAxScriptCommands()
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}
	ctx.Data(http.StatusOK, "application/json; charset=utf-8", []byte(jsonCommands))
}

func (tc *TsConnector) TcAxScriptLoad(ctx *gin.Context) {
	var data AxScriptLoadData

	err := ctx.ShouldBindJSON(&data)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	if tc.ManagePasswordHash != "" {
		if data.ManagePassword == "" || krypt.SHA256([]byte(data.ManagePassword)) != tc.ManagePasswordHash {
			respondError(ctx, http.StatusOK, "invalid manage_password")
			return
		}
	}

	if data.Name == "" || data.Script == "" {
		respondError(ctx, http.StatusOK, "name and script are required")
		return
	}

	err = tc.teamserver.TsAxScriptLoadUser(data.Name, data.Script)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOK(ctx)
}

func (tc *TsConnector) TcAxScriptUnload(ctx *gin.Context) {
	var data AxScriptUnloadData

	err := ctx.ShouldBindJSON(&data)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	if tc.ManagePasswordHash != "" {
		if data.ManagePassword == "" || krypt.SHA256([]byte(data.ManagePassword)) != tc.ManagePasswordHash {
			respondError(ctx, http.StatusOK, "invalid manage_password")
			return
		}
	}

	if data.Name == "" {
		respondError(ctx, http.StatusOK, "name is required")
		return
	}

	err = tc.teamserver.TsAxScriptUnloadUser(data.Name)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOK(ctx)
}
