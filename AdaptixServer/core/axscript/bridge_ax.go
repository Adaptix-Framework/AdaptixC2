package axscript

import (
	"crypto/md5"
	"crypto/sha1"
	"crypto/sha256"
	"crypto/sha512"
	"encoding/base64"
	"encoding/binary"
	"encoding/hex"
	"fmt"
	"log"
	"math"
	"math/rand"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"
	"unicode/utf16"

	"github.com/Adaptix-Framework/axc2/v2"
	"github.com/dop251/goja"
)

const maxSafeJSInt = int64(1) << 53

func registerAxBridge(engine *ScriptEngine) {
	rt := engine.runtime

	axObj := rt.NewObject()

	axObj.Set("create_command", func(call goja.FunctionCall) goja.Value {
		name := ""
		description := ""
		example := ""
		message := ""
		if len(call.Arguments) > 0 {
			name = call.Argument(0).String()
		}
		if len(call.Arguments) > 1 {
			description = call.Argument(1).String()
		}
		if len(call.Arguments) > 2 {
			example = call.Argument(2).String()
		}
		if len(call.Arguments) > 3 {
			message = call.Argument(3).String()
		}

		builder := newJsCommandBuilder(engine, name, description, example, message)

		obj := rt.NewObject()
		obj.Set("addArgBool", builder.AddArgBool)
		obj.Set("addArgInt", builder.AddArgInt)
		obj.Set("addArgFlagInt", builder.AddArgFlagInt)
		obj.Set("addArgString", builder.AddArgString)
		obj.Set("addArgFlagString", builder.AddArgFlagString)
		obj.Set("addArgFile", builder.AddArgFile)
		obj.Set("addArgFlagFile", builder.AddArgFlagFile)
		obj.Set("addSubCommands", builder.AddSubCommands)
		obj.Set("setPreHook", builder.SetPreHook)
		obj.Set("setPostHook", builder.SetPostHook)
		obj.Set("setHandler", builder.SetHandler)
		obj.Set("setDestructive", builder.SetDestructive)

		obj.Set("__builder", builder)

		return obj
	})

	axObj.Set("create_commands_group", func(call goja.FunctionCall) goja.Value {
		bindGroup := func(group *jsCommandGroupBuilder) *goja.Object {
			obj := rt.NewObject()
			obj.Set("setParams", group.SetParams)
			obj.Set("add", group.Add)
			obj.Set("setDefaultEnabled", group.SetDefaultEnabled)
			obj.Set("__group", group)
			return obj
		}

		if len(call.Arguments) >= 2 {
			groupName := call.Argument(0).String()
			arrayVal := call.Argument(1)
			group := newJsCommandGroupBuilder(engine)
			group.SetParamsFromValue(groupName, arrayVal)
			if len(call.Arguments) >= 3 && !goja.IsUndefined(call.Argument(2)) && !goja.IsNull(call.Argument(2)) {
				group.defaultEnabled = call.Argument(2).ToBoolean()
			}
			return bindGroup(group)
		}

		return bindGroup(newJsCommandGroupBuilder(engine))
	})

	axObj.Set("execute_command", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil {
			panic(rt.NewTypeError("execute_command: script manager not available"))
		}

		if len(call.Arguments) < 2 {
			panic(rt.NewTypeError("execute_command requires at least 2 arguments: (agentId, cmdline)"))
		}

		agentId := exportInt64(call.Argument(0))
		cmdline := call.Argument(1).String()

		var postHookFn goja.Callable
		var handlerFn goja.Callable

		if len(call.Arguments) > 2 && !goja.IsUndefined(call.Argument(2)) && !goja.IsNull(call.Argument(2)) {
			fn, ok := goja.AssertFunction(call.Argument(2))
			if ok {
				postHookFn = fn
			}
		}
		if len(call.Arguments) > 3 && !goja.IsUndefined(call.Argument(3)) && !goja.IsNull(call.Argument(3)) {
			fn, ok := goja.AssertFunction(call.Argument(3))
			if ok {
				handlerFn = fn
			}
		}

		err := engine.manager.ExecuteCommand(engine, agentId, cmdline, engine.CurrentClient(), postHookFn, handlerFn)
		if err != nil {
			panic(rt.NewGoError(err))
		}

		return goja.Undefined()
	})

	axObj.Set("execute_alias", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil {
			panic(rt.NewTypeError("execute_alias: script manager not available"))
		}

		if len(call.Arguments) < 3 {
			panic(rt.NewTypeError("execute_alias requires at least 3 arguments: (agentId, originalCmdline, aliasCmdline)"))
		}

		agentId := exportInt64(call.Argument(0))
		displayCmdline := call.Argument(1).String()
		aliasCmdline := call.Argument(2).String()
		message := ""
		if len(call.Arguments) > 3 && !goja.IsUndefined(call.Argument(3)) && !goja.IsNull(call.Argument(3)) {
			message = call.Argument(3).String()
		}
		// arg4 = hook (optional)
		// arg5 = handler (optional)

		var postHookFn goja.Callable
		var handlerFn goja.Callable

		if len(call.Arguments) > 4 && !goja.IsUndefined(call.Argument(4)) && !goja.IsNull(call.Argument(4)) {
			fn, ok := goja.AssertFunction(call.Argument(4))
			if ok {
				postHookFn = fn
			}
		}
		if len(call.Arguments) > 5 && !goja.IsUndefined(call.Argument(5)) && !goja.IsNull(call.Argument(5)) {
			fn, ok := goja.AssertFunction(call.Argument(5))
			if ok {
				handlerFn = fn
			}
		}

		err := engine.manager.ExecuteAliasWithHooks(engine, agentId, displayCmdline, aliasCmdline, message, engine.CurrentClient(), postHookFn, handlerFn)
		if err != nil {
			panic(rt.NewGoError(err))
		}

		return goja.Undefined()
	})

	axObj.Set("execute_alias_hook", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil {
			panic(rt.NewTypeError("execute_alias_hook: script manager not available"))
		}
		if len(call.Arguments) < 5 {
			panic(rt.NewTypeError("execute_alias_hook requires 5 arguments"))
		}
		agentId := exportInt64(call.Argument(0))
		displayCmdline := call.Argument(1).String()
		aliasCmdline := call.Argument(2).String()
		message := ""
		if len(call.Arguments) > 3 && !goja.IsUndefined(call.Argument(3)) && !goja.IsNull(call.Argument(3)) {
			message = call.Argument(3).String()
		}
		var hookFn goja.Callable
		if fn, ok := goja.AssertFunction(call.Argument(4)); ok {
			hookFn = fn
		}
		err := engine.manager.ExecuteAliasWithHooks(engine, agentId, displayCmdline, aliasCmdline, message, engine.CurrentClient(), hookFn, nil)
		if err != nil {
			panic(rt.NewGoError(err))
		}
		return goja.Undefined()
	})

	axObj.Set("execute_alias_handler", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil {
			panic(rt.NewTypeError("execute_alias_handler: script manager not available"))
		}
		if len(call.Arguments) < 5 {
			panic(rt.NewTypeError("execute_alias_handler requires 5 arguments"))
		}
		agentId := exportInt64(call.Argument(0))
		displayCmdline := call.Argument(1).String()
		aliasCmdline := call.Argument(2).String()
		message := ""
		if len(call.Arguments) > 3 && !goja.IsUndefined(call.Argument(3)) && !goja.IsNull(call.Argument(3)) {
			message = call.Argument(3).String()
		}
		var handlerFn goja.Callable
		if fn, ok := goja.AssertFunction(call.Argument(4)); ok {
			handlerFn = fn
		}
		err := engine.manager.ExecuteAliasWithHooks(engine, agentId, displayCmdline, aliasCmdline, message, engine.CurrentClient(), nil, handlerFn)
		if err != nil {
			panic(rt.NewGoError(err))
		}
		return goja.Undefined()
	})

	axObj.Set("agents", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil {
			return rt.ToValue(map[string]interface{}{})
		}
		agents := engine.manager.GetAgents()
		return rt.ToValue(agents)
	})

	axObj.Set("agent_info", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || len(call.Arguments) < 2 {
			return goja.Undefined()
		}
		agentId := exportInt64(call.Argument(0))
		property := call.Argument(1).String()
		info := engine.manager.GetAgentInfo(agentId, property)
		if info == nil {
			return goja.Undefined()
		}
		return rt.ToValue(info)
	})

	axObj.Set("ids", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil {
			return rt.ToValue([]string{})
		}
		return rt.ToValue(engine.manager.GetAgentIds())
	})

	axObj.Set("credentials", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil {
			return rt.ToValue([]interface{}{})
		}
		return rt.ToValue(engine.manager.GetCredentials())
	})

	axObj.Set("targets", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil {
			return rt.ToValue([]interface{}{})
		}
		return rt.ToValue(engine.manager.GetTargets())
	})

	axObj.Set("payloads", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil {
			return rt.ToValue([]interface{}{})
		}
		return rt.ToValue(engine.manager.GetPayloads())
	})

	axObj.Set("console_message", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || len(call.Arguments) < 2 {
			return goja.Undefined()
		}
		agentId := exportInt64(call.Argument(0))
		message := call.Argument(1).String()
		typeStr := ""
		if len(call.Arguments) > 2 {
			typeStr = call.Argument(2).String()
		}
		clearText := ""
		if len(call.Arguments) > 3 {
			clearText = call.Argument(3).String()
		}

		msgType := 10 // CONSOLE_OUT
		switch strings.ToLower(typeStr) {
		case "info":
			msgType = 2 // CONSOLE_OUT_LOCAL_INFO
		case "success":
			msgType = 4 // CONSOLE_OUT_LOCAL_SUCCESS
		case "error":
			msgType = 3 // CONSOLE_OUT_LOCAL_ERROR
		}

		engine.manager.ConsoleMessage(agentId, engine.CurrentClient(), msgType, message, clearText)
		return goja.Undefined()
	})

	axObj.Set("log", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) > 0 {
			engine.manager.teamserver.TsLogAdd(adaptix.LogStatusInfo, 0, "axscript_manager", "log", "[%s] %s", engine.name, call.Argument(0).String())
		}
		return goja.Undefined()
	})

	axObj.Set("log_error", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) > 0 {
			engine.manager.teamserver.TsLogAdd(adaptix.LogStatusError, 0, "axscript_manager", "log_error", "[%s] %s", engine.name, call.Argument(0).String())
		}
		return goja.Undefined()
	})

	axObj.Set("execute_command_hook", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || len(call.Arguments) < 3 {
			return rt.ToValue(false)
		}
		agentId := exportInt64(call.Argument(0))
		cmdline := call.Argument(1).String()
		var hookFn goja.Callable
		if fn, ok := goja.AssertFunction(call.Argument(2)); ok {
			hookFn = fn
		}
		err := engine.manager.ExecuteCommand(engine, agentId, cmdline, engine.CurrentClient(), hookFn, nil)
		if err != nil {
			panic(rt.NewGoError(err))
		}
		return goja.Undefined()
	})

	axObj.Set("execute_command_handler", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || len(call.Arguments) < 3 {
			return rt.ToValue(false)
		}
		agentId := exportInt64(call.Argument(0))
		cmdline := call.Argument(1).String()
		var handlerFn goja.Callable
		if fn, ok := goja.AssertFunction(call.Argument(2)); ok {
			handlerFn = fn
		}
		err := engine.manager.ExecuteCommand(engine, agentId, cmdline, engine.CurrentClient(), nil, handlerFn)
		if err != nil {
			panic(rt.NewGoError(err))
		}
		return goja.Undefined()
	})

	axObj.Set("register_commands_group", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || len(call.Arguments) < 2 {
			return rt.ToValue(false)
		}

		groupVal := call.Argument(0)
		agentsVal := call.Argument(1)

		var groupBuilder *jsCommandGroupBuilder

		if exported := groupVal.Export(); exported != nil {
			if gb, ok := exported.(*jsCommandGroupBuilder); ok {
				groupBuilder = gb
			} else if m, ok := exported.(map[string]interface{}); ok {
				if gv, exists := m["__group"]; exists {
					if gb2, ok2 := gv.(*jsCommandGroupBuilder); ok2 {
						groupBuilder = gb2
					}
				}
			}
		}

		if groupBuilder == nil {
			groupObj := groupVal.ToObject(rt)
			gv := groupObj.Get("__group")
			if gv != nil && !goja.IsUndefined(gv) && !goja.IsNull(gv) {
				if gb, ok := gv.Export().(*jsCommandGroupBuilder); ok {
					groupBuilder = gb
				}
			}
		}

		if groupBuilder == nil {
			engine.manager.teamserver.TsLogAdd(adaptix.LogStatusWarn, 0, "axscript_manager", "register_commands_group", "invalid group object")
			return goja.Undefined()
		}

		metaVal := rt.Get("metadata")
		if metaVal != nil && !goja.IsUndefined(metaVal) && !goja.IsNull(metaVal) {
			metaObj := metaVal.ToObject(rt)
			if groupBuilder.name == "" {
				nameVal := metaObj.Get("name")
				if nameVal != nil && !goja.IsUndefined(nameVal) && !goja.IsNull(nameVal) {
					groupBuilder.name = nameVal.String()
				}
			}
			if groupBuilder.description == "" {
				descVal := metaObj.Get("description")
				if descVal != nil && !goja.IsUndefined(descVal) && !goja.IsNull(descVal) {
					groupBuilder.description = descVal.String()
				}
			}
		}

		agentNames := exportStringArray(rt, agentsVal)

		var osList []int
		if len(call.Arguments) > 2 {
			osStrings := exportStringArray(rt, call.Argument(2))
			for _, s := range osStrings {
				if v := OsFromString(s); v != 0 {
					osList = append(osList, v)
				}
			}
		}
		if len(osList) == 0 {
			osList = []int{OsWindows, OsLinux, OsMac}
		}

		var listenerTypes []string
		if len(call.Arguments) > 3 {
			listenerTypes = exportStringArray(rt, call.Argument(3))
		}
		if len(listenerTypes) == 0 {
			listenerTypes = []string{""}
		}

		var sourceType SourceType
		var scriptName string
		if strings.HasPrefix(engine.name, "user:") {
			sourceType = SourceUser
			scriptName = strings.TrimPrefix(engine.name, "user:")
		} else {
			sourceType = SourceProfile
			scriptName = engine.GetMetadataName()
			if scriptName == "" {
				scriptName = filepath.Base(engine.scriptPath)
			}
		}

		for _, agentName := range agentNames {
			group := groupBuilder.ToCommandGroup(scriptName)
			for _, listener := range listenerTypes {
				for _, osType := range osList {
					engine.manager.CommandStore.RegisterGroups(sourceType, agentName, listener, osType, []CommandGroup{group}, engine)
				}
			}
		}

		return goja.Undefined()
	})

	axObj.Set("register_service_commands", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || len(call.Arguments) < 1 {
			return rt.ToValue(false)
		}
		groupBuilder := extractGroupBuilder(call.Argument(0))
		if groupBuilder == nil {
			engine.manager.teamserver.TsLogAdd(adaptix.LogStatusWarn, 0, "axscript_manager", "register_service_commands", "invalid group object")
			return goja.Undefined()
		}
		serviceName := strings.TrimPrefix(engine.name, "service:")
		if serviceName == "" || serviceName == engine.name {
			serviceName = groupBuilder.name
		}
		if serviceName == "" {
			engine.manager.teamserver.TsLogAdd(adaptix.LogStatusWarn, 0, "axscript_manager", "register_service_commands", "empty service name")
			return goja.Undefined()
		}
		metaVal := rt.Get("metadata")
		if metaVal != nil && !goja.IsUndefined(metaVal) && !goja.IsNull(metaVal) {
			metaObj := metaVal.ToObject(rt)
			if groupBuilder.description == "" {
				if descVal := metaObj.Get("description"); descVal != nil && !goja.IsUndefined(descVal) && !goja.IsNull(descVal) {
					groupBuilder.description = descVal.String()
				}
			}
		}
		group := groupBuilder.ToCommandGroup(serviceName)
		engine.manager.RegisterServiceCommandGroup(serviceName, group)
		return goja.Undefined()
	})

	axObj.Set("execute_browser", func(call goja.FunctionCall) goja.Value {
		return goja.Undefined()
	})

	// --- Script management ---

	axObj.Set("script_dir", func(call goja.FunctionCall) goja.Value {
		return rt.ToValue(engine.scriptDir)
	})

	axObj.Set("script_load", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || len(call.Arguments) == 0 {
			return goja.Undefined()
		}
		path := call.Argument(0).String()
		err := engine.manager.LoadAxScriptChild(engine, path)
		if err != nil {
			engine.manager.teamserver.TsLogAdd(adaptix.LogStatusError, 0, "axscript_manager", "script", "load error: %v", err)
			panic(rt.NewGoError(err))
		}
		return goja.Undefined()
	})

	axObj.Set("script_import", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || len(call.Arguments) == 0 {
			return goja.Undefined()
		}
		path := call.Argument(0).String()
		absPath, err := engine.manager.ImportAxScript(engine, path)
		if err != nil {
			engine.manager.teamserver.TsLogAdd(adaptix.LogStatusError, 0, "axscript_manager", "script", "import error: %v", err)
			panic(rt.NewGoError(err))
		}
		engine.AddImportedFile(absPath)
		return goja.Undefined()
	})

	axObj.Set("script_unload", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || len(call.Arguments) == 0 {
			return goja.Undefined()
		}
		name := call.Argument(0).String()
		err := engine.manager.UnloadAxScript(name)
		if err != nil {
			engine.manager.teamserver.TsLogAdd(adaptix.LogStatusWarn, 0, "axscript_manager", "script", "unload error: %v", err)
		}
		return goja.Undefined()
	})

	// --- BOF support ---

	axObj.Set("bof_pack", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) < 2 {
			panic(rt.NewTypeError("bof_pack requires 2 arguments: (types, args)"))
		}
		types := call.Argument(0).String()
		argsVal := call.Argument(1)

		argsObj := argsVal.ToObject(rt)
		lengthVal := argsObj.Get("length")
		if lengthVal == nil || goja.IsUndefined(lengthVal) {
			panic(rt.NewTypeError("bof_pack: args must be an array"))
		}
		arrLen := int(lengthVal.ToInteger())

		items := parseBofTypes(types)
		if len(items) != arrLen {
			panic(rt.NewTypeError(fmt.Sprintf("bof_pack: types count (%d) must match args count (%d) — types=%q parsed=%v", len(items), arrLen, types, items)))
		}

		var data []byte

		for i := 0; i < arrLen; i++ {
			val := argsObj.Get(fmt.Sprintf("%d", i))
			switch items[i] {
			case "cstr":
				s := val.String()
				b := []byte(s)
				b = append(b, 0)
				l := uint32(len(b))
				lb := make([]byte, 4)
				binary.LittleEndian.PutUint32(lb, l)
				data = append(data, lb...)
				data = append(data, b...)
			case "wstr":
				s := val.String()
				runes := utf16.Encode([]rune(s))
				runes = append(runes, 0)
				byteLen := uint32(len(runes) * 2)
				lb := make([]byte, 4)
				binary.LittleEndian.PutUint32(lb, byteLen)
				data = append(data, lb...)
				for _, r := range runes {
					rb := make([]byte, 2)
					binary.LittleEndian.PutUint16(rb, r)
					data = append(data, rb...)
				}
			case "bytes":
				// Accept ArrayBuffer / Uint8Array / string (UTF-8 bytes).
				raw, _ := jsBytes(val)
				l := uint32(len(raw))
				lb := make([]byte, 4)
				binary.LittleEndian.PutUint32(lb, l)
				data = append(data, lb...)
				data = append(data, raw...)
			case "int":
				n := int32(val.ToInteger())
				nb := make([]byte, 4)
				binary.LittleEndian.PutUint32(nb, uint32(n))
				data = append(data, nb...)
			case "short":
				n := int16(val.ToInteger())
				nb := make([]byte, 2)
				binary.LittleEndian.PutUint16(nb, uint16(n))
				data = append(data, nb...)
			default:
				panic(rt.NewTypeError(fmt.Sprintf("bof_pack: unknown type '%s'", items[i])))
			}
		}

		// Prepend total length
		totalLen := uint32(len(data))
		header := make([]byte, 4)
		binary.LittleEndian.PutUint32(header, totalLen)
		result := append(header, data...)

		return rt.ToValue(base64.StdEncoding.EncodeToString(result))
	})

	axObj.Set("arch", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || len(call.Arguments) == 0 {
			return rt.ToValue("x86")
		}
		agentId := exportInt64(call.Argument(0))
		info := engine.manager.GetAgentInfo(agentId, "arch")
		if info == nil {
			return rt.ToValue("x86")
		}
		if s, ok := info.(string); ok {
			return rt.ToValue(s)
		}
		return rt.ToValue("x86")
	})

	axObj.Set("is64", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || len(call.Arguments) == 0 {
			return rt.ToValue(false)
		}
		agentId := exportInt64(call.Argument(0))
		info := engine.manager.GetAgentInfo(agentId, "arch")
		if s, ok := info.(string); ok {
			return rt.ToValue(s == "x64")
		}
		return rt.ToValue(false)
	})

	axObj.Set("isactive", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || len(call.Arguments) == 0 {
			return rt.ToValue(false)
		}
		agentId := exportInt64(call.Argument(0))
		info := engine.manager.GetAgentInfo(agentId, "id")
		if info == nil {
			return rt.ToValue(false)
		}
		mark := engine.manager.GetAgentMark(agentId)
		if mark == "Terminated" || mark == "Disconnect" || mark == "Inactive" || mark == "Unlink" {
			return rt.ToValue(false)
		}
		return rt.ToValue(true)
	})

	axObj.Set("isadmin", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || len(call.Arguments) == 0 {
			return rt.ToValue(false)
		}
		agentId := exportInt64(call.Argument(0))
		info := engine.manager.GetAgentInfo(agentId, "elevated")
		if b, ok := info.(bool); ok {
			return rt.ToValue(b)
		}
		return rt.ToValue(false)
	})

	// --- Credentials & Targets ---

	axObj.Set("credentials_add", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || engine.manager.teamserver == nil {
			return goja.Undefined()
		}
		cred := make(map[string]interface{})
		if len(call.Arguments) > 0 {
			cred["username"] = call.Argument(0).String()
		}
		if len(call.Arguments) > 1 {
			cred["password"] = call.Argument(1).String()
		}
		if len(call.Arguments) > 2 {
			cred["realm"] = call.Argument(2).String()
		}
		if len(call.Arguments) > 3 {
			cred["type"] = call.Argument(3).String()
		}
		if len(call.Arguments) > 4 {
			cred["tag"] = call.Argument(4).String()
		}
		if len(call.Arguments) > 5 {
			cred["storage"] = call.Argument(5).String()
		}
		if len(call.Arguments) > 6 {
			cred["host"] = call.Argument(6).String()
		}
		err := engine.manager.teamserver.AxCredentialsAdd([]map[string]interface{}{cred})
		if err != nil {
			engine.manager.teamserver.TsLogAdd(adaptix.LogStatusWarn, 0, "axscript_manager", "log", "credentials_add: %v", err)
			return rt.ToValue(false)
		}
		return rt.ToValue(true)
	})

	axObj.Set("credentials_add_list", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || engine.manager.teamserver == nil || len(call.Arguments) == 0 {
			return rt.ToValue(false)
		}
		exported := call.Argument(0).Export()
		arr, ok := exported.([]interface{})
		if !ok {
			return rt.ToValue(false)
		}
		var creds []map[string]interface{}
		for _, item := range arr {
			if m, ok := item.(map[string]interface{}); ok {
				creds = append(creds, m)
			}
		}
		if len(creds) == 0 {
			return rt.ToValue(true)
		}
		err := engine.manager.teamserver.AxCredentialsAdd(creds)
		if err != nil {
			engine.manager.teamserver.TsLogAdd(adaptix.LogStatusWarn, 0, "axscript_manager", "log", "credentials_add_list: %v", err)
			return rt.ToValue(false)
		}
		return rt.ToValue(true)
	})

	axObj.Set("targets_add", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || engine.manager.teamserver == nil {
			return rt.ToValue(false)
		}
		target := make(map[string]interface{})
		if len(call.Arguments) > 0 {
			target["computer"] = call.Argument(0).String()
		}
		if len(call.Arguments) > 1 {
			target["domain"] = call.Argument(1).String()
		}
		if len(call.Arguments) > 2 {
			target["address"] = call.Argument(2).String()
		}
		if len(call.Arguments) > 3 {
			target["os"] = call.Argument(3).String()
		}
		if len(call.Arguments) > 4 {
			target["os_desc"] = call.Argument(4).String()
		}
		if len(call.Arguments) > 5 {
			target["tag"] = call.Argument(5).String()
		}
		if len(call.Arguments) > 6 {
			target["info"] = call.Argument(6).String()
		}
		if len(call.Arguments) > 7 {
			target["alive"] = call.Argument(7).ToBoolean()
		}
		err := engine.manager.teamserver.AxTargetsAdd([]map[string]interface{}{target})
		if err != nil {
			engine.manager.teamserver.TsLogAdd(adaptix.LogStatusWarn, 0, "axscript_manager", "log", "targets_add: %v", err)
			return rt.ToValue(false)
		}
		return rt.ToValue(true)
	})

	axObj.Set("targets_add_list", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || engine.manager.teamserver == nil || len(call.Arguments) == 0 {
			return rt.ToValue(false)
		}
		exported := call.Argument(0).Export()
		arr, ok := exported.([]interface{})
		if !ok {
			return rt.ToValue(false)
		}
		var targets []map[string]interface{}
		for _, item := range arr {
			if m, ok := item.(map[string]interface{}); ok {
				targets = append(targets, m)
			}
		}
		if len(targets) == 0 {
			return rt.ToValue(true)
		}
		err := engine.manager.teamserver.AxTargetsAdd(targets)
		if err != nil {
			engine.manager.teamserver.TsLogAdd(adaptix.LogStatusWarn, 0, "axscript_manager", "log", "targets_add_list: %v", err)
			return rt.ToValue(false)
		}
		return rt.ToValue(true)
	})

	// --- File operations (sandboxed) ---

	axObj.Set("file_basename", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) == 0 {
			return rt.ToValue("")
		}
		return rt.ToValue(fileBasename(call.Argument(0).String()))
	})

	axObj.Set("file_dirname", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) == 0 {
			return rt.ToValue("")
		}
		return rt.ToValue(filepath.Dir(call.Argument(0).String()))
	})

	axObj.Set("file_extension", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) == 0 {
			return rt.ToValue("")
		}
		ext := filepath.Ext(call.Argument(0).String())
		if len(ext) > 0 {
			ext = ext[1:] // remove leading dot
		}
		return rt.ToValue(ext)
	})

	axObj.Set("file_exists", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) == 0 {
			return rt.ToValue(false)
		}
		path := call.Argument(0).String()
		validated, err := engine.ValidatePath(path)
		if err != nil {
			return rt.ToValue(false)
		}
		_, err = os.Stat(validated)
		return rt.ToValue(err == nil)
	})

	axObj.Set("file_read", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || len(call.Arguments) == 0 {
			return goja.Null()
		}
		path := call.Argument(0).String()
		data, err := engine.manager.ReadFileSandboxed(engine, path)
		if err != nil {
			return goja.Null()
		}
		return rt.ToValue(rt.NewArrayBuffer(data))
	})

	axObj.Set("file_size", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) == 0 {
			return rt.ToValue(0)
		}
		path := call.Argument(0).String()
		validated, err := engine.ValidatePath(path)
		if err != nil {
			return rt.ToValue(0)
		}
		info, err := os.Stat(validated)
		if err != nil {
			return rt.ToValue(0)
		}
		return rt.ToValue(info.Size())
	})

	axObj.Set("file_write", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || len(call.Arguments) < 2 {
			return rt.ToValue(false)
		}
		path := call.Argument(0).String()

		var data []byte
		switch x := call.Argument(1).Export().(type) {
		case goja.ArrayBuffer:
			data = x.Bytes()
		case []byte:
			data = x
		case string:
			data = []byte(x)
		default:
			engine.manager.teamserver.TsLogAdd(adaptix.LogStatusWarn, 0, "axscript_manager", "file", "file_write expected ArrayBuffer/Uint8Array/string, got %T", x)
			return rt.ToValue(false)
		}

		appendMode := false
		if len(call.Arguments) > 2 {
			appendMode = call.Argument(2).ToBoolean()
		}

		if err := engine.manager.WriteFileSandboxed(engine, path, data, appendMode); err != nil {
			engine.manager.teamserver.TsLogAdd(adaptix.LogStatusWarn, 0, "axscript_manager", "file", "file_write %q: %v", path, err)
			return rt.ToValue(false)
		}
		return rt.ToValue(true)
	})

	// --- Agent management ---

	axObj.Set("agent_hide", func(call goja.FunctionCall) goja.Value {
		return goja.Undefined() // client-only UI operation
	})

	axObj.Set("agent_remove", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || engine.manager.teamserver == nil || len(call.Arguments) == 0 {
			return goja.Undefined()
		}
		ids := exportInt64Array(rt, call.Argument(0))
		if len(ids) > 0 {
			_ = engine.manager.teamserver.AxAgentRemove(ids)
		}
		return goja.Undefined()
	})

	axObj.Set("agent_set_color", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || engine.manager.teamserver == nil || len(call.Arguments) < 2 {
			return goja.Undefined()
		}
		ids := exportInt64Array(rt, call.Argument(0))
		bg := call.Argument(1).String()
		fg := ""
		if len(call.Arguments) > 2 {
			fg = call.Argument(2).String()
		}
		reset := false
		if len(call.Arguments) > 3 {
			reset = call.Argument(3).ToBoolean()
		}
		_ = engine.manager.teamserver.AxAgentSetColor(ids, bg, fg, reset)
		return goja.Undefined()
	})

	axObj.Set("agent_set_impersonate", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || engine.manager.teamserver == nil || len(call.Arguments) < 2 {
			return goja.Undefined()
		}
		agentId := exportInt64(call.Argument(0))
		impersonate := call.Argument(1).String()
		elevated := false
		if len(call.Arguments) > 2 {
			elevated = call.Argument(2).ToBoolean()
		}
		updateData := map[string]interface{}{}
		if elevated {
			updateData["impersonated"] = impersonate + " *"
		} else {
			updateData["impersonated"] = impersonate
		}
		_ = engine.manager.teamserver.AxAgentUpdateData(agentId, updateData)
		return goja.Undefined()
	})

	axObj.Set("agent_set_mark", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || engine.manager.teamserver == nil || len(call.Arguments) < 2 {
			return goja.Undefined()
		}
		ids := exportInt64Array(rt, call.Argument(0))
		mark := call.Argument(1).String()
		_ = engine.manager.teamserver.AxAgentSetMark(ids, mark)
		return goja.Undefined()
	})

	axObj.Set("agent_set_tag", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || engine.manager.teamserver == nil || len(call.Arguments) < 2 {
			return goja.Undefined()
		}
		ids := exportInt64Array(rt, call.Argument(0))
		tag := call.Argument(1).String()
		_ = engine.manager.teamserver.AxAgentSetTag(ids, tag)
		return goja.Undefined()
	})

	axObj.Set("agent_update_data", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || engine.manager.teamserver == nil || len(call.Arguments) < 2 {
			return goja.Undefined()
		}
		agentId := exportInt64(call.Argument(0))
		data := call.Argument(1).Export()
		if m, ok := data.(map[string]interface{}); ok {
			_ = engine.manager.teamserver.AxAgentUpdateData(agentId, m)
		}
		return goja.Undefined()
	})

	axObj.Set("agent_set_command_group", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || engine.manager.teamserver == nil || len(call.Arguments) < 3 {
			panic(rt.NewTypeError("agent_set_command_group requires (agentId, groupName, enabled)"))
		}
		agentId := exportInt64(call.Argument(0))
		groupId := call.Argument(1).String()
		enabled := call.Argument(2).ToBoolean()
		if err := engine.manager.teamserver.AxAgentSetCommandGroup(agentId, groupId, enabled); err != nil {
			panic(rt.NewGoError(err))
		}
		return goja.Undefined()
	})

	axObj.Set("agent_get_command_groups", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || engine.manager.teamserver == nil || len(call.Arguments) < 1 {
			return rt.ToValue([]interface{}{})
		}
		agentId := exportInt64(call.Argument(0))
		list, err := engine.manager.teamserver.AxAgentGetCommandGroups(agentId)
		if err != nil {
			return rt.ToValue([]interface{}{})
		}
		out := make([]interface{}, 0, len(list))
		for _, item := range list {
			out = append(out, item)
		}
		return rt.ToValue(out)
	})

	// --- Misc ---

	axObj.Set("ticks", func(call goja.FunctionCall) goja.Value {
		return rt.ToValue(time.Now().Unix())
	})

	axObj.Set("format_time", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) < 2 {
			return rt.ToValue("")
		}
		format := call.Argument(0).String()
		unixTime := call.Argument(1).ToInteger()
		t := time.Unix(unixTime, 0)
		// Convert Qt-style format to Go format
		goFmt := convertQtFormatToGo(format)
		return rt.ToValue(t.Format(goFmt))
	})

	axObj.Set("validate_command", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || len(call.Arguments) < 2 {
			return rt.ToValue(map[string]interface{}{"valid": false, "message": "missing arguments"})
		}
		agentId := exportInt64(call.Argument(0))
		cmdline := call.Argument(1).String()
		result, err := engine.manager.ValidateCommand(agentId, cmdline)
		if err != nil {
			return rt.ToValue(map[string]interface{}{"valid": false, "message": err.Error()})
		}
		return rt.ToValue(result)
	})

	axObj.Set("plugin_service_command", func(call goja.FunctionCall) goja.Value {
		return goja.Undefined()
	})

	axObj.Set("plugin_service_wait", func(call goja.FunctionCall) goja.Value {
		return rt.ToValue(map[string]interface{}{"ok": false, "error": "plugin_service_wait is a client-only stub"})
	})

	axObj.Set("plugin_agent_command", func(call goja.FunctionCall) goja.Value {
		return goja.Undefined()
	})

	axObj.Set("plugin_listener_command", func(call goja.FunctionCall) goja.Value {
		return goja.Undefined()
	})

	axObj.Set("show_message", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) >= 2 {
			engine.manager.teamserver.TsLogAdd(adaptix.LogStatusInfo, 0, "axscript_manager", "log", "[%s] Message: %s - %s", engine.name, call.Argument(0).String(), call.Argument(1).String())
		}
		return goja.Undefined()
	})

	axObj.Set("get_project", func(call goja.FunctionCall) goja.Value {
		return rt.ToValue("")
	})

	axObj.Set("downloads", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil {
			return rt.ToValue([]interface{}{})
		}
		return rt.ToValue(engine.manager.GetDownloads())
	})

	axObj.Set("screenshots", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil {
			return rt.ToValue([]interface{}{})
		}
		return rt.ToValue(engine.manager.GetScreenshots())
	})

	axObj.Set("tunnels", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil {
			return rt.ToValue([]interface{}{})
		}
		return rt.ToValue(engine.manager.GetTunnels())
	})

	axObj.Set("copy_to_clipboard", func(call goja.FunctionCall) goja.Value { return goja.Undefined() })

	registerAxUtilities(axObj, rt, engine)

	rt.Set("ax", axObj)
}

