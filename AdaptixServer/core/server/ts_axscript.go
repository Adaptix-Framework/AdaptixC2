package server

import (
	"AdaptixServer/core/utils/std"
	"encoding/json"
	"errors"
	"fmt"
	"net"
	"strconv"

	"github.com/Adaptix-Framework/axc2/v2"
)

func (ts *Teamserver) TsAxScriptLoadAgent(agentName string, axScript string, listeners []string) error {
	if ts.ScriptManager == nil {
		return fmt.Errorf("script manager not initialized")
	}
	return ts.ScriptManager.LoadAgentScript(agentName, axScript, listeners)
}

////////////////////

func (ts *Teamserver) AxGetAgentContext(agentId int64) (agentName string, listenerRegName string, osType int, err error) {
	agent, ok := ts.Agents.Get(agentId)
	if !ok {
		return "", "", 0, fmt.Errorf("agent %v not found", agentId)
	}
	data := agent.GetData()
	regName, _ := ts.TsListenerRegByName(data.Listener)
	return data.Name, regName, data.Os, nil
}

func (ts *Teamserver) AxGetAgents() map[string]interface{} {
	result := make(map[string]interface{})

	ts.Agents.ForEachFast(func(key int64, agent *adaptix.Agent) bool {
		data := agent.GetData()
		agentMap := map[string]interface{}{
			"id":           data.Id,
			"type":         data.Name,
			"listener":     data.Listener,
			"external_ip":  data.ExternalIP,
			"internal_ip":  data.InternalIP,
			"domain":       data.Domain,
			"computer":     data.Computer,
			"username":     data.Username,
			"impersonated": data.Impersonated,
			"process":      data.Process,
			"arch":         data.Arch,
			"pid":          data.Pid,
			"tid":          data.Tid,
			"gmt":          data.GmtOffset,
			"acp":          data.ACP,
			"oemcp":        data.OemCP,
			"elevated":     data.Elevated,
			"tags":         data.Tags,
			"async":        data.Async,
			"sleep":        data.Sleep,
			"os_full":      data.OsDesc,
			"os":           osToString(data.Os),
		}
		result[strconv.FormatInt(data.Id, 10)] = agentMap
		return true
	})

	return result
}

func (ts *Teamserver) AxGetAgentInfo(agentId int64, property string) interface{} {
	agent, ok := ts.Agents.Get(agentId)
	if !ok {
		return nil
	}
	data := agent.GetData()

	switch property {
	case "id":
		return data.Id
	case "type":
		return data.Name
	case "listener":
		return data.Listener
	case "external_ip":
		return data.ExternalIP
	case "internal_ip":
		return data.InternalIP
	case "domain":
		return data.Domain
	case "computer":
		return data.Computer
	case "username":
		return data.Username
	case "impersonated":
		return data.Impersonated
	case "process":
		return data.Process
	case "arch":
		return data.Arch
	case "pid":
		return data.Pid
	case "tid":
		return data.Tid
	case "gmt":
		return data.GmtOffset
	case "acp":
		return data.ACP
	case "oemcp":
		return data.OemCP
	case "elevated":
		return data.Elevated
	case "tags":
		return data.Tags
	case "async":
		return data.Async
	case "sleep":
		return data.Sleep
	case "os_full":
		return data.OsDesc
	case "os":
		return osToString(data.Os)
	default:
		return nil
	}
}

// /---
func (ts *Teamserver) AxGetAgentIds() []int64 {
	var ids []int64
	ts.Agents.ForEachFast(func(key int64, _ *adaptix.Agent) bool {
		ids = append(ids, key)
		return true
	})
	return ids
}

// /---
func (ts *Teamserver) AxGetCredentials() []interface{} {
	jsonStr, err := ts.TsCredentilsList()
	if err != nil {
		return []interface{}{}
	}
	var result []interface{}
	_ = json.Unmarshal([]byte(jsonStr), &result)
	if result == nil {
		return []interface{}{}
	}
	return result
}

// /---
func (ts *Teamserver) AxGetTargets() []interface{} {
	jsonStr, err := ts.TsTargetsList()
	if err != nil {
		return []interface{}{}
	}
	var result []interface{}
	_ = json.Unmarshal([]byte(jsonStr), &result)
	if result == nil {
		return []interface{}{}
	}
	return result
}

// /---
func (ts *Teamserver) AxGetPayloads() []interface{} {
	list, _, err := ts.DBMS.DbPayloadGetPage(0, 100000, true, "", "Created", "desc")
	if err != nil || list == nil {
		return []interface{}{}
	}
	out := make([]interface{}, 0, len(list))
	for _, p := range list {
		m := map[string]interface{}{
			"id":          p.PayloadId,
			"name":        p.Name,
			"description": p.Notes,
			"type":        p.AgentType,
			"artifact":    p.Artifact,
			"arch":        p.Arch,
			"listeners":   p.Listeners,
			"size":        p.Size,
			"creator":     p.Creator,
			"created":     p.Created,
			"filename":    p.Filename,
			"md5":         p.Md5,
			"sha1":        p.Sha1,
			"sha256":      p.Sha256,
			"uid":         p.Uid,
			"hidden":      p.Hidden,
		}
		out = append(out, m)
	}
	return out
}

