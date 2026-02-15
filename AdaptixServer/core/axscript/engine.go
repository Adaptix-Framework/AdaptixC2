package axscript

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"sync"

	"github.com/dop251/goja"
)

type ScriptEngine struct {
	mu      sync.Mutex
	runtime *goja.Runtime
	name    string

	manager      *ScriptManager
	functions    map[string]goja.Callable
	scriptPath   string   // absolute path to the .axs file
	scriptDir    string   // directory containing the .axs file (with trailing /)
	allowedRoots []string // resolved absolute paths allowed for file access
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

	resolved, err := resolveRealPath(abs)
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

func (e *ScriptEngine) ScriptDir() string {
	return e.scriptDir
}

func (e *ScriptEngine) ScriptPath() string {
	return e.scriptPath
}

func (e *ScriptEngine) ValidatePath(path string) (string, error) {
	abs, err := filepath.Abs(path)
	if err != nil {
		return "", fmt.Errorf("invalid path: %w", err)
	}

	real, err := resolveRealPath(abs)
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

func resolveRealPath(path string) (string, error) {
	info, err := os.Lstat(path)
	if err != nil {
		return filepath.Clean(path), nil
	}

	if info.Mode()&os.ModeSymlink != 0 {
		resolved, err := filepath.EvalSymlinks(path)
		if err != nil {
			return "", err
		}
		return filepath.Clean(resolved), nil
	}

	return filepath.Clean(path), nil
}

func (e *ScriptEngine) Name() string {
	return e.name
}

func (e *ScriptEngine) Runtime() *goja.Runtime {
	return e.runtime
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

func (e *ScriptEngine) StoreFunctionRef(id string, fn goja.Callable) {
	e.mu.Lock()
	defer e.mu.Unlock()
	e.functions[id] = fn
}

func (e *ScriptEngine) GetFunctionRef(id string) (goja.Callable, bool) {
	e.mu.Lock()
	defer e.mu.Unlock()
	fn, ok := e.functions[id]
	return fn, ok
}

func (e *ScriptEngine) RemoveFunctionRef(id string) {
	e.mu.Lock()
	defer e.mu.Unlock()
	delete(e.functions, id)
}

func (e *ScriptEngine) ToValue(v interface{}) goja.Value {
	return e.runtime.ToValue(v)
}

func (e *ScriptEngine) SetGlobal(name string, value interface{}) {
	e.runtime.Set(name, value)
}
