package axscript

import (
	"fmt"

	"github.com/dop251/goja"
)

type jsCommandBuilder struct {
	engine  *ScriptEngine
	command CommandDef
}

func newJsCommandBuilder(engine *ScriptEngine, name, description, example, message string) *jsCommandBuilder {
	return &jsCommandBuilder{
		engine: engine,
		command: CommandDef{
			Name:        name,
			Description: description,
			Example:     example,
			Message:     message,
		},
	}
}

func (b *jsCommandBuilder) AddArgBool(call goja.FunctionCall) goja.Value {
	flag := call.Argument(0).String()
	arg := ArgumentDef{
		Type: ArgTypeBool,
		Flag: true,
		Mark: flag,
	}

	if len(call.Arguments) > 1 {
		arg.Description = call.Argument(1).String()
	}
	if len(call.Arguments) > 2 && !goja.IsUndefined(call.Argument(2)) && !goja.IsNull(call.Argument(2)) {
		arg.DefaultUsed = true
		arg.DefaultValue = call.Argument(2).Export()
	}

	b.command.Args = append(b.command.Args, arg)
	return goja.Undefined()
}

func (b *jsCommandBuilder) AddArgInt(call goja.FunctionCall) goja.Value {
	name := call.Argument(0).String()
	arg := ArgumentDef{
		Type: ArgTypeInt,
		Name: name,
	}

	if len(call.Arguments) > 1 {
		v := call.Argument(1)
		if v.ExportType() != nil && v.ExportType().Kind().String() == "bool" {
			arg.Required = v.ToBoolean()
			if len(call.Arguments) > 2 {
				arg.Description = call.Argument(2).String()
			}
		} else {
			arg.Required = true
			arg.Description = v.String()
			if len(call.Arguments) > 2 && !goja.IsUndefined(call.Argument(2)) && !goja.IsNull(call.Argument(2)) {
				arg.DefaultUsed = true
				arg.DefaultValue = call.Argument(2).Export()
			}
		}
	}

	b.command.Args = append(b.command.Args, arg)
	return goja.Undefined()
}

func (b *jsCommandBuilder) AddArgFlagInt(call goja.FunctionCall) goja.Value {
	flag := call.Argument(0).String()
	name := call.Argument(1).String()
	arg := ArgumentDef{
		Type: ArgTypeInt,
		Name: name,
		Flag: true,
		Mark: flag,
	}

	if len(call.Arguments) > 2 {
		v := call.Argument(2)
		if v.ExportType() != nil && v.ExportType().Kind().String() == "bool" {
			arg.Required = v.ToBoolean()
			if len(call.Arguments) > 3 {
				arg.Description = call.Argument(3).String()
			}
		} else {
			arg.Required = true
			arg.Description = v.String()
			if len(call.Arguments) > 3 && !goja.IsUndefined(call.Argument(3)) && !goja.IsNull(call.Argument(3)) {
				arg.DefaultUsed = true
				arg.DefaultValue = call.Argument(3).Export()
			}
		}
	}

	b.command.Args = append(b.command.Args, arg)
	return goja.Undefined()
}

func (b *jsCommandBuilder) AddArgString(call goja.FunctionCall) goja.Value {
	name := call.Argument(0).String()
	arg := ArgumentDef{
		Type:     ArgTypeString,
		Name:     name,
		Required: true,
	}

	if len(call.Arguments) > 1 {
		v := call.Argument(1)
		if v.ExportType() != nil && v.ExportType().Kind().String() == "bool" {
			arg.Required = v.ToBoolean()
			if len(call.Arguments) > 2 {
				arg.Description = call.Argument(2).String()
			}
		} else {
			arg.Description = v.String()
			if len(call.Arguments) > 2 && !goja.IsUndefined(call.Argument(2)) && !goja.IsNull(call.Argument(2)) {
				arg.DefaultUsed = true
				arg.DefaultValue = call.Argument(2).Export()
			}
		}
	}

	b.command.Args = append(b.command.Args, arg)
	return goja.Undefined()
}

