package server

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"

	eventing2 "AdaptixServer/core/eventing"
)

func (ts *Teamserver) TsEventHookRegister(eventType string, name string, phase int, priority int, handler func(event any) error) string {
	return ts.EventManager.Register(eventing2.EventType(eventType), &eventing2.Hook{
		Name:     name,
		Phase:    eventing2.HookPhase(phase),
		Priority: priority,
		Handler:  handler,
	})
}

func (ts *Teamserver) TsEventHookUnregister(hookID string) bool {
	return ts.EventManager.Unregister(hookID)
}

func (ts *Teamserver) TsEventHookUnregisterByName(name string) int {
	return ts.EventManager.UnregisterByName(name)
}

func (ts *Teamserver) TsEventHookOnPre(eventType string, name string, handler func(event any) error) string {
	return ts.EventManager.OnPre(eventing2.EventType(eventType), name, handler)
}

func (ts *Teamserver) TsEventHookOnPost(eventType string, name string, handler func(event any) error) string {
	return ts.EventManager.OnPost(eventing2.EventType(eventType), name, handler)
}

func (ts *Teamserver) TsEventHookSetEnabled(hookID string, enabled bool) error {
	if ts.EventManager == nil {
		return fmt.Errorf("event manager not initialized")
	}
	return ts.EventManager.SetHookDisabled(hookID, !enabled)
}

func (ts *Teamserver) TsEventMute(eventType string) error {
	if ts.EventManager == nil {
		return fmt.Errorf("event manager not initialized")
	}
	et := strings.TrimSpace(eventType)
	if et == "" {
		return fmt.Errorf("event type is empty")
	}
	ts.EventManager.MuteEvent(eventing2.EventType(et))
	if ts.DBMS != nil {
		_ = ts.DBMS.DbEventMuteAdd(et)
	}
	ts.TsEventHandlersBroadcast()
	return nil
}

func (ts *Teamserver) TsEventUnmute(eventType string) error {
	if ts.EventManager == nil {
		return fmt.Errorf("event manager not initialized")
	}
	et := strings.TrimSpace(eventType)
	if et == "" {
		return fmt.Errorf("event type is empty")
	}
	ts.EventManager.UnmuteEvent(eventing2.EventType(et))
	if ts.DBMS != nil {
		_ = ts.DBMS.DbEventMuteRemove(et)
	}
	ts.TsEventHandlersBroadcast()
	return nil
}

func (ts *Teamserver) loadEventMutes() {
	if ts.EventManager == nil || ts.DBMS == nil {
		return
	}
	legacy := filepath.Join(ts.Paths.DataPath, "event_mutes.json")
	if raw, err := os.ReadFile(legacy); err == nil {
		var list []string
		if json.Unmarshal(raw, &list) == nil {
			for _, et := range list {
				et = strings.TrimSpace(et)
				if et != "" {
					_ = ts.DBMS.DbEventMuteAdd(et)
				}
			}
			_ = os.Remove(legacy)
		}
	}
	for _, et := range ts.DBMS.DbEventMutesAll() {
		et = strings.TrimSpace(et)
		if et != "" {
			ts.EventManager.MuteEvent(eventing2.EventType(et))
		}
	}
}

func (ts *Teamserver) TsEventMutesList() (string, error) {
	if ts.EventManager == nil {
		return "[]", nil
	}
	data, err := json.Marshal(ts.EventManager.ListMutedEvents())
	if err != nil {
		return "[]", err
	}
	return string(data), nil
}

type EventHandlerInfo struct {
	ID          string                 `json:"id"`
	Name        string                 `json:"name,omitempty"`
	Description string                 `json:"description,omitempty"`
	Group       string                 `json:"group,omitempty"`
	Event       string                 `json:"event"`
	Source      string                 `json:"source"` // handler | extender | core
	Enabled     bool                   `json:"enabled"`
	EventMuted  bool                   `json:"event_muted"`
	Filters     *eventing2.EventFilter `json:"filters,omitempty"`
	LastError   string                 `json:"last_error,omitempty"`
	LastRunAt   int64                  `json:"last_run_at,omitempty"`
}

