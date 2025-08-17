package connector

import (
	"github.com/gin-gonic/gin"
	"net/http"
)

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
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
		return
	}
	arr, ok := m["creds"].([]interface{})
	if !ok {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON structure", "ok": false})
		return
	}
	for _, v := range arr {
		if obj, ok := v.(map[string]interface{}); ok {
			creds = append(creds, obj)
		}
	}

	err := tc.teamserver.TsCredentilsAdd(creds)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}

type CredsEdit struct {
	CredId   string `json:"cred_id"`
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
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
		return
	}

	err = tc.teamserver.TsCredentilsEdit(credsEdit.CredId, credsEdit.Username, credsEdit.Password, credsEdit.Realm, credsEdit.Type, credsEdit.Tag, credsEdit.Storage, credsEdit.Host)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}

type CredsTag struct {
	CredIdArray []string `json:"id_array"`
	Tag         string   `json:"tag"`
}

func (tc *TsConnector) TcCredentialsSetTag(ctx *gin.Context) {
	var (
		credsTag CredsTag
		err      error
	)

	err = ctx.ShouldBindJSON(&credsTag)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
		return
	}

	err = tc.teamserver.TsCredentialsSetTag(credsTag.CredIdArray, credsTag.Tag)

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}

type CredsRemove struct {
	CredId string `json:"cred_id"`
}

func (tc *TsConnector) TcCredentialsRemove(ctx *gin.Context) {
	var credsRemove CredsRemove
	err := ctx.ShouldBindJSON(&credsRemove)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
		return
	}

	err = tc.teamserver.TsCredentilsDelete(credsRemove.CredId)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}
