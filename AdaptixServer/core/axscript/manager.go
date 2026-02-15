package axscript

import (
	"AdaptixServer/core/utils/logs"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"sync"

	"github.com/dop251/goja"
)

type TeamserverBridge interface {
	TsAgentCommand(agentName string, agentId string, clientName string, hookId string, handlerId string, cmdline string, ui bool, args map[string]any) error
	TsAgentConsoleOutput(agentId string, messageType int, message string, clearText string, store bool)
	TsAgentConsoleErrorCommand(agentId string, client string, cmdline string, message string, HookId string, HandlerId string)

	AxGetAgentNameById(agentId string) (string, int, error)
	AxGetAgentListenerRegName(agentId string) (string, error)
	AxGetAgents() map[string]interface{}
	AxGetAgentInfo(agentId string, property string) interface{}
	AxGetAgentIds() []string
	AxGetCredentials() []interface{}
	AxGetTargets() []interface{}

	AxCredentialsAdd(creds []map[string]interface{}) error
	AxTargetsAdd(targets []map[string]interface{}) error
	AxAgentRemove(agentIds []string) error
	AxAgentSetTag(agentIds []string, tag string) error
	AxAgentSetMark(agentIds []string, mark string) error
	AxAgentSetColor(agentIds []string, background string, foreground string, reset bool) error
	AxAgentUpdateData(agentId string, updateData map[string]interface{}) error

	AxGetDownloads() []interface{}
	AxGetScreenshots() []interface{}
	AxGetTunnels() []interface{}
	AxGetInterfaces() []string
	AxGetAgentMark(agentId string) string
	AxUnloadAxScript(name string) error
}

type ScriptManager struct {
	mu sync.RWMutex

	teamserver TeamserverBridge
	Registry   *CommandRegistry
	HookStore  *HookStore

	agentEngines       map[string]*ScriptEngine
	userEngines        map[string]*ScriptEngine
	axscriptEngines    map[string]*ScriptEngine
	scriptInfos        []ScriptInfo
	globalAllowedRoots []string
}

func NewScriptManager(ts TeamserverBridge) *ScriptManager {
	return &ScriptManager{
		teamserver:      ts,
		Registry:        NewCommandRegistry(),
		HookStore:       NewHookStore(),
		agentEngines:    make(map[string]*ScriptEngine),
		userEngines:     make(map[string]*ScriptEngine),
		axscriptEngines: make(map[string]*ScriptEngine),
	}
}

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

	logs.Success("AxScript", "Loaded agent script for '%s'", agentName)
	return nil
}

func (sm *ScriptManager) executeRegisterCommands(engine *ScriptEngine, agentName string, listenerType string) {
	engine.mu.Lock()
	rt := engine.runtime

	fn := rt.Get("RegisterCommands")
	if fn == nil || goja.IsUndefined(fn) {
		engine.mu.Unlock()
		logs.Warn("AxScript", "No RegisterCommands function found in script for '%s'", agentName)
		return
	}

	registerFn, ok := goja.AssertFunction(fn)
	if !ok {
		engine.mu.Unlock()
		logs.Error("AxScript", "RegisterCommands is not a function in script for '%s'", agentName)
		return
	}

	result, err := registerFn(goja.Undefined(), rt.ToValue(listenerType))
	engine.mu.Unlock()

	if err != nil {
		logs.Error("AxScript", "Error calling RegisterCommands for '%s': %v", agentName, err)
		return
	}

	if result == nil || goja.IsUndefined(result) || goja.IsNull(result) {
		logs.Warn("AxScript", "RegisterCommands returned nil for '%s'", agentName)
		return
	}

	obj := result.ToObject(rt)
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
		logs.Warn("AxScript", "Property '%s' for agent '%s' is not a CommandGroup", propName, agentName)
		return
	}

	group := groupBuilder.ToCommandGroup(agentName)
	sm.Registry.RegisterGroups(agentName, listenerType, osType, []CommandGroup{group}, engine)
	logs.Debug("AxScript", "Registered %d commands for '%s' listener='%s' os=%s", len(group.Commands), agentName, listenerType, OsToString(osType))
}

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

	logs.Success("AxScript", "Loaded user script '%s'", name)
	return nil
}

