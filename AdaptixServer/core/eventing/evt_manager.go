package eventing

import (
	"context"
	"encoding/json"
	"fmt"
	"math/rand/v2"
	"reflect"
	"sort"
	"strings"
	"sync"
	"time"

	"github.com/Adaptix-Framework/axc2/v2"
)

const (
	DefaultWorkerCount    = 4
	DefaultQueueSize      = 256
	DefaultHookTimeout    = 30 * time.Second
	DefaultPreHookTimeout = 5 * time.Second

	maxPostSequentialFallback = 3
	postSequentialTimeout     = 2 * time.Second
)

type Teamserver interface {
	TsLogAdd(status adaptix.LogStatus, level int, source, category string, format string, args ...any)
}

type HookFunc func(event any) error

type Hook struct {
	ID        string
	Name      string
	Phase     HookPhase
	Priority  int
	Timeout   time.Duration
	Handler   HookFunc
	Disabled  bool
	Source    string
	Filter    *EventFilter
	Interrupt func()
}

type HookInfo struct {
	ID       string `json:"id"`
	Name     string `json:"name"`
	Event    string `json:"event"`
	Phase    string `json:"phase"`
	Priority int    `json:"priority"`
	Enabled  bool   `json:"enabled"`
	Source   string `json:"source,omitempty"`
}

type eventTask struct {
	eventType EventType
	event     any
	hook      *Hook
}

type EventManager struct {
	ts    Teamserver
	hooks map[EventType][]*Hook
	mu    sync.RWMutex

	mutedEvents   map[EventType]bool
	factsEnricher FactsEnricher

	taskQueue  chan eventTask
	workerWg   sync.WaitGroup
	shutdownCh chan struct{}
	running    bool
}

func NewEventManager(ts Teamserver) *EventManager {
	em := &EventManager{
		ts:          ts,
		hooks:       make(map[EventType][]*Hook),
		mutedEvents: make(map[EventType]bool),
		taskQueue:   make(chan eventTask, DefaultQueueSize),
		shutdownCh:  make(chan struct{}),
		running:     true,
	}

	em.startWorkers(DefaultWorkerCount)
	return em
}

func (em *EventManager) SetFactsEnricher(fn FactsEnricher) {
	em.mu.Lock()
	em.factsEnricher = fn
	em.mu.Unlock()
}

func (em *EventManager) matchHook(hook *Hook, event any) bool {
	if hook == nil || hook.Filter == nil || hook.Filter.IsEmpty() {
		return true
	}
	facts := ExtractFacts(event)
	em.mu.RLock()
	enricher := em.factsEnricher
	em.mu.RUnlock()
	if enricher != nil {
		enricher(&facts, event)
	}
	return hook.Filter.MatchEvent(event, facts)
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
			if err := em.executeHookWithTimeout(task.hook, task.event, task.eventType, DefaultHookTimeout); err != nil {
				em.ts.TsLogAdd(adaptix.LogStatusWarn, 0, "events", string(task.eventType), "hook %s failed: %v", task.hook.Name, err)
			}
		}
	}
}

func (em *EventManager) executeHookWithTimeout(hook *Hook, event any, eventType EventType, defaultTimeout time.Duration) error {
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
				select {
				case done <- fmt.Errorf("hook %s panicked: %v", hook.Name, r):
				default:
				}
			}
		}()

		err := hook.Handler(event)
		select {
		case done <- err:
		case <-ctx.Done():
		}
	}()

	select {
	case err := <-done:
		return err
	case <-ctx.Done():
		if hook.Interrupt != nil {
			func() {
				defer func() { _ = recover() }()
				hook.Interrupt()
			}()
		}
		select {
		case err := <-done:
			if err != nil {
				em.ts.TsLogAdd(adaptix.LogStatusWarn, 0, "events", string(eventType), "hook %s timed out after %v: %v", hook.Name, timeout, err)
				return fmt.Errorf("hook %s timed out: %w", hook.Name, err)
			}
			em.ts.TsLogAdd(adaptix.LogStatusWarn, 0, "events", string(eventType), "hook %s timed out after %v", hook.Name, timeout)
			return fmt.Errorf("hook %s timed out", hook.Name)
		case <-time.After(500 * time.Millisecond):
			em.ts.TsLogAdd(adaptix.LogStatusWarn, 0, "events", string(eventType), "hook %s timed out after %v (still running)", hook.Name, timeout)
			return fmt.Errorf("hook %s timed out", hook.Name)
		}
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
	em.workerWg.Wait()
}