func (ts *Teamserver) collectEventHandlers() []EventHandlerInfo {
	mutedSet := make(map[string]bool)
	if ts.EventManager != nil {
		for _, et := range ts.EventManager.ListMutedEvents() {
			mutedSet[et] = true
		}
	}

	out := make([]EventHandlerInfo, 0)

	if ts.EventHandlers != nil {
		for _, h := range ts.EventHandlers.List() {
			out = append(out, EventHandlerInfo{
				ID:          h.ID,
				Name:        h.Name,
				Description: h.Description,
				Group:       h.Group,
				Event:       h.Event,
				Source:      "handler",
				Enabled:     h.Enabled,
				EventMuted:  mutedSet[h.Event],
				Filters:     h.Filters,
				LastError:   h.LastError,
				LastRunAt:   h.LastRunAt,
			})
		}
	}

	if ts.EventManager != nil {
		for _, h := range ts.EventManager.ListHookInfos(true) {
			if h.Source == "handler" || strings.HasPrefix(h.Name, "handler:") {
				continue
			}
			src := h.Source
			if src == "" {
				src = "extender"
			}
			out = append(out, EventHandlerInfo{
				ID:         h.ID,
				Name:       h.Name,
				Event:      h.Event,
				Source:     src,
				Enabled:    h.Enabled,
				EventMuted: mutedSet[h.Event],
			})
		}
	}
	return out
}

func (ts *Teamserver) TsEventHandlersList() (string, error) {
	data, err := json.Marshal(ts.collectEventHandlers())
	if err != nil {
		return "[]", err
	}
	return string(data), nil
}

type EventHandlersPage struct {
	Items  []EventHandlerInfo `json:"items"`
	Total  int                `json:"total"`
	Offset int                `json:"offset"`
	Limit  int                `json:"limit"`
}

func (ts *Teamserver) TsEventHandlersGetPage(offset, limit int, q, event, source, group, enabled string) ([]byte, error) {
	if limit <= 0 {
		limit = 50
	}
	if limit > 1000 {
		limit = 1000
	}
	if offset < 0 {
		offset = 0
	}

	all := ts.collectEventHandlers()
	q = strings.ToLower(strings.TrimSpace(q))
	event = strings.TrimSpace(event)
	source = strings.TrimSpace(source)
	group = strings.TrimSpace(group)
	enabled = strings.TrimSpace(strings.ToLower(enabled))

	filtered := make([]EventHandlerInfo, 0, len(all))
	for _, h := range all {
		if event != "" && h.Event != event {
			continue
		}
		if source != "" && h.Source != source {
			continue
		}
		if group != "" && h.Group != group {
			continue
		}
		if enabled == "true" || enabled == "1" {
			if !h.Enabled {
				continue
			}
		} else if enabled == "false" || enabled == "0" {
			if h.Enabled {
				continue
			}
		}
		if q != "" {
			hay := strings.ToLower(strings.Join([]string{
				h.ID, h.Name, h.Description, h.Group, h.Event, h.Source, h.LastError,
			}, " "))
			if !strings.Contains(hay, q) {
				continue
			}
		}
		filtered = append(filtered, h)
	}

	sort.SliceStable(filtered, func(i, j int) bool {
		if filtered[i].Event != filtered[j].Event {
			return filtered[i].Event < filtered[j].Event
		}
		if filtered[i].Name != filtered[j].Name {
			return filtered[i].Name < filtered[j].Name
		}
		return filtered[i].ID < filtered[j].ID
	})

	total := len(filtered)
	if offset > total {
		offset = total
	}
	end := offset + limit
	if end > total {
		end = total
	}
	page := filtered[offset:end]
	if page == nil {
		page = []EventHandlerInfo{}
	}

	return json.Marshal(EventHandlersPage{
		Items:  page,
		Total:  total,
		Offset: offset,
		Limit:  limit,
	})
}

func (ts *Teamserver) TsEventHandlerEnable(id string) error {
	if id == "" {
		return fmt.Errorf("id is required")
	}
	if ts.EventHandlers != nil {
		if _, err := ts.EventHandlers.Get(id); err == nil {
			return ts.EventHandlers.Enable(id)
		}
	}
	return ts.TsEventHookSetEnabled(id, true)
}

func (ts *Teamserver) TsEventHandlerDisable(id string) error {
	if id == "" {
		return fmt.Errorf("id is required")
	}
	if ts.EventHandlers != nil {
		if _, err := ts.EventHandlers.Get(id); err == nil {
			return ts.EventHandlers.Disable(id)
		}
	}
	return ts.TsEventHookSetEnabled(id, false)
}

func (ts *Teamserver) TsEventHandlerRemove(id string) error {
	if id == "" {
		return fmt.Errorf("id is required")
	}
	if ts.EventHandlers != nil {
		if _, err := ts.EventHandlers.Get(id); err == nil {
			return ts.EventHandlers.Remove(id)
		}
	}
	return fmt.Errorf("handler %q not found or not removable (extender hooks cannot be removed from UI)", id)
}