func (b *jsCommandBuilder) AddArgFlagString(call goja.FunctionCall) goja.Value {
	flag := call.Argument(0).String()
	name := call.Argument(1).String()
	arg := ArgumentDef{
		Type: ArgTypeString,
		Name: name,
		Flag: true,
		Mark: flag,
	}

	if len(call.Arguments) > 2 {
		v := call.Argument(2)
		if v.ExportType() != nil && v.ExportType().Kind().String() == "bool" {
			arg.Required = v.ToBoolean()
			if len(call.Arguments) > 3 {
				arg.Description = call.Argument(3).String()
			}
		} else {
			arg.Required = true
			arg.Description = v.String()
			if len(call.Arguments) > 3 && !goja.IsUndefined(call.Argument(3)) && !goja.IsNull(call.Argument(3)) {
				arg.DefaultUsed = true
				arg.DefaultValue = call.Argument(3).Export()
			}
		}
	}

	b.command.Args = append(b.command.Args, arg)
	return goja.Undefined()
}

func (b *jsCommandBuilder) AddArgFile(call goja.FunctionCall) goja.Value {
	name := call.Argument(0).String()
	required := false
	description := ""
	if len(call.Arguments) > 1 {
		required = call.Argument(1).ToBoolean()
	}
	if len(call.Arguments) > 2 {
		description = call.Argument(2).String()
	}

	arg := ArgumentDef{
		Type:        ArgTypeFile,
		Name:        name,
		Required:    required,
		Description: description,
	}
	b.command.Args = append(b.command.Args, arg)
	return goja.Undefined()
}

func (b *jsCommandBuilder) AddArgFlagFile(call goja.FunctionCall) goja.Value {
	flag := call.Argument(0).String()
	name := call.Argument(1).String()
	required := false
	description := ""
	if len(call.Arguments) > 2 {
		required = call.Argument(2).ToBoolean()
	}
	if len(call.Arguments) > 3 {
		description = call.Argument(3).String()
	}

	arg := ArgumentDef{
		Type:        ArgTypeFile,
		Name:        name,
		Required:    required,
		Flag:        true,
		Mark:        flag,
		Description: description,
	}
	b.command.Args = append(b.command.Args, arg)
	return goja.Undefined()
}

func (b *jsCommandBuilder) AddSubCommands(call goja.FunctionCall) goja.Value {
	val := call.Argument(0)
	if goja.IsUndefined(val) || goja.IsNull(val) {
		return goja.Undefined()
	}

	rt := b.engine.runtime
	obj := val.ToObject(rt)

	if isArray, _ := isJsArray(rt, obj); isArray {
		length := obj.Get("length").ToInteger()
		for i := int64(0); i < length; i++ {
			item := obj.Get(fmt.Sprintf("%d", i))
			if sub := extractCommandBuilder(item); sub != nil {
				b.command.Subcommands = append(b.command.Subcommands, sub.command)
			}
		}
	} else {
		if sub := extractCommandBuilder(val); sub != nil {
			b.command.Subcommands = append(b.command.Subcommands, sub.command)
		}
	}

	return goja.Undefined()
}

func (b *jsCommandBuilder) SetPreHook(call goja.FunctionCall) goja.Value {
	fn, ok := goja.AssertFunction(call.Argument(0))
	if !ok {
		panic(b.engine.runtime.NewTypeError("setPreHook: argument is not a function"))
	}
	b.command.HasPreHook = true
	b.command.PreHookFunc = fn
	return goja.Undefined()
}

type jsCommandGroupBuilder struct {
	engine   *ScriptEngine
	name     string
	commands []CommandDef
}

func newJsCommandGroupBuilder(engine *ScriptEngine) *jsCommandGroupBuilder {
	return &jsCommandGroupBuilder{
		engine: engine,
	}
}