func (em *EventManager) Register(eventType EventType, hook *Hook) string {
	em.mu.Lock()
	defer em.mu.Unlock()

	if hook.ID == "" {
		hook.ID = em.generateUniqueID()
	}
	if hook.Source == "" && !strings.HasPrefix(hook.Name, "axscript:") {
		hook.Source = "extender"
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
	muted := em.mutedEvents[eventType]
	hooks := make([]*Hook, len(em.hooks[eventType]))
	copy(hooks, em.hooks[eventType])
	em.mu.RUnlock()

	baseEvent := getBaseEvent(event)
	if baseEvent != nil {
		baseEvent.Type = eventType
		baseEvent.Phase = phase
	}

	src, cat := eventLogParts(eventType)

	if muted {
		return true
	}

	phaseHooks := countEnabledHooksForPhase(hooks, phase)
	if phaseHooks > 0 {
		em.ts.TsLogAdd(adaptix.LogStatusDebug, 0, src, cat, "%s hooks=%d %s", phaseName(phase), phaseHooks, summarizeEvent(eventType, event))
	}

	for _, hook := range hooks {
		if hook.Phase != phase || hook.Disabled {
			continue
		}
		if !em.matchHook(hook, event) {
			continue
		}

		var err error
		if phase == HookPre {
			err = em.executeHookWithTimeout(hook, event, eventType, DefaultPreHookTimeout)
		} else {
			err = em.executeHookWithTimeout(hook, event, eventType, DefaultHookTimeout)
		}

		if err != nil {
			if baseEvent != nil {
				baseEvent.Error = err
				if phase == HookPre {
					baseEvent.Cancelled = true
					em.ts.TsLogAdd(adaptix.LogStatusWarn, 0, src, cat, "pre cancelled by hook %s: %v | %s", hook.Name, err, summarizeEvent(eventType, event))
					return false
				}
			}
			em.ts.TsLogAdd(adaptix.LogStatusWarn, 0, src, cat, "hook %s error: %v", hook.Name, err)
		}

		if baseEvent != nil && baseEvent.Cancelled {
			em.ts.TsLogAdd(adaptix.LogStatusWarn, 0, src, cat, "pre cancelled by hook %s | %s", hook.Name, summarizeEvent(eventType, event))
			return false
		}
	}

	return true
}

func (em *EventManager) EmitAsync(eventType EventType, event any) {
	em.mu.RLock()
	muted := em.mutedEvents[eventType]
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

	src, cat := eventLogParts(eventType)

	if muted {
		return
	}

	phaseHooks := countEnabledHooksForPhase(hooks, HookPost)
	if phaseHooks == 0 {
		return
	}

	matched := make([]*Hook, 0, phaseHooks)
	for _, hook := range hooks {
		if hook.Phase != HookPost || hook.Disabled {
			continue
		}
		if !em.matchHook(hook, event) {
			continue
		}
		matched = append(matched, hook)
	}
	if len(matched) == 0 {
		return
	}
	em.ts.TsLogAdd(adaptix.LogStatusDebug, 0, src, cat, "post matched=%d %s", len(matched), summarizeEvent(eventType, event))

	raw, snapErr := snapshotEventJSON(event)
	if snapErr != nil || (event != nil && len(raw) == 0) {
		if snapErr == nil {
			snapErr = fmt.Errorf("empty snapshot")
		}
		em.ts.TsLogAdd(adaptix.LogStatusWarn, 0, src, cat, "post snapshot failed (%v); sequential fallback (cap %d)", snapErr, maxPostSequentialFallback)
		em.runPostHooksSequentialLive(matched, event, eventType, src, cat)
		return
	}

	var liveFallback []*Hook
	for _, hook := range matched {
		clone := cloneEventFromJSON(event, raw)
		if clone == nil {
			em.ts.TsLogAdd(adaptix.LogStatusWarn, 0, src, cat, "post clone failed for hook %s (type %T); sequential fallback", hook.Name, event)
			liveFallback = append(liveFallback, hook)
			continue
		}
		if be := getBaseEvent(clone); be != nil {
			be.Type = eventType
			be.Phase = HookPost
			be.Cancelled = false
			be.Error = nil
		}

		select {
		case em.taskQueue <- eventTask{
			eventType: eventType,
			event:     clone,
			hook:      hook,
		}:
		default:
			em.ts.TsLogAdd(adaptix.LogStatusWarn, 0, src, cat, "queue full, dropping hook %s", hook.Name)
		}
	}

	if len(liveFallback) > 0 {
		em.runPostHooksSequentialLive(liveFallback, event, eventType, src, cat)
	}
}

func (em *EventManager) runPostHooksSequentialLive(hooks []*Hook, event any, eventType EventType, src, cat string) {
	if len(hooks) == 0 {
		return
	}
	run := hooks
	if len(run) > maxPostSequentialFallback {
		em.ts.TsLogAdd(adaptix.LogStatusWarn, 0, src, cat, "sequential fallback capped %d→%d hooks (dropped: %s)", len(run), maxPostSequentialFallback, joinHookNames(run[maxPostSequentialFallback:]))
		run = run[:maxPostSequentialFallback]
	}
	for _, hook := range run {
		if err := em.executeHookWithTimeout(hook, event, eventType, postSequentialTimeout); err != nil {
			em.ts.TsLogAdd(adaptix.LogStatusWarn, 0, src, cat, "hook %s failed: %v", hook.Name, err)
		}
	}
}

func joinHookNames(hooks []*Hook) string {
	if len(hooks) == 0 {
		return ""
	}
	parts := make([]string, 0, len(hooks))
	for _, h := range hooks {
		if h != nil && h.Name != "" {
			parts = append(parts, h.Name)
		} else {
			parts = append(parts, "?")
		}
	}
	return strings.Join(parts, ",")
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

func (em *EventManager) ListHookInfos(excludeAxScript bool) []HookInfo {
	em.mu.RLock()
	defer em.mu.RUnlock()

	out := make([]HookInfo, 0)
	for eventType, hooks := range em.hooks {
		for _, hook := range hooks {
			if excludeAxScript && strings.HasPrefix(hook.Name, "axscript:") {
				continue
			}
			src := hook.Source
			if src == "" {
				if strings.HasPrefix(hook.Name, "axscript:") {
					src = "axscript"
				} else {
					src = "extender"
				}
			}
			out = append(out, HookInfo{
				ID:       hook.ID,
				Name:     hook.Name,
				Event:    string(eventType),
				Phase:    phaseName(hook.Phase),
				Priority: hook.Priority,
				Enabled:  !hook.Disabled,
				Source:   src,
			})
		}
	}
	return out
}

func (em *EventManager) SetHookDisabled(hookID string, disabled bool) error {
	em.mu.Lock()
	defer em.mu.Unlock()

	for _, hooks := range em.hooks {
		for _, hook := range hooks {
			if hook.ID == hookID {
				hook.Disabled = disabled
				return nil
			}
		}
	}
	return fmt.Errorf("hook %q not found", hookID)
}

func (em *EventManager) MuteEvent(eventType EventType) {
	em.mu.Lock()
	defer em.mu.Unlock()
	if em.mutedEvents == nil {
		em.mutedEvents = make(map[EventType]bool)
	}
	em.mutedEvents[eventType] = true
}

func (em *EventManager) UnmuteEvent(eventType EventType) {
	em.mu.Lock()
	defer em.mu.Unlock()
	delete(em.mutedEvents, eventType)
}

func (em *EventManager) IsEventMuted(eventType EventType) bool {
	em.mu.RLock()
	defer em.mu.RUnlock()
	return em.mutedEvents[eventType]
}

func (em *EventManager) ListMutedEvents() []string {
	em.mu.RLock()
	defer em.mu.RUnlock()
	out := make([]string, 0, len(em.mutedEvents))
	for et, muted := range em.mutedEvents {
		if muted {
			out = append(out, string(et))
		}
	}
	sort.Strings(out)
	return out
}

func countEnabledHooksForPhase(hooks []*Hook, phase HookPhase) int {
	n := 0
	for _, h := range hooks {
		if h.Phase == phase && !h.Disabled {
			n++
		}
	}
	return n
}

const DefaultPriority = 100

func (em *EventManager) OnPre(eventType EventType, name string, handler HookFunc) string {
	return em.Register(eventType, &Hook{
		Name:     name,
		Phase:    HookPre,
		Priority: DefaultPriority,
		Handler:  handler,
	})
}

func (em *EventManager) OnPost(eventType EventType, name string, handler HookFunc) string {
	return em.Register(eventType, &Hook{
		Name:     name,
		Phase:    HookPost,
		Priority: DefaultPriority,
		Handler:  handler,
	})
}

func (em *EventManager) OnPreWithPriority(eventType EventType, name string, priority int, handler HookFunc) string {
	return em.Register(eventType, &Hook{
		Name:     name,
		Phase:    HookPre,
		Priority: priority,
		Handler:  handler,
	})
}

func (em *EventManager) OnPostWithPriority(eventType EventType, name string, priority int, handler HookFunc) string {
	return em.Register(eventType, &Hook{
		Name:     name,
		Phase:    HookPost,
		Priority: priority,
		Handler:  handler,
	})
}

func On[T any](em *EventManager, eventType EventType, name string, phase HookPhase, handler func(*T) error) string {
	return OnWithPriority(em, eventType, name, phase, DefaultPriority, handler)
}

func OnWithPriority[T any](em *EventManager, eventType EventType, name string, phase HookPhase, priority int, handler func(*T) error) string {
	return em.Register(eventType, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: priority,
		Handler: func(event any) error {
			e, ok := event.(*T)
			if !ok {
				return fmt.Errorf("hook %q: unexpected event type %T", name, event)
			}
			return handler(e)
		},
	})
}

func snapshotEventJSON(event any) ([]byte, error) {
	if event == nil {
		return nil, nil
	}
	return json.Marshal(event)
}

func cloneEventFromJSON(prototype any, raw []byte) any {
	if prototype == nil || len(raw) == 0 {
		return nil
	}
	t := reflect.TypeOf(prototype)
	if t == nil {
		return nil
	}
	if t.Kind() != reflect.Ptr || t.Elem().Kind() != reflect.Struct {
		return nil
	}
	dest := reflect.New(t.Elem())
	if err := json.Unmarshal(raw, dest.Interface()); err != nil {
		return nil
	}
	return dest.Interface()
}

func cloneMapShallow(m map[string]any) map[string]any {
	if m == nil {
		return nil
	}
	out := make(map[string]any, len(m))
	for k, v := range m {
		switch t := v.(type) {
		case map[string]any:
			out[k] = cloneMapShallow(t)
		case []any:
			cp := make([]any, len(t))
			copy(cp, t)
			out[k] = cp
		default:
			out[k] = v
		}
	}
	return out
}

const EventLogSource = "events"

func eventLogParts(eventType EventType) (source, category string) {
	return EventLogSource, string(eventType)
}

func phaseName(phase HookPhase) string {
	if phase == HookPre {
		return "pre"
	}
	return "post"
}

func summarizeEvent(eventType EventType, event any) string {
	if event == nil {
		return string(eventType)
	}
	if s, ok := event.(Summarizable); ok {
		return s.Summary()
	}
	return string(eventType)
}

func truncate(s string, n int) string {
	s = strings.TrimSpace(s)
	if n <= 0 || len(s) <= n {
		return s
	}
	return s[:n] + "…"
}
