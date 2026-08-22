package axscript

import (
	"encoding/base64"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"

	"github.com/Adaptix-Framework/axc2/v2"
	"github.com/dop251/goja"
)

type TeamserverBridge interface {
	TsLogAdd(status adaptix.LogStatus, level int, source, category string, format string, args ...any)

	TsAgentCommand(agentName string, agentId int64, clientName string, hookId string, handlerId string, cmdline string, ui bool, args map[string]any) error
	TsAgentConsoleOutput(agentId int64, client string, messageType int, message string, clearText string, store bool)
	TsAgentConsoleErrorCommand(agentId int64, client string, cmdline string, message string, HookId string, HandlerId string)

	AxGetAgentContext(agentId int64) (agentName string, listenerRegName string, osType int, err error)
	AxGetAgents() map[string]interface{}
	AxGetAgentInfo(agentId int64, property string) interface{}
	AxGetAgentIds() []int64
	AxGetCredentials() []interface{}
	AxGetTargets() []interface{}
	AxGetPayloads() []interface{}

	AxCredentialsAdd(creds []map[string]interface{}) error
	AxTargetsAdd(targets []map[string]interface{}) error
	AxAgentRemove(agentIds []int64) error
	AxAgentSetTag(agentIds []int64, tag string) error
	AxAgentSetMark(agentIds []int64, mark string) error
	AxAgentSetColor(agentIds []int64, background string, foreground string, reset bool) error
	AxAgentUpdateData(agentId int64, updateData map[string]interface{}) error
	AxAgentSetCommandGroup(agentId int64, groupId string, enabled bool) error
	AxAgentGetCommandGroups(agentId int64) ([]map[string]interface{}, error)

	AxGetDownloads() []interface{}
	AxGetScreenshots() []interface{}

	TsPayloadDownload(id int64) (filename string, content []byte, err error)
	AxGetTunnels() []interface{}
	AxGetInterfaces() []string
	AxGetAgentMark(agentId int64) string
	AxUnloadAxScript(name string) error

	TsEventEmitFrom(eventType string, source string, text string) error
}

type ScriptManager struct {
	mu sync.RWMutex

	teamserver   TeamserverBridge
	CommandStore *CommandStore
	HookStore    *HookStore

	agentEngines       map[string]*ScriptEngine
	userEngines        map[string]*ScriptEngine
	axscriptEngines    map[string]*ScriptEngine
	serviceEngines     map[string]*ScriptEngine
	serviceCommands    map[string]CommandGroup
	scriptInfos        []ScriptInfo
	globalAllowedRoots []string
}

func NewScriptManager(ts TeamserverBridge) *ScriptManager {
	return &ScriptManager{
		teamserver:      ts,
		CommandStore:    NewCommandStore(),
		HookStore:       NewHookStore(),
		agentEngines:    make(map[string]*ScriptEngine),
		userEngines:     make(map[string]*ScriptEngine),
		axscriptEngines: make(map[string]*ScriptEngine),
		serviceEngines:  make(map[string]*ScriptEngine),
		serviceCommands: make(map[string]CommandGroup),
	}
}

/// LOAD SCRIPTS

func (sm *ScriptManager) LoadAgentScript(agentName string, axScript string, listeners []string) error {
	engine := NewScriptEngine("agent:"+agentName, sm)

	registerFormStubs(engine)
	registerMenuStubs(engine)
	registerEventStubs(engine)
	registerAxBridge(engine)

	err := engine.Execute(axScript)
	if err != nil {
		return fmt.Errorf("failed to execute agent script for '%s': %w", agentName, err)
	}

	sm.mu.Lock()
	sm.agentEngines[agentName] = engine
	sm.scriptInfos = append(sm.scriptInfos, ScriptInfo{
		Name:       agentName,
		ScriptType: "agent",
		AgentName:  agentName,
	})
	sm.mu.Unlock()

	for _, listenerType := range listeners {
		sm.executeRegisterCommands(engine, agentName, listenerType)
	}
	if len(listeners) == 0 {
		sm.executeRegisterCommands(engine, agentName, "")
	}
	return nil
}

func (sm *ScriptManager) LoadServiceScript(serviceName, axScript string) error {
	if serviceName == "" {
		return fmt.Errorf("service name is required")
	}
	sm.UnloadServiceScript(serviceName)

	engine := NewScriptEngine("service:"+serviceName, sm)
	registerFormStubs(engine)
	registerMenuStubs(engine)
	registerEventStubs(engine)
	registerAxBridge(engine)

	if err := engine.Execute(axScript); err != nil {
		return fmt.Errorf("failed to execute service script for '%s': %w", serviceName, err)
	}

	sm.mu.Lock()
	sm.serviceEngines[serviceName] = engine
	sm.scriptInfos = append(sm.scriptInfos, ScriptInfo{
		Name:       serviceName,
		ScriptType: "service",
	})
	sm.mu.Unlock()

	sm.executeRegisterServiceCommands(engine, serviceName)
	return nil
}

