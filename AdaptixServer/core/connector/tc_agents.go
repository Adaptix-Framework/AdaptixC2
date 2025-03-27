package connector

import (
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"github.com/gin-gonic/gin"
	"log"
	"net/http"
)

type AgentConfig struct {
	ListenerName string `json:"listener_name"`
	ListenerType string `json:"listener_type"`
	AgentName    string `json:"agent"`
	Config       string `json:"config"`
}

func (tc *TsConnector) TcAgentGenerate(ctx *gin.Context) {
	var (
		agentConfig     AgentConfig
		err             error
		listenerProfile []byte
		listenerWM      string
		fileContent     []byte
		fileName        string
	)

	err = ctx.ShouldBindJSON(&agentConfig)
	if err != nil {
		_ = ctx.Error(errors.New("invalid agent config"))
		return
	}

	listenerWM, listenerProfile, err = tc.teamserver.TsListenerGetProfile(agentConfig.ListenerName, agentConfig.ListenerType)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	fileContent, fileName, err = tc.teamserver.TsAgentGenerate(agentConfig.AgentName, agentConfig.Config, listenerWM, listenerProfile)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	encodedContent := base64.StdEncoding.EncodeToString([]byte(fileName)) + ":" + base64.StdEncoding.EncodeToString(fileContent)

	ctx.JSON(http.StatusOK, gin.H{"message": encodedContent, "ok": true})
}

type CommandData struct {
	AgentName string `json:"name"`
	AgentId   string `json:"id"`
	CmdLine   string `json:"cmdline"`
	Data      string `json:"data"`
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

type AgentExit struct {
	AgentIdArray []string `json:"agent_id_array"`
}

func (tc *TsConnector) TcAgentExit(ctx *gin.Context) {
	var (
		agentExit AgentExit
		err       error
		username  string
		ok        bool
	)

	err = ctx.ShouldBindJSON(&agentExit)
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

	var errorsSlice []string
	for _, agentId := range agentExit.AgentIdArray {
		err = tc.teamserver.TsAgentGuiExit(agentId, username)
		if err != nil {
			errorsSlice = append(errorsSlice, err.Error())
		}
	}

	if len(errorsSlice) > 0 {
		message := ""
		for i, errorMessage := range errorsSlice {
			message += fmt.Sprintf("%d. %s\n", i+1, errorMessage)
		}

		ctx.JSON(http.StatusOK, gin.H{"message": message, "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}

type AgentRemove struct {
	AgentIdArray []string `json:"agent_id_array"`
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

	var errorsSlice []string
	for _, agentId := range agentRemove.AgentIdArray {
		err = tc.teamserver.TsAgentRemove(agentId)
		if err != nil {
			errorsSlice = append(errorsSlice, err.Error())
		}
	}

	if len(errorsSlice) > 0 {
		message := ""
		for i, errorMessage := range errorsSlice {
			message += fmt.Sprintf("%d. %s\n", i+1, errorMessage)
		}

		ctx.JSON(http.StatusOK, gin.H{"message": message, "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}

type AgentTag struct {
	AgentIdArray []string `json:"agent_id_array"`
	Tag          string   `json:"tag"`
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

	var errorsSlice []string
	for _, agentId := range agentTag.AgentIdArray {
		err = tc.teamserver.TsAgentSetTag(agentId, agentTag.Tag)
		if err != nil {
			errorsSlice = append(errorsSlice, err.Error())
		}
	}

	if len(errorsSlice) > 0 {
		message := ""
		for i, errorMessage := range errorsSlice {
			message += fmt.Sprintf("%d. %s\n", i+1, errorMessage)
		}

		ctx.JSON(http.StatusOK, gin.H{"message": message, "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}

type AgentMark struct {
	AgentIdArray []string `json:"agent_id_array"`
	Mark         string   `json:"mark"`
}

func (tc *TsConnector) TcAgentSetMark(ctx *gin.Context) {
	var (
		agentMark AgentMark
		err       error
	)

	err = ctx.ShouldBindJSON(&agentMark)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid command data", "ok": false})
		return
	}

	var errorsSlice []string
	for _, agentId := range agentMark.AgentIdArray {
		err = tc.teamserver.TsAgentSetMark(agentId, agentMark.Mark)
		if err != nil {
			errorsSlice = append(errorsSlice, err.Error())
		}
	}

	if len(errorsSlice) > 0 {
		message := ""
		for i, errorMessage := range errorsSlice {
			message += fmt.Sprintf("%d. %s\n", i+1, errorMessage)
		}

		ctx.JSON(http.StatusOK, gin.H{"message": message, "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}

type AgentColor struct {
	AgentIdArray []string `json:"agent_id_array"`
	Background   string   `json:"bc"`
	Foreground   string   `json:"fc"`
	Reset        bool     `json:"reset"`
}

func (tc *TsConnector) TcAgentSetColor(ctx *gin.Context) {
	var (
		agentColor AgentColor
		err        error
	)

	err = ctx.ShouldBindJSON(&agentColor)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid command data", "ok": false})
		return
	}

	var errorsSlice []string
	for _, agentId := range agentColor.AgentIdArray {
		err = tc.teamserver.TsAgentSetColor(agentId, agentColor.Background, agentColor.Foreground, agentColor.Reset)
		if err != nil {
			errorsSlice = append(errorsSlice, err.Error())
		}
	}

	if len(errorsSlice) > 0 {
		message := ""
		for i, errorMessage := range errorsSlice {
			message += fmt.Sprintf("%d. %s\n", i+1, errorMessage)
		}

		ctx.JSON(http.StatusOK, gin.H{"message": message, "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}

type AgentTaskDelete struct {
	AgentId string   `json:"agent_id"`
	TasksId []string `json:"tasks_array"`
}

func (tc *TsConnector) TcAgentTaskStop(ctx *gin.Context) {
	var (
		agentTasks AgentTaskDelete
		err        error
	)

	err = ctx.ShouldBindJSON(&agentTasks)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid command data", "ok": false})
		return
	}

	var errorsSlice []string
	for _, taskId := range agentTasks.TasksId {
		err = tc.teamserver.TsTaskStop(agentTasks.AgentId, taskId)
		if err != nil {
			errorsSlice = append(errorsSlice, err.Error())
		}
	}

	if len(errorsSlice) > 0 {
		message := ""
		for i, errorMessage := range errorsSlice {
			message += fmt.Sprintf("%d. %s\n", i+1, errorMessage)
		}

		ctx.JSON(http.StatusOK, gin.H{"message": message, "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}

func (tc *TsConnector) TcAgentTaskDelete(ctx *gin.Context) {
	var (
		agentTasks AgentTaskDelete
		err        error
	)

	err = ctx.ShouldBindJSON(&agentTasks)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid command data", "ok": false})
		return
	}

	var errorsSlice []string
	for _, taskId := range agentTasks.TasksId {
		err = tc.teamserver.TsTaskDelete(agentTasks.AgentId, taskId)
		if err != nil {
			errorsSlice = append(errorsSlice, err.Error())
		}
	}

	if len(errorsSlice) > 0 {
		message := ""
		for i, errorMessage := range errorsSlice {
			message += fmt.Sprintf("%d. %s\n", i+1, errorMessage)
		}

		ctx.JSON(http.StatusOK, gin.H{"message": message, "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}