// /---
func (ts *Teamserver) TsAxScriptLoadUser(name string, script string) error {
	if ts.ScriptManager == nil {
		return fmt.Errorf("script manager not initialized")
	}
	return ts.ScriptManager.LoadUserScript(name, script)
}

// /---
func (ts *Teamserver) TsAxScriptUnloadUser(name string) error {
	if ts.ScriptManager == nil {
		return fmt.Errorf("script manager not initialized")
	}
	_ = name
	return fmt.Errorf("user-script unload is currently disabled")
}

// /---
func (ts *Teamserver) TsAxScriptList() (string, error) {
	if ts.ScriptManager == nil {
		return "[]", nil
	}
	scripts := ts.ScriptManager.ListScripts()
	data, err := json.Marshal(scripts)
	if err != nil {
		return "", err
	}
	return string(data), nil
}

// /---
func (ts *Teamserver) TsAxScriptCommands() (string, error) {
	if ts.ScriptManager == nil {
		return "{}", nil
	}
	return ts.ScriptManager.GetCommandsJSON()
}

// /---
func (ts *Teamserver) TsAxScriptParseAndExecute(agentId int64, username string, cmdline string) error {
	_, _, err := ts.TsAxScriptParseAndExecuteResult(agentId, username, cmdline)
	return err
}

func (ts *Teamserver) TsAxScriptParseAndExecuteResult(agentId int64, username string, cmdline string) (int64, bool, error) {
	if ts.ScriptManager == nil {
		return 0, false, fmt.Errorf("script manager not initialized")
	}

	agentName, listenerRegName, agentOs, err := ts.AxGetAgentContext(agentId)
	if err != nil {
		return 0, false, fmt.Errorf("agent not found: %w", err)
	}

	resolved, resolveErr := ts.ScriptManager.CommandStore.ResolveFromCmdline(agentName, listenerRegName, agentOs, cmdline)
	if resolveErr != nil {
		return 0, false, fmt.Errorf("unknown command: %w", resolveErr)
	}

	parsed, parseErr := ts.ScriptManager.ParseCommandPublic(cmdline, resolved)
	if parseErr != nil {
		return 0, false, fmt.Errorf("parse error: %w", parseErr)
	}

	if resolved.Engine != nil {
		if fileErr := ts.ScriptManager.ResolveFileArgsPublic(resolved.Engine, parsed); fileErr != nil {
			return 0, false, fmt.Errorf("file arg error: %w", fileErr)
		}
	}

	cmdDef := resolved.Command
	if resolved.Subcommand != nil {
		cmdDef = resolved.Subcommand
	}

	if cmdDef.HasPreHook && cmdDef.PreHookFunc != nil && resolved.Engine != nil {
		preHookErr := ts.ScriptManager.ExecutePreHookPublic(resolved.Engine, cmdDef.PreHookFunc, agentId, cmdline, parsed.Args, username)
		if preHookErr != nil {
			ts.TsAgentConsoleOutputClient(agentId, username, CONSOLE_OUT_LOCAL_ERROR, cmdline, std.ExtractJsErrorMessage(preHookErr))
			return 0, true, nil
		}
		return 0, true, nil
	}

	hookId := ""
	handlerId := ""

	if cmdDef.HasPostHook && cmdDef.PostHookFunc != nil && resolved.Engine != nil {
		hookId = ts.ScriptManager.HookStore.RegisterPostHook(resolved.Engine, cmdDef.PostHookFunc, agentId, username)
	}
	if cmdDef.HasHandler && cmdDef.HandlerFunc != nil && resolved.Engine != nil {
		handlerId = ts.ScriptManager.HookStore.RegisterHandler(resolved.Engine, cmdDef.HandlerFunc, agentId, username)
	}

	return ts.TsAgentCommandResult(agentName, agentId, username, hookId, handlerId, cmdline, false, parsed.Args)
}

func (ts *Teamserver) TsAxScriptCommandsForAgent(agentId int64) ([]byte, error) {
	if ts.ScriptManager == nil {
		return []byte("[]"), nil
	}
	agentName, listenerRegName, agentOs, err := ts.AxGetAgentContext(agentId)
	if err != nil {
		return nil, err
	}
	groups := ts.ScriptManager.CommandStore.GetCommandsForAgent(agentName, listenerRegName, agentOs)
	data, err := json.Marshal(groups)
	if err != nil {
		return nil, err
	}
	return data, nil
}

