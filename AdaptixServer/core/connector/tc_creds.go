package connector

import (
	"net/http"
	"strconv"

	"github.com/gin-gonic/gin"
)

func (tc *TsConnector) TcCredentialsList(ctx *gin.Context) {
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

	jsonData, err := tc.teamserver.TsCredentialsGetPage(agentId, offset, limit, filterExpr, sortCol, sortOrder)
	if err != nil {
		respondError(ctx, http.StatusBadRequest, err.Error())
		return
	}

	ctx.Data(http.StatusOK, "application/json; charset=utf-8", jsonData)
}

type CredsAdd struct {
	Username string `json:"username"`
	Password string `json:"password"`
	Realm    string `json:"realm"`
	Type     string `json:"type"`
	Tag      string `json:"tag"`
	Storage  string `json:"storage"`
	Host     string `json:"host"`
}

func (tc *TsConnector) TcCredentialsAdd(ctx *gin.Context) {
	var m map[string]interface{}
	var creds []map[string]interface{}

	if err := ctx.ShouldBindJSON(&m); err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}
	arr, ok := m["creds"].([]interface{})
	if !ok {
		respondError(ctx, http.StatusOK, "invalid JSON structure")
		return
	}
	for _, v := range arr {
		if obj, ok := v.(map[string]interface{}); ok {
			creds = append(creds, obj)
		}
	}

	err := tc.teamserver.TsCredentilsAdd(creds)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOK(ctx)
}

type CredsEdit struct {
	CredId   int64  `json:"cred_id"`
	Username string `json:"username"`
	Password string `json:"password"`
	Realm    string `json:"realm"`
	Type     string `json:"type"`
	Tag      string `json:"tag"`
	Storage  string `json:"storage"`
	Host     string `json:"host"`
}

func (tc *TsConnector) TcCredentialsEdit(ctx *gin.Context) {
	var credsEdit CredsEdit
	err := ctx.ShouldBindJSON(&credsEdit)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	err = tc.teamserver.TsCredentilsEdit(credsEdit.CredId, credsEdit.Username, credsEdit.Password, credsEdit.Realm, credsEdit.Type, credsEdit.Tag, credsEdit.Storage, credsEdit.Host)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOK(ctx)
}

type CredsTag struct {
	CredIdArray []int64 `json:"id_array"`
	Tag         string  `json:"tag"`
}

func (tc *TsConnector) TcCredentialsSetTag(ctx *gin.Context) {
	var (
		credsTag CredsTag
		err      error
	)

	err = ctx.ShouldBindJSON(&credsTag)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	err = tc.teamserver.TsCredentialsSetTag(credsTag.CredIdArray, credsTag.Tag)

	respondOK(ctx)
}

type CredsRemove struct {
	CredsId []int64 `json:"cred_id_array"`
}

func (tc *TsConnector) TcCredentialsRemove(ctx *gin.Context) {
	var credsRemove CredsRemove
	err := ctx.ShouldBindJSON(&credsRemove)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	err = tc.teamserver.TsCredentilsDelete(credsRemove.CredsId)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOK(ctx)
}
