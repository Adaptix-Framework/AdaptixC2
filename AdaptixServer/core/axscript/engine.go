package axscript

import (
	"AdaptixServer/core/utils/fsystem"
	"fmt"
	"path/filepath"
	"strings"
	"sync"

	"github.com/dop251/goja"
)

type ScriptEngine struct {
	mu      sync.Mutex
	runtime *goja.Runtime
	name    string

	manager       *ScriptManager
	functions     map[string]goja.Callable
	scriptPath    string
	scriptDir     string
	allowedRoots  []string
	importedFiles []string
}

func NewScriptEngine(name string, manager *ScriptManager) *ScriptEngine {
	rt := goja.New()
	rt.SetFieldNameMapper(goja.UncapFieldNameMapper())

	engine := &ScriptEngine{
		runtime:   rt,
		name:      name,
		manager:   manager,
		functions: make(map[string]goja.Callable),
	}

	consoleObj := rt.NewObject()
	consoleObj.Set("log", func(call goja.FunctionCall) goja.Value { return goja.Undefined() })
	consoleObj.Set("warn", func(call goja.FunctionCall) goja.Value { return goja.Undefined() })
	consoleObj.Set("error", func(call goja.FunctionCall) goja.Value { return goja.Undefined() })
	consoleObj.Set("info", func(call goja.FunctionCall) goja.Value { return goja.Undefined() })
	consoleObj.Set("debug", func(call goja.FunctionCall) goja.Value { return goja.Undefined() })
	rt.Set("console", consoleObj)

	return engine
}

func NewScriptEngineFromPath(scriptPath string, manager *ScriptManager) (*ScriptEngine, error) {
	abs, err := filepath.Abs(scriptPath)
	if err != nil {
		return nil, fmt.Errorf("invalid script path: %w", err)
	}

	resolved, err := fsystem.ResolveRealPath(abs)
	if err != nil {
		return nil, fmt.Errorf("cannot resolve script path: %w", err)
	}

	dir := filepath.Dir(resolved)

	engine := NewScriptEngine(resolved, manager)
	engine.scriptPath = resolved
	engine.scriptDir = dir + "/"
	engine.allowedRoots = []string{dir}

	if manager != nil {
		for _, root := range manager.globalAllowedRoots {
			engine.allowedRoots = append(engine.allowedRoots, root)
		}
	}

	return engine, nil
}

func (e *ScriptEngine) Execute(script string) error {
	e.mu.Lock()
	defer e.mu.Unlock()

	_, err := e.runtime.RunString(script)
	if err != nil {
		return fmt.Errorf("script execution error in '%s': %w", e.name, err)
	}
	return nil
}

//////////

// /---
func (e *ScriptEngine) ScriptDir() string {
	return e.scriptDir
}

// /---
func (e *ScriptEngine) ScriptPath() string {
	return e.scriptPath
}

func (e *ScriptEngine) ValidatePath(path string) (string, error) {
	abs, err := filepath.Abs(path)
	if err != nil {
		return "", fmt.Errorf("invalid path: %w", err)
	}

	real, err := fsystem.ResolveRealPath(abs)
	if err != nil {
		return "", fmt.Errorf("cannot resolve path: %w", err)
	}

	for _, root := range e.allowedRoots {
		if strings.HasPrefix(real, root+"/") || real == root {
			return real, nil
		}
	}

	return "", fmt.Errorf("access denied: %s is outside allowed directories", path)
}

func (e *ScriptEngine) AddImportedFile(path string) {
	for _, p := range e.importedFiles {
		if p == path {
			return
		}
	}
	e.importedFiles = append(e.importedFiles, path)
}

func (e *ScriptEngine) GetImportedFiles() []string {
	result := make([]string, len(e.importedFiles))
	copy(result, e.importedFiles)
	return result
}

// /---
func (e *ScriptEngine) Name() string {
	return e.name
}

// /---
func (e *ScriptEngine) Runtime() *goja.Runtime {
	return e.runtime
}

// /---
func (e *ScriptEngine) CallFunction(name string, args ...goja.Value) (goja.Value, error) {
	e.mu.Lock()
	defer e.mu.Unlock()

	fn, ok := goja.AssertFunction(e.runtime.Get(name))
	if !ok {
		return nil, fmt.Errorf("function '%s' not found or not callable in engine '%s'", name, e.name)
	}

	result, err := fn(goja.Undefined(), args...)
	if err != nil {
		return nil, fmt.Errorf("error calling '%s' in engine '%s': %w", name, e.name, err)
	}
	return result, nil
}

func (e *ScriptEngine) CallCallable(fn goja.Callable, args ...goja.Value) (goja.Value, error) {
	e.mu.Lock()
	defer e.mu.Unlock()

	result, err := fn(goja.Undefined(), args...)
	if err != nil {
		return nil, fmt.Errorf("error calling callable in engine '%s': %w", e.name, err)
	}
	return result, nil
}

// /---
func (e *ScriptEngine) StoreFunctionRef(id string, fn goja.Callable) {
	e.mu.Lock()
	defer e.mu.Unlock()
	e.functions[id] = fn
}

// /---
func (e *ScriptEngine) GetFunctionRef(id string) (goja.Callable, bool) {
	e.mu.Lock()
	defer e.mu.Unlock()
	fn, ok := e.functions[id]
	return fn, ok
}

// /---
func (e *ScriptEngine) RemoveFunctionRef(id string) {
	e.mu.Lock()
	defer e.mu.Unlock()
	delete(e.functions, id)
}

func (e *ScriptEngine) ToValue(v interface{}) goja.Value {
	return e.runtime.ToValue(v)
}

// /---
func (e *ScriptEngine) SetGlobal(name string, value interface{}) {
	e.runtime.Set(name, value)
}

func (e *ScriptEngine) GetMetadataName() string {
	metaVal := e.runtime.Get("metadata")
	if metaVal == nil || goja.IsUndefined(metaVal) || goja.IsNull(metaVal) {
		return ""
	}
	metaObj := metaVal.ToObject(e.runtime)
	if metaObj == nil {
		return ""
	}
	nameVal := metaObj.Get("name")
	if nameVal == nil || goja.IsUndefined(nameVal) || goja.IsNull(nameVal) {
		return ""
	}
	return nameVal.String()
}

func (e *ScriptEngine) GetMetadataNoSave() bool {
	metaVal := e.runtime.Get("metadata")
	if metaVal == nil || goja.IsUndefined(metaVal) || goja.IsNull(metaVal) {
		return false
	}
	metaObj := metaVal.ToObject(e.runtime)
	if metaObj == nil {
		return false
	}
	nosaveVal := metaObj.Get("nosave")
	if nosaveVal == nil || goja.IsUndefined(nosaveVal) || goja.IsNull(nosaveVal) {
		return false
	}
	return nosaveVal.ToBoolean()
}