func (ts *Teamserver) TsAxScriptResolveHooks(agentName string, agentId int64, listenerRegName string, os int, cmdline string, args map[string]interface{}, client string) (string, string, bool, error) {
	if ts.ScriptManager == nil {
		return "", "", false, nil
	}
	return ts.ScriptManager.ResolveAndExecutePreHook(agentName, agentId, listenerRegName, os, cmdline, args, client)
}

func (ts *Teamserver) TsAxScriptExecPostHook(hookId string, data map[string]interface{}, client string) (map[string]interface{}, error) {
	if ts.ScriptManager == nil {
		return data, nil
	}
	return ts.ScriptManager.HookStore.ExecutePostHook(hookId, data, client)
}

func (ts *Teamserver) TsAxScriptExecHandler(handlerId string, data map[string]interface{}, client string) error {
	if ts.ScriptManager == nil {
		return nil
	}
	return ts.ScriptManager.HookStore.ExecuteHandler(handlerId, data, client)
}

func (ts *Teamserver) TsAxScriptRemovePostHook(hookId string) {
	if ts.ScriptManager == nil {
		return
	}
	ts.ScriptManager.HookStore.RemovePostHook(hookId)
}

// /---
func (ts *Teamserver) TsAxScriptRemoveHandler(handlerId string) {
	if ts.ScriptManager == nil {
		return
	}
	ts.ScriptManager.HookStore.RemoveHandler(handlerId)
}

func (ts *Teamserver) TsAxScriptIsServerHook(id string) bool {
	if ts.ScriptManager == nil {
		return false
	}
	return ts.ScriptManager.HookStore.IsServerHook(id)
}

func (ts *Teamserver) TsPresyncAxScriptData() []interface{} {
	if ts.ScriptManager == nil {
		return nil
	}

	scripts := ts.ScriptManager.ListProfileScriptsWithContent()
	batches := ts.ScriptManager.CommandStore.GetProfileAndUserCommands()

	if len(scripts) == 0 && len(batches) == 0 {
		return nil
	}

	type scriptData struct {
		content string
		groups  []AxCommandBatch
	}
	scriptsMap := make(map[string]*scriptData)

	for _, s := range scripts {
		scriptsMap[s.Name] = &scriptData{
			content: s.Script,
			groups:  []AxCommandBatch{},
		}
	}

	for _, batch := range batches {
		if len(batch.Groups) == 0 {
			continue
		}

		for _, group := range batch.Groups {
			scriptName := group.ScriptName
			if scriptName == "" {
				scriptName = "_unknown_"
			}

			data, err := json.Marshal([]interface{}{group})
			if err != nil {
				ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "axscript", "Presync marshal error for group '%s': %v", group.GroupName, err)
				continue
			}

			entry, exists := scriptsMap[scriptName]
			if !exists {
				entry = &scriptData{
					content: "",
					groups:  []AxCommandBatch{},
				}
				scriptsMap[scriptName] = entry
			}

			entry.groups = append(entry.groups, AxCommandBatch{
				Agent:    batch.Agent,
				Listener: batch.Listener,
				Os:       batch.Os,
				Commands: string(data),
			})
		}
	}

	var packets []interface{}
	for name, data := range scriptsMap {
		packets = append(packets, CreateSpAxScriptData(name, data.content, data.groups))
	}
	return packets
}

// /---
func (ts *Teamserver) TsAxScriptBroadcastData() {
	packets := ts.TsPresyncAxScriptData()
	for _, p := range packets {
		ts.TsSyncAllClients(p)
	}
	ts.TsSyncAllClients(ts.TsPresyncAxScriptList())
}

func (ts *Teamserver) TsPresyncAxScriptList() interface{} {
	items := make([]map[string]interface{}, 0)
	if ts.ScriptManager == nil {
		return CreateSpAxScriptList(items)
	}
	for _, s := range ts.ScriptManager.ListScripts() {
		origin := "profile"
		if s.ScriptType == "agent" {
			origin = "agent"
		} else if s.ScriptType == "user" {
			origin = "user"
		}
		item := map[string]interface{}{
			"name":        s.Name,
			"path":        s.Path,
			"origin":      origin,
			"enabled":     true,
			"description": s.ScriptType,
		}
		if s.Path != "" {
			item["path"] = s.Path
		}
		items = append(items, item)
	}
	return CreateSpAxScriptList(items)
}