func (sm *ScriptManager) UnloadUserScript(name string) error {
	sm.mu.Lock()
	defer sm.mu.Unlock()

	if _, ok := sm.userEngines[name]; !ok {
		return fmt.Errorf("user script '%s' not found", name)
	}

	delete(sm.userEngines, name)

	for i, info := range sm.scriptInfos {
		if info.Name == name && info.ScriptType == "user" {
			sm.scriptInfos = append(sm.scriptInfos[:i], sm.scriptInfos[i+1:]...)
			break
		}
	}

	logs.Success("AxScript", "Unloaded user script '%s'", name)
	return nil
}

func (sm *ScriptManager) ListScripts() []ScriptInfo {
	sm.mu.RLock()
	defer sm.mu.RUnlock()

	result := make([]ScriptInfo, len(sm.scriptInfos))
	copy(result, sm.scriptInfos)
	return result
}

func (sm *ScriptManager) ResolveAndExecutePreHook(agentName string, agentId string, listenerRegName string, os int, cmdline string, args map[string]interface{}) (hookId string, handlerId string, preHookHandled bool, err error) {
	resolved, resolveErr := sm.Registry.ResolveFromCmdline(agentName, listenerRegName, os, cmdline)
	if resolveErr != nil {
		return "", "", false, nil
	}

	cmdDef := resolved.Command
	if resolved.Subcommand != nil {
		cmdDef = resolved.Subcommand
	}

	if cmdDef.HasPreHook && cmdDef.PreHookFunc != nil && resolved.Engine != nil {
		preHookErr := sm.executePreHook(resolved.Engine, cmdDef.PreHookFunc, agentId, cmdline, args)
		if preHookErr != nil {
			return "", "", true, preHookErr
		}
		return "", "", true, nil
	}

	if cmdDef.HasPostHook && cmdDef.PostHookFunc != nil && resolved.Engine != nil {
		hookId = sm.HookStore.RegisterPostHook(resolved.Engine, cmdDef.PostHookFunc, agentId, "server")
	}

	if cmdDef.HasHandler && cmdDef.HandlerFunc != nil && resolved.Engine != nil {
		handlerId = sm.HookStore.RegisterHandler(resolved.Engine, cmdDef.HandlerFunc, agentId, "server")
	}

	return hookId, handlerId, false, nil
}

func (sm *ScriptManager) ParseCommandPublic(cmdline string, resolved *ResolvedCommand) (*ParsedCommand, error) {
	return ParseCommand(cmdline, resolved)
}

func (sm *ScriptManager) ExecutePreHookPublic(engine *ScriptEngine, fn goja.Callable, agentId string, cmdline string, args map[string]interface{}) error {
	return sm.executePreHook(engine, fn, agentId, cmdline, args)
}

func (sm *ScriptManager) ResolveFileArgsPublic(engine *ScriptEngine, parsed *ParsedCommand) error {
	return sm.resolveFileArgs(engine, parsed)
}

func (sm *ScriptManager) executePreHook(engine *ScriptEngine, fn goja.Callable, agentId string, cmdline string, args map[string]interface{}) error {
	argsCopy := make(map[string]interface{}, len(args))
	for k, v := range args {
		argsCopy[k] = v
	}

	_, err := engine.CallCallable(fn,
		engine.ToValue(agentId),
		engine.ToValue(cmdline),
		engine.ToValue(argsCopy),
	)
	return err
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
		data, err := sm.ReadFileSandboxed(engine, fa.OriginalPath)
		if err != nil {
			return fmt.Errorf("cannot read file for argument '%s': %w", fa.ArgName, err)
		}
		parsed.Args[fa.ArgName] = base64.StdEncoding.EncodeToString(data)
	}
	return nil
}