func (sm *ScriptManager) UnloadServiceScript(serviceName string) {
	sm.mu.Lock()
	delete(sm.serviceEngines, serviceName)
	delete(sm.serviceCommands, serviceName)
	filtered := sm.scriptInfos[:0]
	for _, info := range sm.scriptInfos {
		if !(info.ScriptType == "service" && info.Name == serviceName) {
			filtered = append(filtered, info)
		}
	}
	sm.scriptInfos = filtered
	sm.mu.Unlock()
}

func (sm *ScriptManager) RegisterServiceCommandGroup(serviceName string, group CommandGroup) {
	if serviceName == "" {
		return
	}
	if group.GroupName == "" {
		group.GroupName = serviceName
	}
	group.ScriptName = serviceName
	group.Source = "service"
	sm.mu.Lock()
	sm.serviceCommands[serviceName] = group
	sm.mu.Unlock()
}

func (sm *ScriptManager) ServiceCommandGroup(serviceName string) (CommandGroup, bool) {
	sm.mu.RLock()
	defer sm.mu.RUnlock()
	g, ok := sm.serviceCommands[serviceName]
	return g, ok
}

func (sm *ScriptManager) executeRegisterServiceCommands(engine *ScriptEngine, serviceName string) {
	engine.mu.Lock()
	rt := engine.runtime
	fn := rt.Get("RegisterServiceCommands")
	if fn == nil || goja.IsUndefined(fn) {
		engine.mu.Unlock()
		return
	}
	registerFn, ok := goja.AssertFunction(fn)
	if !ok {
		engine.mu.Unlock()
		sm.teamserver.TsLogAdd(adaptix.LogStatusError, 0, "axscript_manager", "log", "RegisterServiceCommands is not a function in script for service '%s'", serviceName)
		return
	}
	_, err := registerFn(goja.Undefined())
	engine.mu.Unlock()
	if err != nil {
		sm.teamserver.TsLogAdd(adaptix.LogStatusError, 0, "axscript_manager", "log", "Error calling RegisterServiceCommands for service '%s': %v", serviceName, err)
	}
}

func (sm *ScriptManager) LoadAxScript(scriptPath string) error {
	abs, err := filepath.Abs(scriptPath)
	if err != nil {
		return fmt.Errorf("invalid script path '%s': %w", scriptPath, err)
	}

	_, err = os.Stat(abs)
	if err != nil {
		return fmt.Errorf("script file not found: %s", abs)
	}

	content, err := os.ReadFile(abs)
	if err != nil {
		return fmt.Errorf("failed to read script '%s': %w", abs, err)
	}

	engine, err := NewScriptEngineFromPath(abs, sm)
	if err != nil {
		return fmt.Errorf("failed to create engine for '%s': %w", abs, err)
	}

	registerFormStubs(engine)
	registerMenuStubs(engine)
	registerEventStubs(engine)
	registerAxBridge(engine)

	err = engine.Execute(string(content))
	if err != nil {
		return fmt.Errorf("failed to execute script '%s': %w", abs, err)
	}

	if engine.GetMetadataNoSave() {
		//sm.teamserver.TsLogAdd(adaptix.LogStatusSuccess, 0, "server:axscript_manager", "Executed axscript '%s' (nosave)", scriptPath)
		return nil
	}

	scriptName := engine.GetMetadataName()
	if scriptName == "" {
		scriptName = filepath.Base(abs)
	}

	sm.mu.Lock()
	sm.axscriptEngines[abs] = engine
	sm.scriptInfos = append(sm.scriptInfos, ScriptInfo{
		Name:       scriptName,
		ScriptType: "axscript",
		Path:       abs,
	})
	sm.mu.Unlock()

	sm.teamserver.TsLogAdd(adaptix.LogStatusSuccess, 0, "axscript_manager", "script", "Loaded '%s'", scriptPath)
	return nil
}

