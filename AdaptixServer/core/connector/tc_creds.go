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
	var credsAdd CredsAdd
	err := ctx.ShouldBindJSON(&credsAdd)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
		return
	}

	err = tc.teamserver.TsCredentilsAdd(credsAdd.Username, credsAdd.Password, credsAdd.Realm, credsAdd.Type, credsAdd.Tag, credsAdd.Storage, "", credsAdd.Host)
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
