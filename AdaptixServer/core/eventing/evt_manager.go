package eventing

import (
	"context"
	"fmt"
	"math/rand/v2"
	"sort"
	"sync"
	"time"

	"AdaptixServer/core/utils/logs"
)

const (
	DefaultWorkerCount    = 4
	DefaultQueueSize      = 256
	DefaultHookTimeout    = 30 * time.Second
	DefaultPreHookTimeout = 5 * time.Second
)

type HookFunc func(event any) error

type Hook struct {
	ID       string
	Name     string
	Phase    HookPhase
	Priority int
	Timeout  time.Duration
	Handler  HookFunc
}

type eventTask struct {
	eventType EventType
	event     any
	hook      *Hook
}

type EventManager struct {
	hooks map[EventType][]*Hook
	mu    sync.RWMutex

	taskQueue  chan eventTask
	workerWg   sync.WaitGroup
	shutdownCh chan struct{}
	running    bool
}

func NewEventManager() *EventManager {
	em := &EventManager{
		hooks:      make(map[EventType][]*Hook),
		taskQueue:  make(chan eventTask, DefaultQueueSize),
		shutdownCh: make(chan struct{}),
		running:    true,
	}

	em.startWorkers(DefaultWorkerCount)
	return em
}

func (em *EventManager) startWorkers(count int) {
	for i := 0; i < count; i++ {
		em.workerWg.Add(1)
		go em.worker(i)
	}
}

func (em *EventManager) worker(id int) {
	defer em.workerWg.Done()

	for {
		select {
		case <-em.shutdownCh:
			return
		case task, ok := <-em.taskQueue:
			if !ok {
				return
			}
			_ = em.executeHookWithTimeout(task.hook, task.event, DefaultHookTimeout)
		}
	}
}

func (em *EventManager) executeHookWithTimeout(hook *Hook, event any, defaultTimeout time.Duration) error {
	timeout := hook.Timeout
	if timeout == 0 {
		timeout = defaultTimeout
	}

	ctx, cancel := context.WithTimeout(context.Background(), timeout)
	defer cancel()

	done := make(chan error, 1)
	go func() {
		defer func() {
			if r := recover(); r != nil {
				done <- fmt.Errorf("hook %s panicked: %v", hook.Name, r)
			}
		}()
		done <- hook.Handler(event)
	}()

	select {
	case err := <-done:
		return err
	case <-ctx.Done():
		logs.Warn("", fmt.Sprintf("Hook %s timed out after %v", hook.Name, timeout))
		return fmt.Errorf("hook %s timed out", hook.Name)
	}
}

func (em *EventManager) Shutdown() {
	em.mu.Lock()
	if !em.running {
		em.mu.Unlock()
		return
	}
	em.running = false
	em.mu.Unlock()

	close(em.shutdownCh)
	close(em.taskQueue)
	em.workerWg.Wait()
}

func (em *EventManager) Register(eventType EventType, hook *Hook) string {
	em.mu.Lock()
	defer em.mu.Unlock()

	if hook.ID == "" {
		hook.ID = em.generateUniqueID()
	}

	em.hooks[eventType] = append(em.hooks[eventType], hook)

	sort.Slice(em.hooks[eventType], func(i, j int) bool {
		return em.hooks[eventType][i].Priority < em.hooks[eventType][j].Priority
	})

	return hook.ID
}

func (em *EventManager) generateUniqueID() string {
	for {
		id := fmt.Sprintf("%08x", rand.Uint32())
		if !em.hookIDExists(id) {
			return id
		}
	}
}

func (em *EventManager) hookIDExists(id string) bool {
	for _, hooks := range em.hooks {
		for _, hook := range hooks {
			if hook.ID == id {
				return true
			}
		}
	}
	return false
}

func (em *EventManager) Unregister(hookID string) bool {
	em.mu.Lock()
	defer em.mu.Unlock()

	for eventType, hooks := range em.hooks {
		for i, hook := range hooks {
			if hook.ID == hookID {
				em.hooks[eventType] = append(hooks[:i], hooks[i+1:]...)
				return true
			}
		}
	}
	return false
}

func (em *EventManager) UnregisterByName(name string) int {
	em.mu.Lock()
	defer em.mu.Unlock()

	count := 0
	for eventType, hooks := range em.hooks {
		filtered := make([]*Hook, 0, len(hooks))
		for _, hook := range hooks {
			if hook.Name != name {
				filtered = append(filtered, hook)
			} else {
				count++
			}
		}
		em.hooks[eventType] = filtered
	}
	return count
}

