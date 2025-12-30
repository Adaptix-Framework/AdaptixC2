package connector

import (
	"fmt"
	"net/http"
	"strconv"

	adaptix "github.com/Adaptix-Framework/axc2"
	"github.com/gin-gonic/gin"
)

type AgentTaskDelete struct {
	AgentId string   `json:"agent_id"`
	TasksId []string `json:"tasks_array"`
}

func (tc *TsConnector) TcAgentTaskCancel(ctx *gin.Context) {
	var (
		agentTasks AgentTaskDelete
		err        error
	)

	err = ctx.ShouldBindJSON(&agentTasks)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
		return
	}

	var errorsSlice []string
	for _, taskId := range agentTasks.TasksId {
		err = tc.teamserver.TsTaskCancel(agentTasks.AgentId, taskId)
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
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
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

type AgentTaskHook struct {
	AgentId     string `json:"a_id"`
	TaskId      string `json:"a_task_id"`
	HookId      string `json:"a_hook_id"`
	JobIndex    int    `json:"a_job_index"`
	MessageType int    `json:"a_msg_type"`
	Message     string `json:"a_message"`
	Text        string `json:"a_text"`
	Completed   bool   `json:"a_completed"`
}

func (tc *TsConnector) TcAgentTaskHook(ctx *gin.Context) {
	var (
		username  string
		tasksHook AgentTaskHook
		err       error
		ok        bool
	)

	err = ctx.ShouldBindJSON(&tasksHook)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
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

	hookData := adaptix.TaskData{
		AgentId:     tasksHook.AgentId,
		TaskId:      tasksHook.TaskId,
		HookId:      tasksHook.HookId,
		Client:      username,
		MessageType: tasksHook.MessageType,
		Message:     tasksHook.Message,
		ClearText:   tasksHook.Text,
		Completed:   tasksHook.Completed,
	}

	err = tc.teamserver.TsTaskPostHook(hookData, tasksHook.JobIndex)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}

type TaskSave struct {
	AgentId     string `json:"agent_id"`
	CommandLine string `json:"command_line"`
	MessageType int    `json:"message_type"`
	Message     string `json:"message"`
	ClearText   string `json:"clear_text"`
}

func (tc *TsConnector) TcAgentTaskSave(ctx *gin.Context) {
	var (
		taskSave TaskSave
		username string
		err      error
		ok       bool
	)

	err = ctx.ShouldBindJSON(&taskSave)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
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

	taskData := adaptix.TaskData{
		AgentId:     taskSave.AgentId,
		CommandLine: taskSave.CommandLine,
		Client:      username,
		MessageType: taskSave.MessageType,
		Message:     taskSave.Message,
		ClearText:   taskSave.ClearText,
	}

	err = tc.teamserver.TsTaskSave(taskData)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}

func (tc *TsConnector) TcAgentTaskList(ctx *gin.Context) {
	agentId := ctx.Query("agent_id")
	if agentId == "" {
		ctx.JSON(http.StatusOK, gin.H{"message": "agent_id is required", "ok": false})
		return
	}

	limit := 200
	offset := 0

	if raw := ctx.Query("limit"); raw != "" {
		value, err := strconv.Atoi(raw)
		if err != nil {
			ctx.JSON(http.StatusOK, gin.H{"message": "limit must be an integer", "ok": false})
			return
		}
		if value < 0 {
			ctx.JSON(http.StatusOK, gin.H{"message": "limit must be >= 0", "ok": false})
			return
		}
		if value > 1000 {
			value = 1000
		}
		limit = value
	}

	if raw := ctx.Query("offset"); raw != "" {
		value, err := strconv.Atoi(raw)
		if err != nil {
			ctx.JSON(http.StatusOK, gin.H{"message": "offset must be an integer", "ok": false})
			return
		}
		if value < 0 {
			ctx.JSON(http.StatusOK, gin.H{"message": "offset must be >= 0", "ok": false})
			return
		}
		offset = value
	}

	jsonTasks, err := tc.teamserver.TsTaskListCompleted(agentId, limit, offset)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.Data(http.StatusOK, "application/json; charset=utf-8", jsonTasks)
}