func (sm *ScriptManager) LoadAxScriptChild(parentEngine *ScriptEngine, scriptPath string) error {
	abs, err := filepath.Abs(scriptPath)
	if err != nil {
		return fmt.Errorf("invalid script path '%s': %w", scriptPath, err)
	}

	if _, err := os.Stat(abs); err != nil {
		return fmt.Errorf("script file not found: %s", abs)
	}

	content, err := os.ReadFile(abs)
	if err != nil {
		return fmt.Errorf("failed to read script '%s': %w", abs, err)
	}

	engine, err := NewScriptEngineFromPath(abs, sm)
	if err != nil {
		return fmt.Errorf("failed to create engine for '%s': %w", abs, err)
	}

	for _, root := range parentEngine.allowedRoots {
		found := false
		for _, r := range engine.allowedRoots {
			if r == root {
				found = true
				break
			}
		}
		if !found {
			engine.allowedRoots = append(engine.allowedRoots, root)
		}
	}

	registerFormStubs(engine)
	registerMenuStubs(engine)
	registerEventStubs(engine)
	registerAxBridge(engine)

	err = engine.Execute(string(content))
	if err != nil {
		return fmt.Errorf("failed to execute script '%s': %w", abs, err)
	}

	if engine.GetMetadataNoSave() {
		//sm.teamserver.TsLogAdd(adaptix.LogStatusSuccess, 0, "server:axscript_manager", "Executed axscript '%s' (nosave)", abs)
		return nil
	}

	scriptName := engine.GetMetadataName()
	if scriptName == "" {
		scriptName = filepath.Base(abs)
	}

	sm.mu.Lock()
	sm.axscriptEngines[abs] = engine
	sm.scriptInfos = append(sm.scriptInfos, ScriptInfo{
		Name:       scriptName,
		ScriptType: "axscript",
		Path:       abs,
	})
	sm.mu.Unlock()

	sm.teamserver.TsLogAdd(adaptix.LogStatusSuccess, 0, "axscript_manager", "script", "Loaded '%s'", abs)
	return nil
}

func (sm *ScriptManager) ImportAxScript(engine *ScriptEngine, scriptPath string) (string, error) {
	abs, err := filepath.Abs(scriptPath)
	if err != nil {
		return "", fmt.Errorf("invalid script path '%s': %w", scriptPath, err)
	}

	_, err = engine.ValidatePath(abs)
	if err != nil {
		return "", err
	}

	content, err := os.ReadFile(abs)
	if err != nil {
		return "", fmt.Errorf("failed to read script '%s': %w", abs, err)
	}

	_, execErr := engine.runtime.RunString(string(content))
	if execErr != nil {
		return "", fmt.Errorf("failed to import script '%s': %w", abs, execErr)
	}

	return abs, nil
}

///

func (sm *ScriptManager) executeRegisterCommands(engine *ScriptEngine, agentName string, listenerType string) {
	engine.mu.Lock()
	rt := engine.runtime

	fn := rt.Get("RegisterCommands")
	if fn == nil || goja.IsUndefined(fn) {
		engine.mu.Unlock()
		sm.teamserver.TsLogAdd(adaptix.LogStatusWarn, 0, "axscript_manager", "log", "No RegisterCommands function found in script for '%s'", agentName)
		return
	}

	registerFn, ok := goja.AssertFunction(fn)
	if !ok {
		engine.mu.Unlock()
		sm.teamserver.TsLogAdd(adaptix.LogStatusError, 0, "axscript_manager", "log", "RegisterCommands is not a function in script for '%s'", agentName)
		return
	}

	result, err := registerFn(goja.Undefined(), rt.ToValue(listenerType))
	engine.mu.Unlock()
	if err != nil {
		sm.teamserver.TsLogAdd(adaptix.LogStatusError, 0, "axscript_manager", "log", "Error calling RegisterCommands for '%s': %v", agentName, err)
		return
	}
	if result == nil || goja.IsUndefined(result) || goja.IsNull(result) {
		sm.teamserver.TsLogAdd(adaptix.LogStatusWarn, 0, "axscript_manager", "log", "RegisterCommands returned nil for '%s'", agentName)
		return
	}

	obj := result.ToObject(rt)
	sm.extractCommandGroupsArray(engine, agentName, listenerType, obj, "command_groups_windows", OsWindows)
	sm.extractCommandGroupsArray(engine, agentName, listenerType, obj, "command_groups_linux", OsLinux)
	sm.extractCommandGroupsArray(engine, agentName, listenerType, obj, "command_groups_macos", OsMac)
	sm.extractCommandsFromResult(engine, agentName, listenerType, obj, "commands_windows", OsWindows)
	sm.extractCommandsFromResult(engine, agentName, listenerType, obj, "commands_linux", OsLinux)
	sm.extractCommandsFromResult(engine, agentName, listenerType, obj, "commands_macos", OsMac)
}

func (sm *ScriptManager) extractCommandsFromResult(engine *ScriptEngine, agentName string, listenerType string, obj *goja.Object, propName string, osType int) {
	prop := obj.Get(propName)
	if prop == nil || goja.IsUndefined(prop) || goja.IsNull(prop) {
		return
	}

	var groupBuilder *jsCommandGroupBuilder

	exported := prop.Export()
	if gb, ok := exported.(*jsCommandGroupBuilder); ok {
		groupBuilder = gb
	}

	if groupBuilder == nil {
		if m, ok := exported.(map[string]interface{}); ok {
			if gv, exists := m["__group"]; exists {
				if gb, ok2 := gv.(*jsCommandGroupBuilder); ok2 {
					groupBuilder = gb
				}
			}
		}
	}

	if groupBuilder == nil {
		propObj := prop.ToObject(engine.runtime)
		groupVal := propObj.Get("__group")
		if groupVal != nil && !goja.IsUndefined(groupVal) && !goja.IsNull(groupVal) {
			if gb, ok := groupVal.Export().(*jsCommandGroupBuilder); ok {
				groupBuilder = gb
			}
		}
	}

	if groupBuilder == nil {
		sm.teamserver.TsLogAdd(adaptix.LogStatusWarn, 0, "axscript_manager", "log", "Property '%s' for agent '%s' is not a CommandGroup", propName, agentName)
		return
	}

	group := groupBuilder.ToCommandGroup(agentName)
	group.Source = "agent"
	sm.CommandStore.RegisterGroups(SourceAgent, agentName, listenerType, osType, []CommandGroup{group}, engine)
}

