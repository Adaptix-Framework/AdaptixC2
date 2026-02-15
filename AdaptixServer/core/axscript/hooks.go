package axscript

import (
	"AdaptixServer/core/utils/krypt"
	"sync"

	"github.com/dop251/goja"
)

type HookStore struct {
	mu sync.RWMutex

	pendingPostHooks map[string]*PendingHook
	pendingHandlers  map[string]*PendingHook
}

func NewHookStore() *HookStore {
	hs := &HookStore{
		pendingPostHooks: make(map[string]*PendingHook),
		pendingHandlers:  make(map[string]*PendingHook),
	}

	return hs
}

func (hs *HookStore) RegisterPostHook(engine *ScriptEngine, fn goja.Callable, agentId string, client string) string {
	hs.mu.Lock()
	defer hs.mu.Unlock()

	id, _ := krypt.GenerateUID(8)

	hs.pendingPostHooks[id] = &PendingHook{
		ID:      id,
		Engine:  engine,
		Func:    fn,
		AgentID: agentId,
		Client:  client,
	}

	return id
}

func (hs *HookStore) RegisterHandler(engine *ScriptEngine, fn goja.Callable, agentId string, client string) string {
	hs.mu.Lock()
	defer hs.mu.Unlock()

	id, _ := krypt.GenerateUID(8)

	hs.pendingHandlers[id] = &PendingHook{
		ID:      id,
		Engine:  engine,
		Func:    fn,
		AgentID: agentId,
		Client:  client,
	}

	return id
}

func (hs *HookStore) GetPostHook(hookId string) *PendingHook {
	hs.mu.RLock()
	defer hs.mu.RUnlock()

	return hs.pendingPostHooks[hookId]
}

func (hs *HookStore) GetHandler(handlerId string) *PendingHook {
	hs.mu.RLock()
	defer hs.mu.RUnlock()

	return hs.pendingHandlers[handlerId]
}

func (hs *HookStore) RemovePostHook(hookId string) {
	hs.mu.Lock()
	defer hs.mu.Unlock()

	delete(hs.pendingPostHooks, hookId)
}

func (hs *HookStore) RemoveHandler(handlerId string) {
	hs.mu.Lock()
	defer hs.mu.Unlock()

	delete(hs.pendingHandlers, handlerId)
}

func (hs *HookStore) ExecutePostHook(hookId string, data map[string]interface{}) (map[string]interface{}, error) {
	hook := hs.GetPostHook(hookId)
	if hook == nil {
		return data, nil
	}

	result, err := hook.Engine.CallCallable(hook.Func, hook.Engine.ToValue(data))
	if err != nil {
		return data, err
	}

	if result != nil && !goja.IsUndefined(result) && !goja.IsNull(result) {
		if exported, ok := result.Export().(map[string]interface{}); ok {
			for k, v := range exported {
				data[k] = v
			}
		}
	}

	return data, nil
}

func (hs *HookStore) ExecuteHandler(handlerId string, data map[string]interface{}) error {
	hook := hs.GetHandler(handlerId)
	if hook == nil {
		return nil
	}

	_, err := hook.Engine.CallCallable(hook.Func, hook.Engine.ToValue(data))

	hs.RemoveHandler(handlerId)

	return err
}

func (hs *HookStore) IsServerHook(id string) bool {
	hs.mu.RLock()
	defer hs.mu.RUnlock()

	if _, ok := hs.pendingPostHooks[id]; ok {
		return true
	}
	if _, ok := hs.pendingHandlers[id]; ok {
		return true
	}
	return false
}