func exportStringArray(rt *goja.Runtime, val goja.Value) []string {
	if val == nil || goja.IsUndefined(val) || goja.IsNull(val) {
		return nil
	}
	exported := val.Export()
	if arr, ok := exported.([]interface{}); ok {
		var result []string
		for _, item := range arr {
			if s, ok := item.(string); ok {
				result = append(result, s)
			}
		}
		return result
	}
	return nil
}

func exportInt64(val goja.Value) int64 {
	if val == nil || goja.IsUndefined(val) || goja.IsNull(val) {
		return 0
	}
	switch v := val.Export().(type) {
	case int64:
		return v
	case int:
		return int64(v)
	case float64:
		if math.IsNaN(v) || math.IsInf(v, 0) {
			return 0
		}
		if v >= float64(maxSafeJSInt) || v <= -float64(maxSafeJSInt) {
			log.Printf("axscript: int64 precision loss converting %g — pass IDs > 2^53 as strings", v)
		}
		return int64(v)
	case string:
		n, _ := strconv.ParseInt(v, 10, 64)
		return n
	}
	return val.ToInteger()
}

func exportInt64Array(rt *goja.Runtime, val goja.Value) []int64 {
	if val == nil || goja.IsUndefined(val) || goja.IsNull(val) {
		return nil
	}
	exported := val.Export()
	if arr, ok := exported.([]interface{}); ok {
		var result []int64
		for _, item := range arr {
			switch v := item.(type) {
			case int64:
				result = append(result, v)
			case int:
				result = append(result, int64(v))
			case float64:
				if math.IsNaN(v) || math.IsInf(v, 0) {
					continue
				}
				if v >= float64(maxSafeJSInt) || v <= -float64(maxSafeJSInt) {
					log.Printf("axscript: int64 precision loss converting %g — pass IDs > 2^53 as strings", v)
				}
				result = append(result, int64(v))
			case string:
				if n, err := strconv.ParseInt(v, 10, 64); err == nil {
					result = append(result, n)
				}
			}
		}
		return result
	}
	return nil
}

