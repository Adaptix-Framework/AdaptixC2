package server

import (
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
