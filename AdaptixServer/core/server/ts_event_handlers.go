package server

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"time"
	"unicode"

	"AdaptixServer/core/axscript"
	"AdaptixServer/core/database"
	"AdaptixServer/core/eventing"
	"AdaptixServer/core/utils/krypt"

	"github.com/Adaptix-Framework/axc2/v2"
	"github.com/dop251/goja"
)

const (
	maxEventHandlerScriptBytes = 256 * 1024
	eventHandlerHookTimeout    = 10 * time.Second
	eventHandlerHookPriority   = 50
)

type StoredEventHandler struct {
	ID          string                `json:"id"`
	Name        string                `json:"name"`
	Group       string                `json:"group,omitempty"`
	Description string                `json:"description,omitempty"`
	Event       string                `json:"event"`
	Script      string                `json:"script"`
	Enabled     bool                  `json:"enabled"`
	Filters     *eventing.EventFilter `json:"filters,omitempty"`
	CreatedBy   string                `json:"created_by,omitempty"`
	UpdatedBy   string                `json:"updated_by,omitempty"`
	CreatedAt   int64                 `json:"created_at"`
	UpdatedAt   int64                 `json:"updated_at"`
	LastError   string                `json:"last_error,omitempty"`
	LastRunAt   int64                 `json:"last_run_at,omitempty"`
}

type eventHandlerRuntime struct {
	engine   *axscript.ScriptEngine
	callable goja.Callable
	hookID   string
}

type EventHandlerRegistry struct {
	ts      *Teamserver
	mu      sync.RWMutex
	byID    map[string]*StoredEventHandler
	runtime map[string]*eventHandlerRuntime // id → live engine/hook
}

func NewEventHandlerRegistry(ts *Teamserver) *EventHandlerRegistry {
	return &EventHandlerRegistry{
		ts:      ts,
		byID:    make(map[string]*StoredEventHandler),
		runtime: make(map[string]*eventHandlerRuntime),
	}
}

func storedFromDB(row database.DbEventHandler) *StoredEventHandler {
	h := &StoredEventHandler{
		ID:          row.ID,
		Name:        row.Name,
		Group:       row.Group,
		Description: row.Description,
		Event:       row.Event,
		Script:      row.Script,
		Enabled:     row.Enabled,
		CreatedBy:   row.CreatedBy,
		UpdatedBy:   row.UpdatedBy,
		CreatedAt:   row.CreatedAt,
		UpdatedAt:   row.UpdatedAt,
		LastError:   row.LastError,
		LastRunAt:   row.LastRunAt,
	}
	if strings.TrimSpace(row.Filters) != "" {
		var f eventing.EventFilter
		if err := json.Unmarshal([]byte(row.Filters), &f); err == nil {
			h.Filters = eventing.NormalizeFilter(&f)
		}
	}
	return h
}

func (h *StoredEventHandler) toDB() database.DbEventHandler {
	filters := ""
	if h.Filters != nil {
		if raw, err := json.Marshal(h.Filters); err == nil {
			filters = string(raw)
		}
	}
	return database.DbEventHandler{
		ID: h.ID, Name: h.Name, Group: h.Group, Description: h.Description,
		Event: h.Event, Script: h.Script, Enabled: h.Enabled, Filters: filters,
		CreatedBy: h.CreatedBy, UpdatedBy: h.UpdatedBy,
		CreatedAt: h.CreatedAt, UpdatedAt: h.UpdatedAt,
		LastError: h.LastError, LastRunAt: h.LastRunAt,
	}
}

func (r *EventHandlerRegistry) LoadFromDB() error {
	if r == nil || r.ts == nil || r.ts.DBMS == nil {
		return nil
	}
	rows := r.ts.DBMS.DbEventHandlersAll()
	if len(rows) == 0 {
		r.migrateFromJSONFiles()
		rows = r.ts.DBMS.DbEventHandlersAll()
	}
	for _, row := range rows {
		h := storedFromDB(row)
		if h.ID == "" || h.Event == "" || strings.TrimSpace(h.Script) == "" {
			continue
		}
		r.mu.Lock()
		r.byID[h.ID] = h
		r.mu.Unlock()
		if h.Enabled {
			if err := r.bind(h.ID); err != nil {
				r.ts.TsLogAdd(adaptix.LogStatusError, 0, "events", "handler",
					"bind %s (%s): %v", h.ID, h.Name, err)
			}
		}
	}
	return nil
}