func (sm *ScriptManager) ExecuteCommand(fromEngine *ScriptEngine, agentId string, cmdline string, postHookFn goja.Callable, handlerFn goja.Callable) error {
	if sm.teamserver == nil {
		return fmt.Errorf("teamserver not available")
	}

	agentName, os, err := sm.teamserver.AxGetAgentNameById(agentId)
	if err != nil {
		return err
	}

	listenerRegName, _ := sm.teamserver.AxGetAgentListenerRegName(agentId)

	resolved, resolveErr := sm.Registry.ResolveFromCmdline(agentName, listenerRegName, os, cmdline)
	if resolveErr != nil {
		return resolveErr
	}

	parsed, parseErr := ParseCommand(cmdline, resolved)
	if parseErr != nil {
		return parseErr
	}

	if fromEngine != nil {
		if fileErr := sm.resolveFileArgs(fromEngine, parsed); fileErr != nil {
			return fileErr
		}
	}

	hookId := ""
	handlerId := ""

	if postHookFn != nil {
		hookId = sm.HookStore.RegisterPostHook(fromEngine, postHookFn, agentId, "server")
	}
	if handlerFn != nil {
		handlerId = sm.HookStore.RegisterHandler(fromEngine, handlerFn, agentId, "server")
	}

	return sm.teamserver.TsAgentCommand(agentName, agentId, "server", hookId, handlerId, cmdline, false, parsed.Args)
}

func (sm *ScriptManager) ExecuteAlias(fromEngine *ScriptEngine, agentId string, aliasCmdline string) error {
	if sm.teamserver == nil {
		return fmt.Errorf("teamserver not available")
	}

	agentName, os, err := sm.teamserver.AxGetAgentNameById(agentId)
	if err != nil {
		return err
	}

	listenerRegName, _ := sm.teamserver.AxGetAgentListenerRegName(agentId)

	resolved, resolveErr := sm.Registry.ResolveFromCmdline(agentName, listenerRegName, os, aliasCmdline)
	if resolveErr != nil {
		return resolveErr
	}

	parsed, parseErr := ParseCommand(aliasCmdline, resolved)
	if parseErr != nil {
		return parseErr
	}

	if fromEngine != nil {
		if fileErr := sm.resolveFileArgs(fromEngine, parsed); fileErr != nil {
			return fileErr
		}
	}

	cmdDef := resolved.Command
	if resolved.Subcommand != nil {
		cmdDef = resolved.Subcommand
	}

	if cmdDef.HasPreHook && cmdDef.PreHookFunc != nil && resolved.Engine != nil {
		preHookErr := sm.executePreHook(resolved.Engine, cmdDef.PreHookFunc, agentId, aliasCmdline, parsed.Args)
		if preHookErr != nil {
			return preHookErr
		}
		return nil
	}

	hookId := ""
	handlerId := ""

	if cmdDef.HasPostHook && cmdDef.PostHookFunc != nil && resolved.Engine != nil {
		hookId = sm.HookStore.RegisterPostHook(resolved.Engine, cmdDef.PostHookFunc, agentId, "server")
	}
	if cmdDef.HasHandler && cmdDef.HandlerFunc != nil && resolved.Engine != nil {
		handlerId = sm.HookStore.RegisterHandler(resolved.Engine, cmdDef.HandlerFunc, agentId, "server")
	}

	return sm.teamserver.TsAgentCommand(agentName, agentId, "server", hookId, handlerId, aliasCmdline, false, parsed.Args)
}