func (sm *ScriptManager) extractGroupBuilder(engine *ScriptEngine, prop goja.Value) *jsCommandGroupBuilder {
	if prop == nil || goja.IsUndefined(prop) || goja.IsNull(prop) {
		return nil
	}
	exported := prop.Export()
	if gb, ok := exported.(*jsCommandGroupBuilder); ok {
		return gb
	}
	if m, ok := exported.(map[string]interface{}); ok {
		if gv, exists := m["__group"]; exists {
			if gb, ok2 := gv.(*jsCommandGroupBuilder); ok2 {
				return gb
			}
		}
	}
	propObj := prop.ToObject(engine.runtime)
	if propObj == nil {
		return nil
	}
	groupVal := propObj.Get("__group")
	if groupVal != nil && !goja.IsUndefined(groupVal) && !goja.IsNull(groupVal) {
		if gb, ok := groupVal.Export().(*jsCommandGroupBuilder); ok {
			return gb
		}
	}
	return nil
}

func (sm *ScriptManager) extractCommandGroupsArray(engine *ScriptEngine, agentName string, listenerType string, obj *goja.Object, propName string, osType int) {
	prop := obj.Get(propName)
	if prop == nil || goja.IsUndefined(prop) || goja.IsNull(prop) {
		return
	}

	rt := engine.runtime
	propObj := prop.ToObject(rt)
	if propObj == nil {
		return
	}

	isArr, _ := isJsArray(rt, propObj)
	if !isArr {
		if gb := sm.extractGroupBuilder(engine, prop); gb != nil {
			group := gb.ToCommandGroup(agentName)
			group.Source = "agent"
			sm.CommandStore.RegisterGroups(SourceAgent, agentName, listenerType, osType, []CommandGroup{group}, engine)
		}
		return
	}

	length := int(propObj.Get("length").ToInteger())
	var groups []CommandGroup
	for i := 0; i < length; i++ {
		item := propObj.Get(fmt.Sprintf("%d", i))
		gb := sm.extractGroupBuilder(engine, item)
		if gb == nil {
			sm.teamserver.TsLogAdd(adaptix.LogStatusWarn, 0, "axscript_manager", "log",
				"command group item %d in '%s' for agent '%s' is not a CommandGroup", i, propName, agentName)
			continue
		}
		group := gb.ToCommandGroup(agentName)
		group.Source = "agent"
		groups = append(groups, group)
	}
	if len(groups) > 0 {
		sm.CommandStore.RegisterGroups(SourceAgent, agentName, listenerType, osType, groups, engine)
	}
}

////////////////////

// /---
func (sm *ScriptManager) LoadUserScript(name string, script string) error {
	engine := NewScriptEngine("user:"+name, sm)

	registerFormStubs(engine)
	registerMenuStubs(engine)
	registerEventStubs(engine)
	registerAxBridge(engine)

	err := engine.Execute(script)
	if err != nil {
		return fmt.Errorf("failed to execute user script '%s': %w", name, err)
	}

	sm.mu.Lock()
	sm.userEngines[name] = engine
	sm.scriptInfos = append(sm.scriptInfos, ScriptInfo{
		Name:       name,
		ScriptType: "user",
	})
	sm.mu.Unlock()

	return nil
}

// /---
func (sm *ScriptManager) UnloadUserScript(name string) error {
	sm.mu.Lock()
	defer sm.mu.Unlock()

	if _, ok := sm.userEngines[name]; !ok {
		return fmt.Errorf("user script '%s' not found", name)
	}

	delete(sm.userEngines, name)

	sm.CommandStore.UnregisterByScriptName(SourceUser, name)

	for i, info := range sm.scriptInfos {
		if info.Name == name && info.ScriptType == "user" {
			sm.scriptInfos = append(sm.scriptInfos[:i], sm.scriptInfos[i+1:]...)
			break
		}
	}

	return nil
}

// /---
func (sm *ScriptManager) ListScripts() []ScriptInfo {
	sm.mu.RLock()
	defer sm.mu.RUnlock()

	result := make([]ScriptInfo, len(sm.scriptInfos))
	copy(result, sm.scriptInfos)
	return result
}