func (ts *Teamserver) TsGetAgentCommandGroups(agentName string) []AxCommandBatch {
	if ts.ScriptManager == nil {
		return nil
	}

	batches := ts.ScriptManager.CommandStore.GetAgentCommandBatches(agentName)
	var result []AxCommandBatch

	for _, batch := range batches {
		if len(batch.Groups) == 0 {
			continue
		}

		data, err := json.Marshal(batch.Groups)
		if err != nil {
			ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "axscript", "Marshal error for agent '%s': %v", agentName, err)
			continue
		}

		result = append(result, AxCommandBatch{
			Agent:    batch.Agent,
			Listener: batch.Listener,
			Os:       batch.Os,
			Commands: string(data),
		})
	}

	return result
}

func (ts *Teamserver) AxCredentialsAdd(creds []map[string]interface{}) error {
	return ts.TsCredentilsAdd(creds)
}

// /---
func (ts *Teamserver) AxTargetsAdd(targets []map[string]interface{}) error {
	return ts.TsTargetsAdd(targets)
}

// /---
func (ts *Teamserver) AxAgentRemove(agentIds []int64) error {
	var errs []error
	for _, id := range agentIds {
		if err := ts.TsAgentRemove(id); err != nil {
			errs = append(errs, err)
		}
	}
	return errors.Join(errs...)
}

// /---
func (ts *Teamserver) AxAgentSetTag(agentIds []int64, tag string) error {
	for _, id := range agentIds {
		updateData := map[string]interface{}{"tags": tag}
		_ = ts.TsAgentUpdateDataPartial(id, updateData)
	}
	return nil
}

// /---
func (ts *Teamserver) AxAgentSetMark(agentIds []int64, mark string) error {
	for _, id := range agentIds {
		updateData := map[string]interface{}{"mark": mark}
		_ = ts.TsAgentUpdateDataPartial(id, updateData)
	}
	return nil
}

// /---
func (ts *Teamserver) AxAgentSetColor(agentIds []int64, background string, foreground string, reset bool) error {
	return nil
}

func (ts *Teamserver) AxAgentUpdateData(agentId int64, updateData map[string]interface{}) error {
	return ts.TsAgentUpdateDataPartial(agentId, updateData)
}

func (ts *Teamserver) TsAxScriptLoadFromProfile() {
	if ts.ScriptManager == nil {
		return
	}

	if ts.Profile == nil || ts.Profile.Server == nil {
		return
	}

	for _, scriptPath := range ts.Profile.Server.AxScripts {
		err := ts.ScriptManager.LoadAxScript(scriptPath)
		if err != nil {
			ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "axscript", "Failed to load profile axscript '%s': %v", scriptPath, err)
		}
	}
}

// /---
func (ts *Teamserver) AxGetDownloads() []interface{} {
	jsonStr, err := ts.TsDownloadList()
	if err != nil {
		return []interface{}{}
	}
	var result []interface{}
	_ = json.Unmarshal([]byte(jsonStr), &result)
	if result == nil {
		return []interface{}{}
	}
	return result
}

// /---
func (ts *Teamserver) AxGetScreenshots() []interface{} {
	jsonStr, err := ts.TsScreenshotList()
	if err != nil {
		return []interface{}{}
	}
	var result []interface{}
	_ = json.Unmarshal([]byte(jsonStr), &result)
	if result == nil {
		return []interface{}{}
	}
	return result
}

// /---
func (ts *Teamserver) AxGetTunnels() []interface{} {
	jsonStr, err := ts.TsTunnelList()
	if err != nil {
		return []interface{}{}
	}
	var result []interface{}
	_ = json.Unmarshal([]byte(jsonStr), &result)
	if result == nil {
		return []interface{}{}
	}
	return result
}

// /---
func (ts *Teamserver) AxGetInterfaces() []string {
	var result []string
	ifaces, err := net.Interfaces()
	if err != nil {
		return result
	}
	for _, iface := range ifaces {
		addrs, err := iface.Addrs()
		if err != nil {
			continue
		}
		for _, addr := range addrs {
			var ip net.IP
			switch v := addr.(type) {
			case *net.IPNet:
				ip = v.IP
			case *net.IPAddr:
				ip = v.IP
			}
			if ip != nil && !ip.IsLoopback() {
				result = append(result, ip.String())
			}
		}
	}
	return result
}

// /---
func (ts *Teamserver) AxGetAgentMark(agentId int64) string {
	agent, ok := ts.Agents.Get(agentId)
	if !ok {
		return ""
	}
	data := agent.GetData()
	return data.Mark
}

// /---
func (ts *Teamserver) AxUnloadAxScript(name string) error {
	if ts.ScriptManager == nil {
		return fmt.Errorf("script manager not initialized")
	}
	_ = name
	return fmt.Errorf("user-script unload is currently disabled")
}

func osToString(os int) string {
	switch os {
	case adaptix.OS_WINDOWS:
		return "windows"
	case adaptix.OS_LINUX:
		return "linux"
	case adaptix.OS_MAC:
		return "macos"
	default:
		return "unknown"
	}
}
