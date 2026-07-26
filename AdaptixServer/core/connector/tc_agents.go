package connector

import (
	"AdaptixServer/core/utils/std"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"strconv"

	"github.com/Adaptix-Framework/axc2/v2"
	"github.com/gin-gonic/gin"
)

func (tc *TsConnector) TcAgentList(ctx *gin.Context) {
	jsonAgents, err := tc.teamserver.TsAgentList()
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	ctx.Data(http.StatusOK, "application/json; charset=utf-8", []byte(jsonAgents))
}

type AgentConfig struct {
	ListenerName []string `json:"listener_name"`
	AgentName    string   `json:"agent"`
	Config       string   `json:"config"`
}

func (tc *TsConnector) TcAgentGenerate(ctx *gin.Context) {
	var (
		agentConfig AgentConfig
		err         error
		fileContent []byte
		fileName    string
	)

	err = ctx.ShouldBindJSON(&agentConfig)
	if err != nil {
		_ = ctx.Error(errors.New("invalid agent config"))
		return
	}

	fileContent, fileName, err = tc.teamserver.TsAgentBuildSyncOnce(agentConfig.AgentName, agentConfig.Config, agentConfig.ListenerName)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	encodedContent := base64.StdEncoding.EncodeToString([]byte(fileName)) + ":" + base64.StdEncoding.EncodeToString(fileContent)

	respondOKMessage(ctx, encodedContent)
}

type CommandData struct {
	AgentId    int64  `json:"id"`
	UI         bool   `json:"ui"`
	CmdLine    string `json:"cmdline"`
	Data       string `json:"data"`
	HookId     string `json:"ax_hook_id"`
	HandlerId  string `json:"ax_handler_id"`
	WaitAnswer bool   `json:"wait_answer"`
}

func toInt64(v any) (int64, bool) {
	switch x := v.(type) {
	case int64:
		return x, true
	case float64:
		return int64(x), true
	case json.Number:
		n, err := x.Int64()
		return n, err == nil
	}
	return 0, false
}

func (tc *TsConnector) resolveFileRefs(args map[string]any) error {
	for key, val := range args {
		m, ok := val.(map[string]any)
		if !ok {
			continue
		}
		ref, ok := toInt64(m["__file_ref"])
		if !ok || ref == 0 {
			continue
		}
		data, err := tc.teamserver.TsUploadGetFileContent(ref)
		if err != nil {
			return fmt.Errorf("failed to resolve file ref '%d' for arg '%s': %w", ref, key, err)
		}
		args[key] = base64.StdEncoding.EncodeToString(data)
		if path, ok := m["__file_path"].(string); ok && path != "" {
			args[key+"_path"] = path
		}
	}
	return nil
}

func (tc *TsConnector) dispatchAgentCommand(ctx *gin.Context, username string, commandData *CommandData, args map[string]any) {
	agentName, listenerRegName, agentOs, ctxErr := tc.teamserver.AxGetAgentContext(commandData.AgentId)
	if ctxErr != nil {
		respondError(ctx, http.StatusOK, fmt.Sprintf("agent not found: %v", ctxErr))
		return
	}

	/// Resolve __file_ref markers: read temp files, base64-encode, replace in args
	if err := tc.resolveFileRefs(args); err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	/// Resolve server-side hooks if client did not provide any
	if commandData.HookId == "" && commandData.HandlerId == "" {
		srvHookId, srvHandlerId, preHookHandled, hookErr := tc.teamserver.TsAxScriptResolveHooks(agentName, commandData.AgentId, listenerRegName, agentOs, commandData.CmdLine, args, username)
		if hookErr != nil {
			tc.teamserver.TsAgentConsoleErrorCommand(commandData.AgentId, username, commandData.CmdLine, std.ExtractJsErrorMessage(hookErr), "", "")
			respondOK(ctx)
			return
		}
		if preHookHandled {
			respondOK(ctx)
			return
		}
		commandData.HookId = srvHookId
		commandData.HandlerId = srvHandlerId
	}

	if commandData.WaitAnswer {
		err := tc.teamserver.TsAgentCommand(agentName, commandData.AgentId, username, commandData.HookId, commandData.HandlerId, commandData.CmdLine, commandData.UI, args)
		if err != nil {
			respondError(ctx, http.StatusOK, err.Error())
			return
		}
	} else {
		go func(agentName string, agentId int64, clientName, hookId, handlerId, cmdline string, ui bool, a map[string]any) {
			defer func() {
				if r := recover(); r != nil {
					tc.teamserver.TsLogAdd(adaptix.LogStatusError, 0, "server", "connector", "panic in agent command: %v", r)
				}
			}()
			err := tc.teamserver.TsAgentCommand(agentName, agentId, clientName, hookId, handlerId, cmdline, ui, a)
			if err != nil {
				tc.teamserver.TsAgentConsoleErrorCommand(agentId, clientName, cmdline, err.Error(), hookId, handlerId)
			}
		}(agentName, commandData.AgentId, username, commandData.HookId, commandData.HandlerId, commandData.CmdLine, commandData.UI, args)
	}

	respondOK(ctx)
}