func (sm *ScriptManager) ExecuteAliasWithHooks(fromEngine *ScriptEngine, agentId string, aliasCmdline string, postHookFn goja.Callable, handlerFn goja.Callable) error {
	if sm.teamserver == nil {
		return fmt.Errorf("teamserver not available")
	}

	agentName, os, err := sm.teamserver.AxGetAgentNameById(agentId)
	if err != nil {
		return err
	}

	listenerRegName, _ := sm.teamserver.AxGetAgentListenerRegName(agentId)

	resolved, resolveErr := sm.Registry.ResolveFromCmdline(agentName, listenerRegName, os, aliasCmdline)
	if resolveErr != nil {
		return resolveErr
	}

	parsed, parseErr := ParseCommand(aliasCmdline, resolved)
	if parseErr != nil {
		return parseErr
	}

	if fromEngine != nil {
		if fileErr := sm.resolveFileArgs(fromEngine, parsed); fileErr != nil {
			return fileErr
		}
	}

	cmdDef := resolved.Command
	if resolved.Subcommand != nil {
		cmdDef = resolved.Subcommand
	}

	if cmdDef.HasPreHook && cmdDef.PreHookFunc != nil && resolved.Engine != nil {
		preHookErr := sm.executePreHook(resolved.Engine, cmdDef.PreHookFunc, agentId, aliasCmdline, parsed.Args)
		if preHookErr != nil {
			return preHookErr
		}
		return nil
	}

	hookId := ""
	handlerId := ""

	// Explicit hook/handler from caller take priority over command definition
	if postHookFn != nil {
		hookId = sm.HookStore.RegisterPostHook(fromEngine, postHookFn, agentId, "server")
	} else if cmdDef.HasPostHook && cmdDef.PostHookFunc != nil && resolved.Engine != nil {
		hookId = sm.HookStore.RegisterPostHook(resolved.Engine, cmdDef.PostHookFunc, agentId, "server")
	}

	if handlerFn != nil {
		handlerId = sm.HookStore.RegisterHandler(fromEngine, handlerFn, agentId, "server")
	} else if cmdDef.HasHandler && cmdDef.HandlerFunc != nil && resolved.Engine != nil {
		handlerId = sm.HookStore.RegisterHandler(resolved.Engine, cmdDef.HandlerFunc, agentId, "server")
	}

	return sm.teamserver.TsAgentCommand(agentName, agentId, "server", hookId, handlerId, aliasCmdline, false, parsed.Args)
}

func (sm *ScriptManager) GetAgents() map[string]interface{} {
	if sm.teamserver == nil {
		return map[string]interface{}{}
	}
	return sm.teamserver.AxGetAgents()
}

func (sm *ScriptManager) GetAgentInfo(agentId string, property string) interface{} {
	if sm.teamserver == nil {
		return nil
	}
	return sm.teamserver.AxGetAgentInfo(agentId, property)
}

func (sm *ScriptManager) GetAgentIds() []string {
	if sm.teamserver == nil {
		return []string{}
	}
	return sm.teamserver.AxGetAgentIds()
}

func (sm *ScriptManager) GetCredentials() []interface{} {
	if sm.teamserver == nil {
		return []interface{}{}
	}
	return sm.teamserver.AxGetCredentials()
}

func (sm *ScriptManager) GetTargets() []interface{} {
	if sm.teamserver == nil {
		return []interface{}{}
	}
	return sm.teamserver.AxGetTargets()
}

func (sm *ScriptManager) ConsoleMessage(agentId string, msgType int, message string, clearText string) {
	if sm.teamserver == nil {
		return
	}
	sm.teamserver.TsAgentConsoleOutput(agentId, msgType, message, clearText, false)
}

func (sm *ScriptManager) GetCommandsJSON() (string, error) {
	allCommands := sm.Registry.GetAllCommands()
	data, err := json.Marshal(allCommands)
	if err != nil {
		return "", err
	}
	return string(data), nil
}

func (sm *ScriptManager) SetGlobalAllowedRoots(roots []string) {
	sm.globalAllowedRoots = roots
}

func (sm *ScriptManager) LoadAxScript(scriptPath string) error {
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

	registerFormStubs(engine)
	registerMenuStubs(engine)
	registerEventStubs(engine)
	registerAxBridge(engine)

	err = engine.Execute(string(content))
	if err != nil {
		return fmt.Errorf("failed to execute script '%s': %w", abs, err)
	}

	sm.mu.Lock()
	sm.axscriptEngines[abs] = engine
	sm.scriptInfos = append(sm.scriptInfos, ScriptInfo{
		Name:       filepath.Base(abs),
		ScriptType: "axscript",
		Path:       abs,
	})
	sm.mu.Unlock()

	logs.Success("AxScript", "Loaded axscript '%s'", abs)
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

	// Child inherits parent's allowed roots
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

	sm.mu.Lock()
	sm.axscriptEngines[abs] = engine
	sm.scriptInfos = append(sm.scriptInfos, ScriptInfo{
		Name:       filepath.Base(abs),
		ScriptType: "axscript",
		Path:       abs,
	})
	sm.mu.Unlock()

	logs.Success("AxScript", "Loaded axscript '%s'", abs)
	return nil
}