func (r *EventHandlerRegistry) migrateFromJSONFiles() {
	dir := filepath.Join(r.ts.Paths.DataPath, "event_handlers")
	entries, err := os.ReadDir(dir)
	if err != nil {
		return
	}
	for _, e := range entries {
		if e.IsDir() || !strings.HasSuffix(e.Name(), ".json") {
			continue
		}
		raw, err := os.ReadFile(filepath.Join(dir, e.Name()))
		if err != nil {
			continue
		}
		var h StoredEventHandler
		if err := json.Unmarshal(raw, &h); err != nil || h.ID == "" {
			continue
		}
		h.Filters = eventing.NormalizeFilter(h.Filters)
		_ = r.ts.DBMS.DbEventHandlerUpsert(h.toDB())
		r.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "events", "handler",
			"migrated handler %s from JSON file", h.ID)
	}
}

type RegisterEventHandlerRequest struct {
	ID          string                `json:"id"`
	Name        string                `json:"name"`
	Group       string                `json:"group"`
	Description string                `json:"description"`
	Event       string                `json:"event"`
	Script      string                `json:"script"`
	Enabled     *bool                 `json:"enabled"` // nil → true
	Filters     *eventing.EventFilter `json:"filters"`
}

func (r *EventHandlerRegistry) Register(req RegisterEventHandlerRequest, operator string) (*StoredEventHandler, error) {
	if r == nil {
		return nil, fmt.Errorf("event handler registry not initialized")
	}

	event := strings.TrimSpace(req.Event)
	if event == "" {
		return nil, fmt.Errorf("event is required")
	}
	if !eventing.IsValidEventType(event) {
		return nil, fmt.Errorf("invalid event type %q (use domain.action, e.g. sessions.create)", event)
	}
	eventing.TrackCustomEventType(event)

	script := strings.TrimSpace(req.Script)
	if script == "" {
		return nil, fmt.Errorf("script is required")
	}
	if len(script) > maxEventHandlerScriptBytes {
		return nil, fmt.Errorf("script exceeds %d bytes", maxEventHandlerScriptBytes)
	}

	name := strings.TrimSpace(req.Name)
	if name == "" {
		return nil, fmt.Errorf("name is required")
	}
	group := strings.TrimSpace(req.Group)
	if group == "" {
		group = name
	}
	desc := strings.TrimSpace(req.Description)
	filters := eventing.NormalizeFilter(req.Filters)

	enabled := true
	if req.Enabled != nil {
		enabled = *req.Enabled
	}

	if err := dryCompileHandlerScript(script); err != nil {
		return nil, fmt.Errorf("script compile: %w", err)
	}

	now := time.Now().Unix()
	var existing *StoredEventHandler

	r.mu.Lock()
	id := strings.TrimSpace(req.ID)
	if id != "" {
		existing = r.byID[id]
		if existing == nil {
			r.mu.Unlock()
			return nil, fmt.Errorf("handler %q not found", id)
		}
	} else {
		for _, h := range r.byID {
			if strings.EqualFold(h.Group, group) && strings.EqualFold(h.Name, name) {
				existing = h
				id = h.ID
				break
			}
		}
	}

	if existing == nil {
		uid, err := krypt.GenerateUID(8)
		if err != nil {
			r.mu.Unlock()
			return nil, err
		}
		id = uid
		existing = &StoredEventHandler{
			ID:        id,
			CreatedAt: now,
			CreatedBy: operator,
		}
		r.byID[id] = existing
	}

	existing.Name = name
	existing.Group = group
	existing.Description = desc
	existing.Event = event
	existing.Script = script
	existing.Filters = filters
	existing.Enabled = enabled
	existing.UpdatedAt = now
	existing.UpdatedBy = operator
	if existing.CreatedAt == 0 {
		existing.CreatedAt = now
	}
	if existing.CreatedBy == "" {
		existing.CreatedBy = operator
	}

	snap := *existing
	r.mu.Unlock()

	if err := r.persist(&snap); err != nil {
		return nil, err
	}

	r.unbind(id)
	if enabled {
		if err := r.bind(id); err != nil {
			return nil, err
		}
	}

	r.mu.RLock()
	out := *r.byID[id]
	r.mu.RUnlock()
	if r.ts != nil {
		r.ts.TsEventHandlersBroadcast()
	}
	return &out, nil
}

func (r *EventHandlerRegistry) Enable(id string) error {
	h, err := r.getCopy(id)
	if err != nil {
		return err
	}
	if h.Enabled {
		r.mu.RLock()
		_, bound := r.runtime[id]
		r.mu.RUnlock()
		if bound {
			return nil
		}
	}
	r.mu.Lock()
	if cur, ok := r.byID[id]; ok {
		cur.Enabled = true
		cur.UpdatedAt = time.Now().Unix()
		h = *cur
	}
	r.mu.Unlock()
	if err := r.persist(&h); err != nil {
		return err
	}
	if err := r.bind(id); err != nil {
		return err
	}
	if r.ts != nil {
		r.ts.TsEventHandlersBroadcast()
	}
	return nil
}