func convertQtFormatToGo(qtFmt string) string {
	r := strings.NewReplacer(
		"yyyy", "2006", "yy", "06",
		"MM", "01", "M", "1",
		"dd", "02", "d", "2",
		"HH", "15", "hh", "03", "h", "3",
		"mm", "04", "m", "4",
		"ss", "05", "s", "5",
		"AP", "PM", "ap", "pm",
	)
	return r.Replace(qtFmt)
}

func registerAxUtilities(axObj *goja.Object, rt *goja.Runtime, engine *ScriptEngine) {

	axObj.Set("base64_encode", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) == 0 {
			return rt.ToValue("")
		}
		data, _ := jsBytes(call.Argument(0))
		return rt.ToValue(base64.StdEncoding.EncodeToString(data))
	})

	axObj.Set("base64_decode", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) == 0 {
			return rt.ToValue(rt.NewArrayBuffer(nil))
		}
		decoded, err := base64.StdEncoding.DecodeString(call.Argument(0).String())
		if err != nil {
			return rt.ToValue(rt.NewArrayBuffer(nil))
		}
		return rt.ToValue(rt.NewArrayBuffer(decoded))
	})

	axObj.Set("hex_encode", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) == 0 {
			return rt.ToValue("")
		}
		data, _ := jsBytes(call.Argument(0))
		return rt.ToValue(hex.EncodeToString(data))
	})

	axObj.Set("hex_decode", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) == 0 {
			return rt.ToValue(rt.NewArrayBuffer(nil))
		}
		decoded, err := hex.DecodeString(call.Argument(0).String())
		if err != nil {
			return rt.ToValue(rt.NewArrayBuffer(nil))
		}
		return rt.ToValue(rt.NewArrayBuffer(decoded))
	})

	axObj.Set("hash", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) < 3 {
			return rt.ToValue("")
		}
		algorithm := strings.ToLower(call.Argument(0).String())
		length := int(call.Argument(1).ToInteger())
		data, _ := jsBytes(call.Argument(2))

		var result string
		switch algorithm {
		case "md5":
			h := md5.Sum(data)
			result = hex.EncodeToString(h[:])
		case "sha1":
			h := sha1.Sum(data)
			result = hex.EncodeToString(h[:])
		case "sha256":
			h := sha256.Sum256(data)
			result = hex.EncodeToString(h[:])
		case "sha512":
			h := sha512.Sum512(data)
			result = hex.EncodeToString(h[:])
		default:
			result = ""
		}

		if length > 0 && length < len(result) {
			result = result[:length]
		}
		return rt.ToValue(result)
	})

	axObj.Set("random_string", func(call goja.FunctionCall) goja.Value {
		length := 8
		charset := "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
		if len(call.Arguments) > 0 {
			length = int(call.Argument(0).ToInteger())
			if length < 0 {
				length = 0
			}
			if length > 4096 {
				length = 4096
			}
		}
		if len(call.Arguments) > 1 {
			setName := call.Argument(1).String()
			switch setName {
			case "hex":
				charset = "0123456789abcdef"
			case "alpha":
				charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
			case "numeric":
				charset = "0123456789"
			case "lower":
				charset = "abcdefghijklmnopqrstuvwxyz"
			case "upper":
				charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			}
		}

		b := make([]byte, length)
		for i := range b {
			b[i] = charset[rand.Intn(len(charset))]
		}
		return rt.ToValue(string(b))
	})

	axObj.Set("random_int", func(call goja.FunctionCall) goja.Value {
		min := 0
		max := 100
		if len(call.Arguments) > 0 {
			min = int(call.Argument(0).ToInteger())
		}
		if len(call.Arguments) > 1 {
			max = int(call.Argument(1).ToInteger())
		}
		if max <= min {
			return rt.ToValue(min)
		}
		return rt.ToValue(min + rand.Intn(max-min))
	})

	axObj.Set("format_size", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) == 0 {
			return rt.ToValue("0 B")
		}
		size := call.Argument(0).ToInteger()
		return rt.ToValue(formatBytes(uint64(size)))
	})

	axObj.Set("get_commands", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || len(call.Arguments) == 0 {
			return rt.ToValue([]interface{}{})
		}
		agentId := exportInt64(call.Argument(0))
		names, err := engine.manager.GetCommandNames(agentId)
		if err != nil {
			return rt.ToValue([]interface{}{})
		}
		var result []interface{}
		for _, n := range names {
			result = append(result, n)
		}
		if result == nil {
			return rt.ToValue([]interface{}{})
		}
		return rt.ToValue(result)
	})

	axObj.Set("interfaces", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil {
			return rt.ToValue([]interface{}{})
		}
		addrs := engine.manager.GetInterfaces()
		var result []interface{}
		for _, a := range addrs {
			result = append(result, a)
		}
		if result == nil {
			return rt.ToValue([]interface{}{})
		}
		return rt.ToValue(result)
	})

	axObj.Set("prompt_confirm", func(call goja.FunctionCall) goja.Value {
		return rt.ToValue(false)
	})
	axObj.Set("prompt_open_file", func(call goja.FunctionCall) goja.Value {
		return rt.ToValue("")
	})
	axObj.Set("prompt_open_dir", func(call goja.FunctionCall) goja.Value {
		return rt.ToValue("")
	})
	axObj.Set("prompt_save_file", func(call goja.FunctionCall) goja.Value {
		return rt.ToValue("")
	})
	axObj.Set("clipboard_set", func(call goja.FunctionCall) goja.Value {
		return goja.Undefined()
	})
	axObj.Set("clipboard_get", func(call goja.FunctionCall) goja.Value {
		return rt.ToValue("")
	})

	axObj.Set("tokenize", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) == 0 {
			return rt.ToValue([]string{})
		}
		return rt.ToValue(Tokenize(call.Argument(0).String()))
	})

	axObj.Set("open_agent_console", func(call goja.FunctionCall) goja.Value { return goja.Undefined() })
	axObj.Set("open_access_tunnel", func(call goja.FunctionCall) goja.Value { return goja.Undefined() })
	axObj.Set("open_browser_files", func(call goja.FunctionCall) goja.Value { return goja.Undefined() })
	axObj.Set("open_browser_process", func(call goja.FunctionCall) goja.Value { return goja.Undefined() })
	axObj.Set("open_remote_terminal", func(call goja.FunctionCall) goja.Value { return goja.Undefined() })
	axObj.Set("open_remote_shell", func(call goja.FunctionCall) goja.Value { return goja.Undefined() })

	axObj.Set("convert_to_code", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) < 2 {
			return rt.ToValue("")
		}
		language := strings.ToLower(call.Argument(0).String())
		data, ok := jsBytes(call.Argument(1))
		if !ok {
			return rt.ToValue("")
		}
		varName := "shellcode"
		if len(call.Arguments) > 2 {
			varName = call.Argument(2).String()
		}
		return rt.ToValue(bytesToCode(language, data, varName))
	})

	axObj.Set("encode_data", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) < 2 {
			return rt.ToValue("")
		}
		alg := strings.ToLower(call.Argument(0).String())
		data, _ := jsBytes(call.Argument(1))
		key := ""
		if len(call.Arguments) > 2 {
			key = call.Argument(2).String()
		}
		text, raw, isText := encodeData(alg, data, key)
		if isText {
			return rt.ToValue(text)
		}
		return rt.ToValue(rt.NewArrayBuffer(raw))
	})

	axObj.Set("decode_data", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) < 2 {
			return rt.ToValue(rt.NewArrayBuffer(nil))
		}
		alg := strings.ToLower(call.Argument(0).String())
		key := ""
		if len(call.Arguments) > 2 {
			key = call.Argument(2).String()
		}

		var decoded []byte
		switch alg {
		case "base64":
			s := call.Argument(1).String()
			b, err := base64.StdEncoding.DecodeString(s)
			if err != nil {
				return rt.ToValue(rt.NewArrayBuffer(nil))
			}
			decoded = b
		case "hex":
			s := call.Argument(1).String()
			b, err := hex.DecodeString(s)
			if err != nil {
				return rt.ToValue(rt.NewArrayBuffer(nil))
			}
			decoded = b
		case "xor":
			raw, _ := jsBytes(call.Argument(1))
			decoded = xorBytes(raw, key)
		default:
			raw, _ := jsBytes(call.Argument(1))
			decoded = raw
		}
		return rt.ToValue(rt.NewArrayBuffer(decoded))
	})

	axObj.Set("encode_file", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || len(call.Arguments) < 2 {
			return rt.ToValue("")
		}
		alg := strings.ToLower(call.Argument(0).String())
		path := call.Argument(1).String()
		key := ""
		if len(call.Arguments) > 2 {
			key = call.Argument(2).String()
		}
		data, err := engine.manager.ReadFileSandboxed(engine, path)
		if err != nil {
			return rt.ToValue("")
		}
		text, raw, isText := encodeData(alg, data, key)
		if isText {
			return rt.ToValue(text)
		}
		return rt.ToValue(rt.NewArrayBuffer(raw))
	})

	axObj.Set("decode_file", func(call goja.FunctionCall) goja.Value {
		if engine.manager == nil || len(call.Arguments) < 2 {
			return rt.ToValue(rt.NewArrayBuffer(nil))
		}
		alg := strings.ToLower(call.Argument(0).String())
		path := call.Argument(1).String()
		key := ""
		if len(call.Arguments) > 2 {
			key = call.Argument(2).String()
		}
		rawData, err := engine.manager.ReadFileSandboxed(engine, path)
		if err != nil {
			return rt.ToValue(rt.NewArrayBuffer(nil))
		}

		var decoded []byte
		switch alg {
		case "base64":
			b, err := base64.StdEncoding.DecodeString(string(rawData))
			if err != nil {
				return rt.ToValue(rt.NewArrayBuffer(nil))
			}
			decoded = b
		case "hex":
			b, err := hex.DecodeString(strings.TrimSpace(string(rawData)))
			if err != nil {
				return rt.ToValue(rt.NewArrayBuffer(nil))
			}
			decoded = b
		case "xor":
			decoded = xorBytes(rawData, key)
		default:
			decoded = rawData
		}
		return rt.ToValue(rt.NewArrayBuffer(decoded))
	})
}