func (em *EventManager) Emit(eventType EventType, phase HookPhase, event any) bool {
	em.mu.RLock()
	hooks := make([]*Hook, len(em.hooks[eventType]))
	copy(hooks, em.hooks[eventType])
	em.mu.RUnlock()

	baseEvent := getBaseEvent(event)
	if baseEvent != nil {
		baseEvent.Type = eventType
		baseEvent.Phase = phase
	}

	for _, hook := range hooks {
		if hook.Phase != phase {
			continue
		}

		var err error
		if phase == HookPre {
			err = em.executeHookWithTimeout(hook, event, DefaultPreHookTimeout)
		} else {
			err = em.executeHookWithTimeout(hook, event, DefaultHookTimeout)
		}

		if err != nil {
			if baseEvent != nil {
				baseEvent.Error = err
				if phase == HookPre {
					baseEvent.Cancelled = true
					return false
				}
			}
			logs.Warn("", fmt.Sprintf("Hook %s error: %v", hook.Name, err))
		}

		if baseEvent != nil && phase == HookPre && baseEvent.Cancelled {
			return false
		}
	}

	return true
}

func (em *EventManager) EmitAsync(eventType EventType, event any) {
	em.mu.RLock()
	hooks := make([]*Hook, len(em.hooks[eventType]))
	copy(hooks, em.hooks[eventType])
	running := em.running
	em.mu.RUnlock()

	if !running {
		return
	}

	baseEvent := getBaseEvent(event)
	if baseEvent != nil {
		baseEvent.Type = eventType
		baseEvent.Phase = HookPost
	}

	for _, hook := range hooks {
		if hook.Phase != HookPost {
			continue
		}

		select {
		case em.taskQueue <- eventTask{
			eventType: eventType,
			event:     event,
			hook:      hook,
		}:
		default:
			logs.Warn("", fmt.Sprintf("Event queue full, dropping hook %s for event %s", hook.Name, eventType))
		}
	}
}

func (em *EventManager) ListHooks(eventType EventType) []Hook {
	em.mu.RLock()
	defer em.mu.RUnlock()

	result := make([]Hook, 0, len(em.hooks[eventType]))
	for _, hook := range em.hooks[eventType] {
		result = append(result, *hook)
	}
	return result
}

func (em *EventManager) ListAllHooks() map[EventType][]Hook {
	em.mu.RLock()
	defer em.mu.RUnlock()

	result := make(map[EventType][]Hook)
	for eventType, hooks := range em.hooks {
		list := make([]Hook, 0, len(hooks))
		for _, hook := range hooks {
			list = append(list, *hook)
		}
		result[eventType] = list
	}
	return result
}

func getBaseEvent(event any) *BaseEvent {
	switch e := event.(type) {
	case *EventCredentialsAdd:
		return &e.BaseEvent
	case *EventCredentialsEdit:
		return &e.BaseEvent
	case *EventCredentialsRemove:
		return &e.BaseEvent
	case *EventDataAgentNew:
		return &e.BaseEvent
	case *EventDataAgentCheckin:
		return &e.BaseEvent
	case *EventDataAgentTerminate:
		return &e.BaseEvent
	case *EventDataAgentUpdate:
		return &e.BaseEvent
	case *EventDataTaskCreate:
		return &e.BaseEvent
	case *EventDataTaskStart:
		return &e.BaseEvent
	case *EventDataTaskComplete:
		return &e.BaseEvent
	case *EventDataListenerStart:
		return &e.BaseEvent
	case *EventDataListenerStop:
		return &e.BaseEvent
	case *EventDataDownloadStart:
		return &e.BaseEvent
	case *EventDataDownloadFinish:
		return &e.BaseEvent
	case *EventDataScreenshotAdd:
		return &e.BaseEvent
	case *EventDataTunnelStart:
		return &e.BaseEvent
	case *EventDataTunnelStop:
		return &e.BaseEvent
	case *EventDataClientConnect:
		return &e.BaseEvent
	case *EventDataClientDisconnect:
		return &e.BaseEvent
	case *EventDataTargetAdd:
		return &e.BaseEvent
	case *EventDataTargetRemove:
		return &e.BaseEvent
	case *EventDataPivotCreate:
		return &e.BaseEvent
	case *EventDataPivotRemove:
		return &e.BaseEvent
	default:
		return nil
	}
}