func (sm *ScriptManager) ListProfileScriptsWithContent() []ScriptWithContent {
	sm.mu.RLock()
	defer sm.mu.RUnlock()

	var result []ScriptWithContent
	for _, info := range sm.scriptInfos {
		if info.ScriptType == "axscript" && info.Path != "" {
			engine, ok := sm.axscriptEngines[info.Path]
			if !ok {
				continue
			}

			var combined strings.Builder

			importedFiles := engine.GetImportedFiles()
			for _, importPath := range importedFiles {
				importContent, err := os.ReadFile(importPath)
				if err != nil {
					combined.WriteString(fmt.Sprintf("/* import error: %s */\n", importPath))
					continue
				}
				combined.WriteString(fmt.Sprintf("/* inlined: %s */\n", filepath.Base(importPath)))
				combined.Write(importContent)
				combined.WriteString(fmt.Sprintf("\n/* end: %s */\n\n", filepath.Base(importPath)))
			}

			mainContent, err := os.ReadFile(info.Path)
			if err != nil {
				continue
			}
			combined.Write(mainContent)

			result = append(result, ScriptWithContent{
				Name:   info.Name,
				Script: combined.String(),
			})
		}
	}
	return result
}

func (sm *ScriptManager) ResolveAndExecutePreHook(agentName string, agentId int64, listenerRegName string, os int, cmdline string, args map[string]interface{}, client string) (hookId string, handlerId string, preHookHandled bool, err error) {
	resolved, resolveErr := sm.CommandStore.ResolveFromCmdline(agentName, listenerRegName, os, cmdline)
	if resolveErr != nil {
		return "", "", false, nil
	}

	cmdDef := resolved.GetEffectiveCommand()

	if cmdDef.HasPreHook && cmdDef.PreHookFunc != nil && resolved.Engine != nil {
		preHookErr := sm.executePreHook(resolved.Engine, cmdDef.PreHookFunc, agentId, cmdline, args, client)
		if preHookErr != nil {
			return "", "", true, preHookErr
		}
		return "", "", true, nil
	}

	if cmdDef.HasPostHook && cmdDef.PostHookFunc != nil && resolved.Engine != nil {
		hookId = sm.HookStore.RegisterPostHook(resolved.Engine, cmdDef.PostHookFunc, agentId, client)
	}

	if cmdDef.HasHandler && cmdDef.HandlerFunc != nil && resolved.Engine != nil {
		handlerId = sm.HookStore.RegisterHandler(resolved.Engine, cmdDef.HandlerFunc, agentId, client)
	}

	return hookId, handlerId, false, nil
}

func (sm *ScriptManager) ParseCommandPublic(cmdline string, resolved *ResolvedCommand) (*ParsedCommand, error) {
	return ParseCommand(cmdline, resolved)
}

// /---
func (sm *ScriptManager) ExecutePreHookPublic(engine *ScriptEngine, fn goja.Callable, agentId int64, cmdline string, args map[string]interface{}, client string) error {
	return sm.executePreHook(engine, fn, agentId, cmdline, args, client)
}

// /---
func (sm *ScriptManager) ResolveFileArgsPublic(engine *ScriptEngine, parsed *ParsedCommand) error {
	return sm.resolveFileArgs(engine, parsed)
}

func (sm *ScriptManager) executePreHook(engine *ScriptEngine, fn goja.Callable, agentId int64, cmdline string, args map[string]interface{}, client string) error {
	argsCopy := make(map[string]interface{}, len(args))
	for k, v := range args {
		argsCopy[k] = v
	}

	_, err := engine.CallCallableAs(client, fn,
		engine.ToValue(agentId),
		engine.ToValue(cmdline),
		engine.ToValue(argsCopy),
	)
	return err
}

func parsePayloadRef(path string) (int64, bool) {
	s := strings.TrimSpace(path)
	lower := strings.ToLower(s)
	const prefix = "__payload:#"
	if !strings.HasPrefix(lower, prefix) {
		return 0, false
	}
	idStr := strings.TrimSpace(s[len(prefix):])
	// Allow "__payload:#12 (name.bin)" copied from UI / tool notes.
	if i := strings.IndexAny(idStr, " \t("); i >= 0 {
		idStr = strings.TrimSpace(idStr[:i])
	}
	id, err := strconv.ParseInt(idStr, 10, 64)
	if err != nil || id <= 0 {
		return 0, false
	}
	return id, true
}