func (r *EventHandlerRegistry) Disable(id string) error {
	h, err := r.getCopy(id)
	if err != nil {
		return err
	}
	r.unbind(id)
	r.mu.Lock()
	if cur, ok := r.byID[id]; ok {
		cur.Enabled = false
		cur.UpdatedAt = time.Now().Unix()
		h = *cur
	}
	r.mu.Unlock()
	if err := r.persist(&h); err != nil {
		return err
	}
	if r.ts != nil {
		r.ts.TsEventHandlersBroadcast()
	}
	return nil
}

func (r *EventHandlerRegistry) Remove(id string) error {
	if _, err := r.getCopy(id); err != nil {
		return err
	}
	r.unbind(id)
	r.mu.Lock()
	delete(r.byID, id)
	r.mu.Unlock()
	if r.ts != nil && r.ts.DBMS != nil {
		_ = r.ts.DBMS.DbEventHandlerDelete(id)
	}
	if r.ts != nil {
		r.ts.TsEventHandlersBroadcast()
	}
	return nil
}

func (r *EventHandlerRegistry) Get(id string) (*StoredEventHandler, error) {
	h, err := r.getCopy(id)
	if err != nil {
		return nil, err
	}
	return &h, nil
}

func (r *EventHandlerRegistry) List() []StoredEventHandler {
	if r == nil {
		return nil
	}
	r.mu.RLock()
	defer r.mu.RUnlock()
	out := make([]StoredEventHandler, 0, len(r.byID))
	for _, h := range r.byID {
		out = append(out, *h)
	}
	return out
}

func (r *EventHandlerRegistry) getCopy(id string) (StoredEventHandler, error) {
	if r == nil {
		return StoredEventHandler{}, fmt.Errorf("event handler registry not initialized")
	}
	id = strings.TrimSpace(id)
	r.mu.RLock()
	h, ok := r.byID[id]
	r.mu.RUnlock()
	if !ok || h == nil {
		return StoredEventHandler{}, fmt.Errorf("handler %q not found", id)
	}
	return *h, nil
}

func (r *EventHandlerRegistry) persist(h *StoredEventHandler) error {
	if r.ts == nil || r.ts.DBMS == nil {
		return fmt.Errorf("database not available")
	}
	return r.ts.DBMS.DbEventHandlerUpsert(h.toDB())
}

func (r *EventHandlerRegistry) unbind(id string) {
	r.mu.Lock()
	rt := r.runtime[id]
	delete(r.runtime, id)
	r.mu.Unlock()
	if rt == nil {
		return
	}
	if rt.hookID != "" && r.ts.EventManager != nil {
		r.ts.EventManager.Unregister(rt.hookID)
	}
}

func (r *EventHandlerRegistry) recordRun(id string, runErr error) {
	now := time.Now().Unix()
	errStr := ""
	if runErr != nil {
		errStr = runErr.Error()
	}
	r.mu.Lock()
	if h, ok := r.byID[id]; ok && h != nil {
		h.LastRunAt = now
		h.LastError = errStr
	}
	r.mu.Unlock()
	if r.ts != nil && r.ts.DBMS != nil {
		_ = r.ts.DBMS.DbEventHandlerSetLastRun(id, errStr, now)
	}
}

func (r *EventHandlerRegistry) bind(id string) error {
	h, err := r.getCopy(id)
	if err != nil {
		return err
	}
	if r.ts.ScriptManager == nil {
		return fmt.Errorf("script manager not initialized")
	}
	if r.ts.EventManager == nil {
		return fmt.Errorf("event manager not initialized")
	}

	r.unbind(id)

	engine := axscript.NewScriptEngine("handler:"+id, r.ts.ScriptManager)
	axscript.RegisterHandlerBridges(engine)

	fn, err := engine.CompileHandler(h.Script)
	if err != nil {
		r.recordRun(id, err)
		return err
	}

	handlerID := id
	eng := engine
	callable := fn
	hookName := "handler:" + id
	eventType := h.Event
	handlerName := h.Name

	hookID := r.ts.EventManager.Register(eventing.EventType(h.Event), &eventing.Hook{
		ID:       id, // stable id shared with store
		Name:     hookName,
		Phase:    eventing.HookPost,
		Priority: eventHandlerHookPriority,
		Timeout:  eventHandlerHookTimeout,
		Source:   "handler",
		Filter:   h.Filters,
		Interrupt: func() {
			eng.Interrupt("timeout")
		},
		Handler: func(event any) error {
			eng.ClearInterrupt()
			payload := eventing.EventToMap(event)
			_, callErr := eng.CallCallableAs("server", callable, eng.ToValue(payload))
			r.recordRun(handlerID, callErr)
			if callErr != nil {
				r.ts.TsLogAdd(adaptix.LogStatusWarn, 0, "events", eventType,
					"handler %s (%s) error: %v", handlerID, handlerName, callErr)
			}
			eng.ClearInterrupt()
			return nil // never cancel post phase
		},
	})

	r.mu.Lock()
	r.runtime[id] = &eventHandlerRuntime{
		engine:   engine,
		callable: fn,
		hookID:   hookID,
	}
	r.mu.Unlock()
	return nil
}