func parseBofTypes(types string) []string {
	trimmed := strings.TrimSpace(types)
	if trimmed == "" {
		return nil
	}

	if strings.Contains(trimmed, ",") {
		items := strings.Split(trimmed, ",")
		for i := range items {
			items[i] = strings.TrimSpace(items[i])
		}
		return items
	}

	switch trimmed {
	case "cstr", "wstr", "bytes", "int", "short":
		return []string{trimmed}
	}

	items := make([]string, 0, len(trimmed))
	for _, r := range trimmed {
		switch r {
		case 'z':
			items = append(items, "cstr")
		case 'Z':
			items = append(items, "wstr")
		case 'b':
			items = append(items, "bytes")
		case 'i':
			items = append(items, "int")
		case 's':
			items = append(items, "short")
		default:
			// unknown — pass through so caller reports the exact bad char
			items = append(items, string(r))
		}
	}
	return items
}

// /---
func formatBytes(b uint64) string {
	const unit = 1024
	if b < unit {
		return fmt.Sprintf("%d B", b)
	}
	div, exp := uint64(unit), 0
	for n := b / unit; n >= unit; n /= unit {
		div *= unit
		exp++
	}
	return fmt.Sprintf("%.1f %cB", float64(b)/float64(div), "KMGTPE"[exp])
}