func (sm *ScriptManager) resolveFileArgs(engine *ScriptEngine, parsed *ParsedCommand) error {
	if len(parsed.FileArgs) == 0 {
		return nil
	}
	for _, fa := range parsed.FileArgs {
		if fa.OriginalPath == "" {
			if fa.Required {
				return fmt.Errorf("missing required file argument: %s", fa.ArgName)
			}
			continue
		}

		// Payload Store: __payload:#id
		if pid, ok := parsePayloadRef(fa.OriginalPath); ok {
			if sm.teamserver == nil {
				return fmt.Errorf("cannot resolve payload for argument '%s': teamserver not available", fa.ArgName)
			}
			filename, data, err := sm.teamserver.TsPayloadDownload(pid)
			if err != nil {
				return fmt.Errorf("cannot resolve payload #%d for argument '%s': %w", pid, fa.ArgName, err)
			}
			parsed.Args[fa.ArgName] = base64.StdEncoding.EncodeToString(data)
			if filename != "" {
				parsed.Args[fa.ArgName+"_path"] = fmt.Sprintf("__payload:#%d (%s)", pid, filename)
			} else {
				parsed.Args[fa.ArgName+"_path"] = fa.OriginalPath
			}
			continue
		}

		data, err := sm.ReadFileSandboxed(engine, fa.OriginalPath)
		if err != nil {
			return fmt.Errorf("cannot read file for argument '%s': %w", fa.ArgName, err)
		}
		parsed.Args[fa.ArgName] = base64.StdEncoding.EncodeToString(data)
		parsed.Args[fa.ArgName+"_path"] = fa.OriginalPath
	}
	return nil
}

type agentCommandContext struct {
	agentName string
	resolved  *ResolvedCommand
	parsed    *ParsedCommand
}

func (sm *ScriptManager) resolveAgentCommand(agentId int64, cmdline string) (*agentCommandContext, error) {
	if sm.teamserver == nil {
		return nil, fmt.Errorf("teamserver not available")
	}

	agentName, listenerRegName, os, err := sm.teamserver.AxGetAgentContext(agentId)
	if err != nil {
		return nil, err
	}

	resolved, resolveErr := sm.CommandStore.ResolveFromCmdline(agentName, listenerRegName, os, cmdline)
	if resolveErr != nil {
		return nil, resolveErr
	}

	parsed, parseErr := ParseCommand(cmdline, resolved)
	if parseErr != nil {
		return nil, parseErr
	}

	return &agentCommandContext{
		agentName: agentName,
		resolved:  resolved,
		parsed:    parsed,
	}, nil
}

// /---
func (sm *ScriptManager) ExecuteCommand(fromEngine *ScriptEngine, agentId int64, cmdline string, clientName string, postHookFn goja.Callable, handlerFn goja.Callable) error {
	ctx, err := sm.resolveAgentCommand(agentId, cmdline)
	if err != nil {
		return err
	}

	if fromEngine != nil {
		if fileErr := sm.resolveFileArgs(fromEngine, ctx.parsed); fileErr != nil {
			return fileErr
		}
	}

	hookId := ""
	handlerId := ""

	if postHookFn != nil {
		hookId = sm.HookStore.RegisterPostHook(fromEngine, postHookFn, agentId, clientName)
	}
	if handlerFn != nil {
		handlerId = sm.HookStore.RegisterHandler(fromEngine, handlerFn, agentId, clientName)
	}

	return sm.teamserver.TsAgentCommand(ctx.agentName, agentId, clientName, hookId, handlerId, cmdline, false, ctx.parsed.Args)
}

// /---
func (sm *ScriptManager) ExecuteAlias(fromEngine *ScriptEngine, agentId int64, aliasCmdline string, clientName string) error {
	ctx, err := sm.resolveAgentCommand(agentId, aliasCmdline)
	if err != nil {
		return err
	}

	if fromEngine != nil {
		if fileErr := sm.resolveFileArgs(fromEngine, ctx.parsed); fileErr != nil {
			return fileErr
		}
	}

	cmdDef := ctx.resolved.GetEffectiveCommand()

	if cmdDef.HasPreHook && cmdDef.PreHookFunc != nil && ctx.resolved.Engine != nil {
		preHookErr := sm.executePreHook(ctx.resolved.Engine, cmdDef.PreHookFunc, agentId, aliasCmdline, ctx.parsed.Args, clientName)
		if preHookErr != nil {
			return preHookErr
		}
		return nil
	}

	hookId := ""
	handlerId := ""

	if cmdDef.HasPostHook && cmdDef.PostHookFunc != nil && ctx.resolved.Engine != nil {
		hookId = sm.HookStore.RegisterPostHook(ctx.resolved.Engine, cmdDef.PostHookFunc, agentId, clientName)
	}
	if cmdDef.HasHandler && cmdDef.HandlerFunc != nil && ctx.resolved.Engine != nil {
		handlerId = sm.HookStore.RegisterHandler(ctx.resolved.Engine, cmdDef.HandlerFunc, agentId, clientName)
	}

	return sm.teamserver.TsAgentCommand(ctx.agentName, agentId, clientName, hookId, handlerId, aliasCmdline, false, ctx.parsed.Args)
}