func (sm *ScriptManager) ImportAxScript(engine *ScriptEngine, scriptPath string) error {
	abs, err := filepath.Abs(scriptPath)
	if err != nil {
		return fmt.Errorf("invalid script path '%s': %w", scriptPath, err)
	}

	// Validate path is within engine's allowed roots
	_, err = engine.ValidatePath(abs)
	if err != nil {
		return err
	}

	content, err := os.ReadFile(abs)
	if err != nil {
		return fmt.Errorf("failed to read script '%s': %w", abs, err)
	}

	// Execute in the SAME engine context
	engine.mu.Lock()
	_, execErr := engine.runtime.RunString(string(content))
	engine.mu.Unlock()

	if execErr != nil {
		return fmt.Errorf("failed to import script '%s': %w", abs, execErr)
	}

	logs.Debug("AxScript", "Imported '%s' into engine '%s'", abs, engine.name)
	return nil
}

func (sm *ScriptManager) ReadFileSandboxed(engine *ScriptEngine, path string) ([]byte, error) {
	validated, err := engine.ValidatePath(path)
	if err != nil {
		return nil, err
	}
	return os.ReadFile(validated)
}

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

func (sm *ScriptManager) GetDownloads() []interface{} {
	if sm.teamserver == nil {
		return []interface{}{}
	}
	return sm.teamserver.AxGetDownloads()
}

func (sm *ScriptManager) GetScreenshots() []interface{} {
	if sm.teamserver == nil {
		return []interface{}{}
	}
	return sm.teamserver.AxGetScreenshots()
}

func (sm *ScriptManager) GetTunnels() []interface{} {
	if sm.teamserver == nil {
		return []interface{}{}
	}
	return sm.teamserver.AxGetTunnels()
}

func (sm *ScriptManager) GetInterfaces() []string {
	if sm.teamserver == nil {
		return []string{}
	}
	return sm.teamserver.AxGetInterfaces()
}

func (sm *ScriptManager) GetAgentMark(agentId string) string {
	if sm.teamserver == nil {
		return ""
	}
	return sm.teamserver.AxGetAgentMark(agentId)
}

func (sm *ScriptManager) UnloadAxScript(name string) error {
	if sm.teamserver == nil {
		return fmt.Errorf("teamserver not available")
	}
	return sm.teamserver.AxUnloadAxScript(name)
}

func (sm *ScriptManager) ValidateCommand(agentId string, cmdline string) (map[string]interface{}, error) {
	if sm.teamserver == nil {
		return nil, fmt.Errorf("teamserver not available")
	}

	agentName, osType, err := sm.teamserver.AxGetAgentNameById(agentId)
	if err != nil {
		return map[string]interface{}{"valid": false, "message": "Agent not found"}, nil
	}

	listenerRegName, _ := sm.teamserver.AxGetAgentListenerRegName(agentId)

	resolved, resolveErr := sm.Registry.ResolveFromCmdline(agentName, listenerRegName, osType, cmdline)
	if resolveErr != nil {
		return map[string]interface{}{"valid": false, "message": resolveErr.Error()}, nil
	}

	parsed, parseErr := ParseCommand(cmdline, resolved)
	if parseErr != nil {
		return map[string]interface{}{"valid": false, "message": parseErr.Error()}, nil
	}

	cmdDef := resolved.Command
	if resolved.Subcommand != nil {
		cmdDef = resolved.Subcommand
	}

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

func (sm *ScriptManager) GetCommandNames(agentId string) ([]string, error) {
	if sm.teamserver == nil {
		return nil, fmt.Errorf("teamserver not available")
	}

	agentName, osType, err := sm.teamserver.AxGetAgentNameById(agentId)
	if err != nil {
		return nil, err
	}

	listenerRegName, _ := sm.teamserver.AxGetAgentListenerRegName(agentId)

	groups := sm.Registry.GetCommandsForAgent(agentName, listenerRegName, osType)
	var names []string
	for _, g := range groups {
		for _, cmd := range g.Commands {
			names = append(names, cmd.Name)
		}
	}
	return names, nil
}
