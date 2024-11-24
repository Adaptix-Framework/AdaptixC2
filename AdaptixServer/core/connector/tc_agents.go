package connector

import (
	"encoding/json"
	"github.com/gin-gonic/gin"
	"log"
	"net/http"
)

type CommandData struct {
	AgentName string `json:"name"`
	AgentId   string `json:"id"`
	CmdLine   string `json:"cmdline"`
	Data      string `json:"data"`
}

type AgentRemove struct {
	AgentId string `json:"id"`
}

type AgentTag struct {
	AgentId string `json:"id"`
	Tag     string `json:"tag"`
}

func (tc *TsConnector) TcAgentCommand(ctx *gin.Context) {
	var (
		username    string
		commandData CommandData
		args        map[string]any
		ok          bool
		err         error
	)

	err = ctx.ShouldBindJSON(&commandData)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid command data", "ok": false})
		return
	}

	value, exists := ctx.Get("username")
	if !exists {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: username not found in context", "ok": false})
		return
	}

	username, ok = value.(string)
	if !ok {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: invalid username type in context", "ok": false})
		return
	}

	err = json.Unmarshal([]byte(commandData.Data), &args)
	if err != nil {
		log.Fatalf("Error parsing commands JSON: %v", err)
	}

	err = tc.teamserver.TsAgentCommand(commandData.AgentName, commandData.AgentId, username, commandData.CmdLine, args)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}

func (tc *TsConnector) TcAgentRemove(ctx *gin.Context) {
	var (
		agentRemove AgentRemove
		err         error
	)

	err = ctx.ShouldBindJSON(&agentRemove)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid command data", "ok": false})
		return
	}

	err = tc.teamserver.TsAgentRemove(agentRemove.AgentId)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}

func (tc *TsConnector) TcAgentSetTag(ctx *gin.Context) {
	var (
		agentTag AgentTag
		err      error
	)

	err = ctx.ShouldBindJSON(&agentTag)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid command data", "ok": false})
		return
	}

	err = tc.teamserver.TsAgentSetTag(agentTag.AgentId, agentTag.Tag)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}
