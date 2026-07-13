package connector

import (
	"fmt"
	"net/http"
	"strconv"

	"github.com/Adaptix-Framework/axc2/v2"
	"github.com/gin-gonic/gin"
)

type AgentTaskDelete struct {
	AgentId int64   `json:"agent_id"`
	TasksId []int64 `json:"tasks_array"`
}

func (tc *TsConnector) TcAgentTaskCancel(ctx *gin.Context) {
	var (
		agentTasks AgentTaskDelete
		err        error
	)

	err = ctx.ShouldBindJSON(&agentTasks)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
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

		respondError(ctx, http.StatusOK, message)
		return
	}

	respondOK(ctx)
}

func (tc *TsConnector) TcAgentTaskDelete(ctx *gin.Context) {
	var (
		agentTasks AgentTaskDelete
		err        error
	)

	err = ctx.ShouldBindJSON(&agentTasks)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
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

		respondError(ctx, http.StatusOK, message)
		return
	}

	respondOK(ctx)
}

type AgentTaskHook struct {
	AgentId     int64  `json:"a_id"`
	TaskId      int64  `json:"a_task_id"`
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
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	username, ok = mustUsername(ctx)
	if !ok {
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
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOK(ctx)
}

type TaskSave struct {
	AgentId     int64  `json:"agent_id"`
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
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	username, ok = mustUsername(ctx)
	if !ok {
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
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOK(ctx)
}

func (tc *TsConnector) TcAgentTaskList(ctx *gin.Context) {
	agentId := int64(0) // empty = all agents
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

	var completedFilter *bool
	if raw := ctx.Query("completed"); raw != "" {
		switch raw {
		case "0", "false", "False", "FALSE":
			v := false
			completedFilter = &v
		case "1", "true", "True", "TRUE":
			v := true
			completedFilter = &v
		default:
			respondError(ctx, http.StatusBadRequest, "completed must be 0, 1, true or false")
			return
		}
	}

	jsonData, err := tc.teamserver.TsTasksGetPage(agentId, offset, limit, filterExpr, sortCol, sortOrder, completedFilter)
	if err != nil {
		respondError(ctx, http.StatusBadRequest, err.Error())
		return
	}

	ctx.Data(http.StatusOK, "application/json; charset=utf-8", jsonData)
}