func (tc *TsConnector) TcAgentCommandExecute(ctx *gin.Context) {
	var (
		username    string
		commandData CommandData
		args        map[string]any
		ok          bool
		err         error
	)

	username, ok = mustUsername(ctx)
	if !ok {
		return
	}

	err = ctx.ShouldBindJSON(&commandData)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	err = json.Unmarshal([]byte(commandData.Data), &args)
	if err != nil {
		tc.teamserver.TsLogAdd(adaptix.LogStatusDebug, 0, "server", "connector", "Error parsing commands JSON: %s", err.Error())
		respondError(ctx, http.StatusOK, "invalid command data")
		return
	}

	tc.dispatchAgentCommand(ctx, username, &commandData, args)
}

type CommandData2 struct {
	ObjectId int64 `json:"object_id"`
}

func (tc *TsConnector) TcAgentCommandFile(ctx *gin.Context) {
	var (
		username     string
		commandData  CommandData
		commandData2 CommandData2
		ok           bool
		err          error
	)

	err = ctx.ShouldBindJSON(&commandData2)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	username, ok = mustUsername(ctx)
	if !ok {
		return
	}

	content, err := tc.teamserver.TsUploadGetFileContent(commandData2.ObjectId)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	err = json.Unmarshal(content, &commandData)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	args := make(map[string]any)
	err = json.Unmarshal([]byte(commandData.Data), &args)
	if err != nil {
		tc.teamserver.TsLogAdd(adaptix.LogStatusDebug, 0, "server", "connector", "Error parsing commands JSON: %s", err.Error())
		respondError(ctx, http.StatusOK, "invalid command data")
		return
	}

	tc.dispatchAgentCommand(ctx, username, &commandData, args)
}

type CommandDataRaw struct {
	AgentId int64  `json:"id"`
	CmdLine string `json:"cmdline"`
}

func (tc *TsConnector) TcAgentCommandRaw(ctx *gin.Context) {
	var (
		username string
		rawData  CommandDataRaw
		ok       bool
		err      error
	)

	username, ok = mustUsername(ctx)
	if !ok {
		return
	}

	err = ctx.ShouldBindJSON(&rawData)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	if rawData.AgentId == 0 || rawData.CmdLine == "" {
		respondError(ctx, http.StatusOK, "id and cmdline are required")
		return
	}

	err = tc.teamserver.TsAxScriptParseAndExecute(rawData.AgentId, username, rawData.CmdLine)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOK(ctx)
}