func dryCompileHandlerScript(script string) error {
	rt := goja.New()
	b := strings.TrimSpace(script)
	b = strings.TrimPrefix(b, "\ufeff")
	if _, err := rt.RunString(b); err != nil {
		return err
	}
	for _, name := range []string{"handler", "onEvent"} {
		val := rt.Get(name)
		if val == nil || goja.IsUndefined(val) || goja.IsNull(val) {
			continue
		}
		if _, ok := goja.AssertFunction(val); ok {
			return nil
		}
	}
	return fmt.Errorf("define function handler(event) { ... }")
}

func (ts *Teamserver) initEventHandlerRegistry() {
	if ts.EventHandlers != nil {
		return
	}
	ts.EventHandlers = NewEventHandlerRegistry(ts)

	if ts.EventManager != nil {
		ts.EventManager.SetFactsEnricher(func(facts *eventing.EventFacts, _ any) {
			if facts == nil || !facts.HasAgent || facts.AgentID == 0 {
				return
			}
			if facts.AgentName != "" && facts.OS != "" && facts.Listener != "" &&
				facts.Computer != "" && facts.User != "" && facts.Tags != "" {
				return
			}
			agent, ok := ts.Agents.Get(facts.AgentID)
			if !ok || agent == nil {
				return
			}
			d := agent.GetData()
			if facts.AgentName == "" {
				facts.AgentName = d.Name
			}
			if facts.Listener == "" {
				facts.Listener = d.Listener
			}
			if facts.OS == "" {
				facts.OS = osToString(d.Os)
			}
			if facts.Computer == "" {
				facts.Computer = d.Computer
			}
			if facts.User == "" {
				if d.Impersonated != "" {
					facts.User = d.Impersonated
				} else {
					facts.User = d.Username
				}
			}
			if facts.Tags == "" {
				facts.Tags = d.Tags
			}
		})
	}
}

func (ts *Teamserver) TsEventHandlersLoad() {
	ts.initEventHandlerRegistry()
	if err := ts.EventHandlers.LoadFromDB(); err != nil {
		ts.TsLogAdd(adaptix.LogStatusError, 0, "events", "handler", "load handlers: %v", err)
	}
}

func (ts *Teamserver) TsEventHandlersBroadcast() {
	ts.TsSyncAllClients(ts.TsPresyncEventHandlers())
}

func (ts *Teamserver) TsPresyncEventHandlers() interface{} {
	items := make([]map[string]interface{}, 0)
	for _, h := range ts.collectEventHandlers() {
		m := map[string]interface{}{
			"id": h.ID, "name": h.Name, "description": h.Description, "group": h.Group,
			"event": h.Event, "source": h.Source, "enabled": h.Enabled,
			"event_muted": h.EventMuted, "last_error": h.LastError, "last_run_at": h.LastRunAt,
		}
		if h.Filters != nil {
			m["filters"] = h.Filters
		}
		items = append(items, m)
	}
	return CreateSpEventHandlers(items)
}

func (ts *Teamserver) TsEventHandlerRegister(requestJSON string, operator string) (string, error) {
	ts.initEventHandlerRegistry()
	var req RegisterEventHandlerRequest
	if err := json.Unmarshal([]byte(requestJSON), &req); err != nil {
		return "", fmt.Errorf("invalid JSON: %w", err)
	}
	h, err := ts.EventHandlers.Register(req, operator)
	if err != nil {
		return "", err
	}
	data, err := json.Marshal(h)
	if err != nil {
		return "", err
	}
	return string(data), nil
}

func (ts *Teamserver) TsEventHandlerGet(id string) (string, error) {
	ts.initEventHandlerRegistry()
	h, err := ts.EventHandlers.Get(id)
	if err != nil {
		return "", err
	}
	data, err := json.Marshal(h)
	if err != nil {
		return "", err
	}
	return string(data), nil
}

func SanitizeHandlerName(name string) string {
	name = strings.TrimSpace(name)
	if name == "" {
		return ""
	}
	var b strings.Builder
	for _, r := range name {
		if unicode.IsLetter(r) || unicode.IsDigit(r) || r == '_' || r == '-' || r == '.' {
			b.WriteRune(r)
		} else {
			b.WriteByte('_')
		}
	}
	s := b.String()
	if s == "" {
		return "handler"
	}
	return s
}