func (g *jsCommandGroupBuilder) SetParams(call goja.FunctionCall) goja.Value {
	g.name = call.Argument(0).String()

	val := call.Argument(1)
	if goja.IsUndefined(val) || goja.IsNull(val) {
		return goja.Undefined()
	}

	rt := g.engine.runtime
	obj := val.ToObject(rt)
	if isArr, _ := isJsArray(rt, obj); !isArr {
		panic(rt.NewTypeError("setParams: second argument must be an array"))
	}

	length := obj.Get("length").ToInteger()
	for i := int64(0); i < length; i++ {
		item := obj.Get(fmt.Sprintf("%d", i))
		if sub := extractCommandBuilder(item); sub != nil {
			g.commands = append(g.commands, sub.command)
		}
	}

	return goja.Undefined()
}

func (g *jsCommandGroupBuilder) SetParamsFromValue(name string, val goja.Value) {
	g.name = name
	if goja.IsUndefined(val) || goja.IsNull(val) {
		return
	}
	rt := g.engine.runtime
	obj := val.ToObject(rt)
	lengthVal := obj.Get("length")
	if lengthVal == nil || goja.IsUndefined(lengthVal) {
		return
	}
	length := lengthVal.ToInteger()
	for i := int64(0); i < length; i++ {
		item := obj.Get(fmt.Sprintf("%d", i))
		if sub := extractCommandBuilder(item); sub != nil {
			g.commands = append(g.commands, sub.command)
		}
	}
}

func (g *jsCommandGroupBuilder) Add(call goja.FunctionCall) goja.Value {
	val := call.Argument(0)
	if goja.IsUndefined(val) || goja.IsNull(val) {
		return goja.Undefined()
	}

	rt := g.engine.runtime
	obj := val.ToObject(rt)
	if isArr, _ := isJsArray(rt, obj); isArr {
		length := obj.Get("length").ToInteger()
		for i := int64(0); i < length; i++ {
			item := obj.Get(fmt.Sprintf("%d", i))
			if sub := extractCommandBuilder(item); sub != nil {
				g.commands = append(g.commands, sub.command)
			}
		}
	} else {
		if sub := extractCommandBuilder(val); sub != nil {
			g.commands = append(g.commands, sub.command)
		}
	}

	return goja.Undefined()
}

func (g *jsCommandGroupBuilder) ToCommandGroup(scriptName string) CommandGroup {
	return CommandGroup{
		GroupName:  g.name,
		ScriptName: scriptName,
		Commands:   g.commands,
	}
}

/// Helpers

func isJsArray(rt *goja.Runtime, obj *goja.Object) (bool, error) {
	arrayConstructor := rt.Get("Array")
	if arrayConstructor == nil {
		return false, nil
	}
	isArrayFn, ok := goja.AssertFunction(arrayConstructor.ToObject(rt).Get("isArray"))
	if !ok {
		return false, nil
	}
	result, err := isArrayFn(goja.Undefined(), obj)
	if err != nil {
		return false, err
	}
	return result.ToBoolean(), nil
}

func extractCommandBuilder(val goja.Value) *jsCommandBuilder {
	if goja.IsUndefined(val) || goja.IsNull(val) {
		return nil
	}
	exported := val.Export()
	if cb, ok := exported.(*jsCommandBuilder); ok {
		return cb
	}
	if m, ok := exported.(map[string]interface{}); ok {
		if builderVal, exists := m["__builder"]; exists {
			if cb, ok2 := builderVal.(*jsCommandBuilder); ok2 {
				return cb
			}
		}
	}
	obj := val.ToObject(nil)
	if obj != nil {
		builderVal := obj.Get("__builder")
		if builderVal != nil && !goja.IsUndefined(builderVal) && !goja.IsNull(builderVal) {
			if cb, ok := builderVal.Export().(*jsCommandBuilder); ok {
				return cb
			}
		}
	}
	return nil
}