func (tc *TsConnector) TcAgentConsoleList(ctx *gin.Context) {
	raw := ctx.Query("agent_id")
	if raw == "" {
		respondError(ctx, http.StatusBadRequest, "agent_id is required")
		return
	}
	agentId, err := strconv.ParseInt(raw, 10, 64)
	if err != nil {
		respondError(ctx, http.StatusBadRequest, "agent_id must be an integer")
		return
	}

	limit := 200
	var afterId int64
	var aroundId int64
	if q := ctx.Query("limit"); q != "" {
		v, err := strconv.Atoi(q)
		if err != nil || v < 0 {
			respondError(ctx, http.StatusBadRequest, "limit must be a non-negative integer")
			return
		}
		if v > 2000 {
			v = 2000
		}
		limit = v
	}
	if q := ctx.Query("after_id"); q != "" {
		v, err := strconv.ParseInt(q, 10, 64)
		if err != nil || v < 0 {
			respondError(ctx, http.StatusBadRequest, "after_id must be a non-negative integer")
			return
		}
		afterId = v
	}
	if q := ctx.Query("around_id"); q != "" {
		v, err := strconv.ParseInt(q, 10, 64)
		if err != nil || v < 0 {
			respondError(ctx, http.StatusBadRequest, "around_id must be a non-negative integer")
			return
		}
		aroundId = v
	}

	username := ""
	if v, exists := ctx.Get("username"); exists {
		username, _ = v.(string)
	}

	var jsonData []byte
	if aroundId > 0 {
		jsonData, err = tc.teamserver.TsConsoleGetAround(agentId, aroundId, limit, username)
	} else {
		jsonData, err = tc.teamserver.TsConsoleGetPage(agentId, afterId, limit, username)
	}
	if err != nil {
		respondError(ctx, http.StatusBadRequest, err.Error())
		return
	}

	ctx.Data(http.StatusOK, "application/json; charset=utf-8", jsonData)
}

func (tc *TsConnector) TcAgentConsoleSearch(ctx *gin.Context) {
	raw := ctx.Query("agent_id")
	if raw == "" {
		respondError(ctx, http.StatusBadRequest, "agent_id is required")
		return
	}
	agentId, err := strconv.ParseInt(raw, 10, 64)
	if err != nil {
		respondError(ctx, http.StatusBadRequest, "agent_id must be an integer")
		return
	}
	query := ctx.Query("q")
	if query == "" {
		respondError(ctx, http.StatusBadRequest, "q is required")
		return
	}

	limit := 50
	offset := 0
	if q := ctx.Query("limit"); q != "" {
		v, err := strconv.Atoi(q)
		if err != nil || v < 0 {
			respondError(ctx, http.StatusBadRequest, "limit must be a non-negative integer")
			return
		}
		if v > 200 {
			v = 200
		}
		limit = v
	}
	if q := ctx.Query("offset"); q != "" {
		v, err := strconv.Atoi(q)
		if err != nil || v < 0 {
			respondError(ctx, http.StatusBadRequest, "offset must be a non-negative integer")
			return
		}
		offset = v
	}

	username := ""
	if v, exists := ctx.Get("username"); exists {
		username, _ = v.(string)
	}

	jsonData, err := tc.teamserver.TsConsoleSearch(agentId, query, limit, offset, username)
	if err != nil {
		respondError(ctx, http.StatusBadRequest, err.Error())
		return
	}
	ctx.Data(http.StatusOK, "application/json; charset=utf-8", jsonData)
}

type AgentRemove struct {
	AgentIdArray []int64 `json:"agent_id_array"`
}

