package eventing

import (
	"fmt"
	"regexp"
	"strings"
	"sync"
)

const MaxEventTypeLen = 64

var eventTypeRe = regexp.MustCompile(`^[a-z][a-z0-9_]*(\.[a-z][a-z0-9_]*)+$`)

func IsValidEventType(eventType string) bool {
	et := strings.TrimSpace(eventType)
	if et == "" || len(et) > MaxEventTypeLen {
		return false
	}
	if KnownEventTypes[EventType(et)] {
		return true
	}
	return eventTypeRe.MatchString(et)
}

func IsSystemEventType(eventType string) bool {
	return KnownEventTypes[EventType(strings.TrimSpace(eventType))]
}

type EventDataCustom struct {
	BaseEvent
	Source string `json:"source,omitempty"`
	Text   string `json:"text"`
}

func (e *EventDataCustom) Base() *BaseEvent { return &e.BaseEvent }

func (e *EventDataCustom) FillFacts(f *EventFacts) {
	if e == nil {
		return
	}
	if be := e.Base(); be != nil && be.Type != "" {
		f.Type = string(be.Type)
	}
}

func (e *EventDataCustom) Summary() string {
	if e == nil {
		return "custom"
	}
	return fmt.Sprintf("source=%s text=%s", e.Source, truncate(e.Text, 80))
}

var (
	_ Event        = (*EventDataCustom)(nil)
	_ FactSource   = (*EventDataCustom)(nil)
	_ Summarizable = (*EventDataCustom)(nil)
)

var (
	seenCustomMu    sync.RWMutex
	seenCustomTypes = map[string]struct{}{}
)

func TrackCustomEventType(eventType string) {
	et := strings.TrimSpace(eventType)
	if et == "" || IsSystemEventType(et) {
		return
	}
	if !IsValidEventType(et) {
		return
	}
	seenCustomMu.Lock()
	seenCustomTypes[et] = struct{}{}
	seenCustomMu.Unlock()
}

func ListSeenCustomEventTypes() []string {
	seenCustomMu.RLock()
	defer seenCustomMu.RUnlock()
	out := make([]string, 0, len(seenCustomTypes))
	for et := range seenCustomTypes {
		out = append(out, et)
	}
	return out
}

func ListAllEventTypes() []string {
	out := make([]string, 0, len(KnownEventTypes)+8)
	for et := range KnownEventTypes {
		out = append(out, string(et))
	}
	out = append(out, ListSeenCustomEventTypes()...)
	return out
}

func TextFromEmitArg(v any) string {
	if v == nil {
		return ""
	}
	switch t := v.(type) {
	case string:
		return t
	case map[string]any:
		if s, ok := t["text"].(string); ok {
			return s
		}
		if x, ok := t["text"]; ok && x != nil {
			return fmt.Sprint(x)
		}
		return ""
	default:
		return fmt.Sprint(t)
	}
}
