package connector

import (
	"AdaptixServer/core/utils/logs"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"net/http"

	"github.com/gin-gonic/gin"
)

func (tc *TsConnector) TcAgentList(ctx *gin.Context) {
	jsonAgents, err := tc.teamserver.TsAgentList()
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
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
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	encodedContent := base64.StdEncoding.EncodeToString([]byte(fileName)) + ":" + base64.StdEncoding.EncodeToString(fileContent)

	ctx.JSON(http.StatusOK, gin.H{"message": encodedContent, "ok": true})
}

type CommandData struct {
	AgentName  string `json:"name"`
	AgentId    string `json:"id"`
	UI         bool   `json:"ui"`
	CmdLine    string `json:"cmdline"`
	Data       string `json:"data"`
	HookId     string `json:"ax_hook_id"`
	HandlerId  string `json:"ax_handler_id"`
	WaitAnswer bool   `json:"wait_answer"`
}

func (tc *TsConnector) TcAgentCommandExecute(ctx *gin.Context) {
	var (
		username    string
		commandData CommandData
		args        map[string]any
		ok          bool
		err         error
	)

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

	err = ctx.ShouldBindJSON(&commandData)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
		return
	}

	err = json.Unmarshal([]byte(commandData.Data), &args)
	if err != nil {
		logs.Debug("", "Error parsing commands JSON: %s\n", err.Error())
	}

	if commandData.WaitAnswer {
		err = tc.teamserver.TsAgentCommand(commandData.AgentName, commandData.AgentId, username, commandData.HookId, commandData.HandlerId, commandData.CmdLine, commandData.UI, args)
		if err != nil {
			ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
			return
		}
	} else {
		go func(agentName, agentId, clientName, hookId, handlerId, cmdline string, ui bool, a map[string]any) {
			err := tc.teamserver.TsAgentCommand(agentName, agentId, clientName, hookId, handlerId, cmdline, ui, a)
			if err != nil {
				tc.teamserver.TsAgentConsoleErrorCommand(agentId, clientName, cmdline, err.Error(), hookId, handlerId)
			}
		}(commandData.AgentName, commandData.AgentId, username, commandData.HookId, commandData.HandlerId, commandData.CmdLine, commandData.UI, args)
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}

type CommandData2 struct {
	ObjectId string `json:"object_id"`
}

func (tc *TsConnector) TcAgentCommandFile(ctx *gin.Context) {
	var (
		username     string
		commandData  CommandData
		commandData2 CommandData2
		args         map[string]any
		ok           bool
		err          error
	)

	err = ctx.ShouldBindJSON(&commandData2)
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

	content, err := tc.teamserver.TsUploadGetFileContent(commandData2.ObjectId)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	err = json.Unmarshal(content, &commandData)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	err = json.Unmarshal([]byte(commandData.Data), &args)
	if err != nil {
		logs.Debug("", "Error parsing commands JSON: %s\n", err.Error())
	}

	if commandData.WaitAnswer {
		err = tc.teamserver.TsAgentCommand(commandData.AgentName, commandData.AgentId, username, commandData.HookId, commandData.HandlerId, commandData.CmdLine, commandData.UI, args)
		if err != nil {
			ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
			return
		}
	} else {
		go func(agentName, agentId, clientName, hookId, handlerId, cmdline string, ui bool, a map[string]any) {
			err := tc.teamserver.TsAgentCommand(agentName, agentId, clientName, hookId, handlerId, cmdline, ui, a)
			if err != nil {
				tc.teamserver.TsAgentConsoleErrorCommand(agentId, clientName, cmdline, err.Error(), hookId, handlerId)
			}
		}(commandData.AgentName, commandData.AgentId, username, commandData.HookId, commandData.HandlerId, commandData.CmdLine, commandData.UI, args)
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}

type AgentRemove struct {
	AgentIdArray []string `json:"agent_id_array"`
}

func (tc *TsConnector) TcAgentConsoleRemove(ctx *gin.Context) {
	var (
		agentRemove AgentRemove
		err         error
	)

	err = ctx.ShouldBindJSON(&agentRemove)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
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

		ctx.JSON(http.StatusOK, gin.H{"message": message, "ok": false})
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
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
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
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
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
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
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
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
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

		ctx.JSON(http.StatusOK, gin.H{"message": message, "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}

type AgentUpdateData struct {
	AgentId      string  `json:"agent_id"`
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
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
		return
	}

	if agentUpdateData.AgentId == "" {
		ctx.JSON(http.StatusOK, gin.H{"message": "agent_id is required", "ok": false})
		return
	}

	err = tc.teamserver.TsAgentUpdateDataPartial(agentUpdateData.AgentId, agentUpdateData)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}