func (tc *TsConnector) TcAgentConsoleRemove(ctx *gin.Context) {
	var (
		agentRemove AgentRemove
		err         error
	)

	err = ctx.ShouldBindJSON(&agentRemove)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	var errorsSlice []string
	for _, agentId := range agentRemove.AgentIdArray {
		err = tc.teamserver.TsAgentConsoleRemove(agentId)
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

func (tc *TsConnector) TcAgentRemove(ctx *gin.Context) {
	var (
		agentRemove AgentRemove
		err         error
	)

	err = ctx.ShouldBindJSON(&agentRemove)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	_ = tc.teamserver.TsTargetRemoveSessions(agentRemove.AgentIdArray)

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

		respondError(ctx, http.StatusOK, message)
		return
	}

	respondOK(ctx)
}

type AgentTag struct {
	AgentIdArray []int64 `json:"agent_id_array"`
	Tag          string  `json:"tag"`
}

func (tc *TsConnector) TcAgentSetTag(ctx *gin.Context) {
	var (
		agentTag AgentTag
		err      error
	)

	err = ctx.ShouldBindJSON(&agentTag)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	var errorsSlice []string
	for _, agentId := range agentTag.AgentIdArray {
		updateData := struct {
			Tags *string `json:"tags"`
		}{Tags: &agentTag.Tag}
		err = tc.teamserver.TsAgentUpdateDataPartial(agentId, updateData)
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

type AgentMark struct {
	AgentIdArray []int64 `json:"agent_id_array"`
	Mark         string  `json:"mark"`
}

func (tc *TsConnector) TcAgentSetMark(ctx *gin.Context) {
	var (
		agentMark AgentMark
		err       error
	)

	err = ctx.ShouldBindJSON(&agentMark)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	var errorsSlice []string
	for _, agentId := range agentMark.AgentIdArray {
		updateData := struct {
			Mark *string `json:"mark"`
		}{Mark: &agentMark.Mark}
		err = tc.teamserver.TsAgentUpdateDataPartial(agentId, updateData)
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

type AgentColor struct {
	AgentIdArray []int64 `json:"agent_id_array"`
	Background   string  `json:"bc"`
	Foreground   string  `json:"fc"`
	Reset        bool    `json:"reset"`
}

func (tc *TsConnector) TcAgentSetColor(ctx *gin.Context) {
	var (
		agentColor AgentColor
		err        error
	)

	err = ctx.ShouldBindJSON(&agentColor)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	newcolor := ""
	if !agentColor.Reset {
		newcolor = agentColor.Background + "-" + agentColor.Foreground
	}

	var errorsSlice []string
	for _, agentId := range agentColor.AgentIdArray {
		updateData := struct {
			Color *string `json:"color"`
		}{Color: &newcolor}
		err = tc.teamserver.TsAgentUpdateDataPartial(agentId, updateData)
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

type AgentUpdateData struct {
	AgentId      int64   `json:"agent_id"`
	InternalIP   *string `json:"internal_ip,omitempty"`
	ExternalIP   *string `json:"external_ip,omitempty"`
	GmtOffset    *int    `json:"gmt_offset,omitempty"`
	ACP          *int    `json:"acp,omitempty"`
	OemCP        *int    `json:"oemcp,omitempty"`
	Pid          *string `json:"pid,omitempty"`
	Tid          *string `json:"tid,omitempty"`
	Arch         *string `json:"arch,omitempty"`
	Elevated     *bool   `json:"elevated,omitempty"`
	Process      *string `json:"process,omitempty"`
	Os           *int    `json:"os,omitempty"`
	OsDesc       *string `json:"os_desc,omitempty"`
	Domain       *string `json:"domain,omitempty"`
	Computer     *string `json:"computer,omitempty"`
	Username     *string `json:"username,omitempty"`
	Impersonated *string `json:"impersonated,omitempty"`
	Tags         *string `json:"tags,omitempty"`
	Mark         *string `json:"mark,omitempty"`
	Color        *string `json:"color,omitempty"`
}

func (tc *TsConnector) TcAgentUpdateData(ctx *gin.Context) {
	var (
		agentUpdateData AgentUpdateData
		err             error
	)

	err = ctx.ShouldBindJSON(&agentUpdateData)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	if agentUpdateData.AgentId == 0 {
		respondError(ctx, http.StatusOK, "agent_id is required")
		return
	}

	err = tc.teamserver.TsAgentUpdateDataPartial(agentUpdateData.AgentId, agentUpdateData)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	respondOK(ctx)
}