func (sm *ScriptManager) ExecuteAliasWithHooks(fromEngine *ScriptEngine, agentId int64, displayCmdline string, aliasCmdline string, message string, clientName string, postHookFn goja.Callable, handlerFn goja.Callable) error {
	ctx, err := sm.resolveAgentCommand(agentId, aliasCmdline)
	if err != nil {
		return err
	}

	if fromEngine != nil {
		if fileErr := sm.resolveFileArgs(fromEngine, ctx.parsed); fileErr != nil {
			return fileErr
		}
	}

	cmdDef := ctx.resolved.GetEffectiveCommand()

	if cmdDef.HasPreHook && cmdDef.PreHookFunc != nil && ctx.resolved.Engine != nil {
		preHookErr := sm.executePreHook(ctx.resolved.Engine, cmdDef.PreHookFunc, agentId, aliasCmdline, ctx.parsed.Args, clientName)
		if preHookErr != nil {
			return preHookErr
		}
		return nil
	}

	hookId := ""
	handlerId := ""

	if postHookFn != nil {
		hookId = sm.HookStore.RegisterPostHook(fromEngine, postHookFn, agentId, clientName)
	} else if cmdDef.HasPostHook && cmdDef.PostHookFunc != nil && ctx.resolved.Engine != nil {
		hookId = sm.HookStore.RegisterPostHook(ctx.resolved.Engine, cmdDef.PostHookFunc, agentId, clientName)
	}

	if handlerFn != nil {
		handlerId = sm.HookStore.RegisterHandler(fromEngine, handlerFn, agentId, clientName)
	} else if cmdDef.HasHandler && cmdDef.HandlerFunc != nil && ctx.resolved.Engine != nil {
		handlerId = sm.HookStore.RegisterHandler(ctx.resolved.Engine, cmdDef.HandlerFunc, agentId, clientName)
	}

	cmdlineForDisplay := displayCmdline
	if cmdlineForDisplay == "" {
		cmdlineForDisplay = aliasCmdline
	}
	if message != "" {
		ctx.parsed.Args["message"] = message
	}

	return sm.teamserver.TsAgentCommand(ctx.agentName, agentId, clientName, hookId, handlerId, cmdlineForDisplay, false, ctx.parsed.Args)
}

func (sm *ScriptManager) GetAgents() map[string]interface{} {
	if sm.teamserver == nil {
		return map[string]interface{}{}
	}
	return sm.teamserver.AxGetAgents()
}

func (sm *ScriptManager) GetAgentInfo(agentId int64, property string) interface{} {
	if sm.teamserver == nil {
		return nil
	}
	return sm.teamserver.AxGetAgentInfo(agentId, property)
}

// /---
func (sm *ScriptManager) GetAgentIds() []int64 {
	if sm.teamserver == nil {
		return []int64{}
	}
	return sm.teamserver.AxGetAgentIds()
}

// /---
func (sm *ScriptManager) GetCredentials() []interface{} {
	if sm.teamserver == nil {
		return []interface{}{}
	}
	return sm.teamserver.AxGetCredentials()
}

// /---
func (sm *ScriptManager) GetTargets() []interface{} {
	if sm.teamserver == nil {
		return []interface{}{}
	}
	return sm.teamserver.AxGetTargets()
}

// /---
func (sm *ScriptManager) GetPayloads() []interface{} {
	if sm.teamserver == nil {
		return []interface{}{}
	}
	return sm.teamserver.AxGetPayloads()
}

func (sm *ScriptManager) ConsoleMessage(agentId int64, client string, msgType int, message string, clearText string) {
	if sm.teamserver == nil {
		return
	}
	sm.teamserver.TsAgentConsoleOutput(agentId, client, msgType, message, clearText, false)
}

// /---
func (sm *ScriptManager) GetCommandsJSON() (string, error) {
	allCommands := sm.CommandStore.GetAllCommandsOrdered()
	data, err := json.Marshal(allCommands)
	if err != nil {
		return "", err
	}
	return string(data), nil
}

// /---
func (sm *ScriptManager) SetGlobalAllowedRoots(roots []string) {
	sm.mu.Lock()
	defer sm.mu.Unlock()
	out := make([]string, 0, len(roots))
	seen := map[string]bool{}
	for _, r := range roots {
		r = strings.TrimSpace(r)
		if r == "" {
			continue
		}
		abs, err := filepath.Abs(r)
		if err != nil {
			continue
		}
		if seen[abs] {
			continue
		}
		seen[abs] = true
		out = append(out, abs)
	}
	sm.globalAllowedRoots = out
}

func (sm *ScriptManager) AddGlobalAllowedRoot(root string) {
	sm.mu.Lock()
	defer sm.mu.Unlock()
	root = strings.TrimSpace(root)
	if root == "" {
		return
	}
	abs, err := filepath.Abs(root)
	if err != nil {
		return
	}
	for _, r := range sm.globalAllowedRoots {
		if r == abs {
			return
		}
	}
	sm.globalAllowedRoots = append(sm.globalAllowedRoots, abs)
	add := func(engines map[string]*ScriptEngine) {
		for _, e := range engines {
			if e == nil {
				continue
			}
			e.allowedRoots = append(e.allowedRoots, abs)
		}
	}
	add(sm.agentEngines)
	add(sm.userEngines)
	add(sm.axscriptEngines)
	add(sm.serviceEngines)
}

func (sm *ScriptManager) ReadFileSandboxed(engine *ScriptEngine, path string) ([]byte, error) {
	validated, err := engine.ValidatePath(path)
	if err != nil {
		return nil, err
	}
	return os.ReadFile(validated)
}

// /---
func (sm *ScriptManager) WriteFileSandboxed(engine *ScriptEngine, path string, data []byte, append_ bool) error {
	validated, err := engine.ValidatePath(path)
	if err != nil {
		return err
	}
	flag := os.O_WRONLY | os.O_CREATE
	if append_ {
		flag |= os.O_APPEND
	} else {
		flag |= os.O_TRUNC
	}
	dir := filepath.Dir(validated)
	if err := os.MkdirAll(dir, 0755); err != nil {
		return err
	}
	f, err := os.OpenFile(validated, flag, 0644)
	if err != nil {
		return err
	}
	defer f.Close()
	_, err = f.Write(data)
	return err
}

// /---
func (sm *ScriptManager) GetDownloads() []interface{} {
	if sm.teamserver == nil {
		return []interface{}{}
	}
	return sm.teamserver.AxGetDownloads()
}

// /---
func (sm *ScriptManager) GetScreenshots() []interface{} {
	if sm.teamserver == nil {
		return []interface{}{}
	}
	return sm.teamserver.AxGetScreenshots()
}

// /---
func (sm *ScriptManager) GetTunnels() []interface{} {
	if sm.teamserver == nil {
		return []interface{}{}
	}
	return sm.teamserver.AxGetTunnels()
}

// /---
func (sm *ScriptManager) GetInterfaces() []string {
	if sm.teamserver == nil {
		return []string{}
	}
	return sm.teamserver.AxGetInterfaces()
}

// /---
func (sm *ScriptManager) GetAgentMark(agentId int64) string {
	if sm.teamserver == nil {
		return ""
	}
	return sm.teamserver.AxGetAgentMark(agentId)
}

// /---
func (sm *ScriptManager) UnloadAxScript(name string) error {
	if sm.teamserver == nil {
		return fmt.Errorf("teamserver not available")
	}
	return sm.teamserver.AxUnloadAxScript(name)
}

// /---
func (sm *ScriptManager) ValidateCommand(agentId int64, cmdline string) (map[string]interface{}, error) {
	if sm.teamserver == nil {
		return nil, fmt.Errorf("teamserver not available")
	}

	agentName, listenerRegName, osType, err := sm.teamserver.AxGetAgentContext(agentId)
	if err != nil {
		return map[string]interface{}{"valid": false, "message": "Agent not found"}, nil
	}

	resolved, resolveErr := sm.CommandStore.ResolveFromCmdline(agentName, listenerRegName, osType, cmdline)
	if resolveErr != nil {
		return map[string]interface{}{"valid": false, "message": resolveErr.Error()}, nil
	}

	if resolved.Group != nil {
		groupId := resolved.Group.GroupName
		if groupId == "" {
			groupId = agentName
		}
		if groups, err := sm.teamserver.AxAgentGetCommandGroups(agentId); err == nil {
			for _, g := range groups {
				name, _ := g["name"].(string)
				if name != groupId {
					continue
				}
				if en, ok := g["enabled"].(bool); ok && !en {
					return map[string]interface{}{
						"valid":   false,
						"message": fmt.Sprintf("Command group '%s' is disabled for this session", groupId),
					}, nil
				}
				break
			}
		}
	}

	parsed, parseErr := ParseCommand(cmdline, resolved)
	if parseErr != nil {
		return map[string]interface{}{"valid": false, "message": parseErr.Error()}, nil
	}

	cmdDef := resolved.GetEffectiveCommand()

	result := map[string]interface{}{
		"valid":         true,
		"message":       "",
		"is_pre_hook":   cmdDef.HasPreHook,
		"has_post_hook": cmdDef.HasPostHook,
		"has_handler":   cmdDef.HasHandler,
		"parsed":        parsed.Args,
	}
	return result, nil
}

// /---
func (sm *ScriptManager) GetCommandNames(agentId int64) ([]string, error) {
	if sm.teamserver == nil {
		return nil, fmt.Errorf("teamserver not available")
	}

	agentName, listenerRegName, osType, err := sm.teamserver.AxGetAgentContext(agentId)
	if err != nil {
		return nil, err
	}

	groups := sm.CommandStore.GetCommandsForAgent(agentName, listenerRegName, osType)
	var names []string
	for _, g := range groups {
		for _, cmd := range g.Commands {
			names = append(names, cmd.Name)
		}
	}
	return names, nil
}
