package eventing

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

/// clients

func (em *EventManager) OnClientConnect(name string, phase HookPhase, handler func(*EventDataClientConnect) error) string {
	return em.Register(EventClientConnect, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventDataClientConnect))
		},
	})
}

func (em *EventManager) OnClientDisconnect(name string, phase HookPhase, handler func(*EventDataClientDisconnect) error) string {
	return em.Register(EventClientDisconnect, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventDataClientDisconnect))
		},
	})
}

/// credentials

func (em *EventManager) OnCredentialsAdd(name string, phase HookPhase, handler func(*EventCredentialsAdd) error) string {
	return em.Register(EventCredsAdd, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventCredentialsAdd))
		},
	})
}

func (em *EventManager) OnCredentialsEdit(name string, phase HookPhase, handler func(*EventCredentialsEdit) error) string {
	return em.Register(EventCredsEdit, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventCredentialsEdit))
		},
	})
}

func (em *EventManager) OnCredentialsRemove(name string, phase HookPhase, handler func(*EventCredentialsRemove) error) string {
	return em.Register(EventCredsRemove, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventCredentialsRemove))
		},
	})
}

/// agents

func (em *EventManager) OnAgentNew(name string, phase HookPhase, handler func(*EventDataAgentNew) error) string {
	return em.Register(EventAgentNew, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventDataAgentNew))
		},
	})
}

func (em *EventManager) OnAgentCheckin(name string, handler func(*EventDataAgentCheckin) error) string {
	return em.Register(EventAgentCheckin, &Hook{
		Name:     name,
		Phase:    HookPost,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventDataAgentCheckin))
		},
	})
}

func (em *EventManager) OnAgentTerminate(name string, phase HookPhase, handler func(*EventDataAgentTerminate) error) string {
	return em.Register(EventAgentTerminate, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventDataAgentTerminate))
		},
	})
}

func (em *EventManager) OnAgentUpdate(name string, phase HookPhase, handler func(*EventDataAgentUpdate) error) string {
	return em.Register(EventAgentUpdate, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventDataAgentUpdate))
		},
	})
}

func (em *EventManager) OnAgentRemove(name string, phase HookPhase, handler func(*EventDataAgentRemove) error) string {
	return em.Register(EventAgentRemove, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventDataAgentRemove))
		},
	})
}

/// tasks

func (em *EventManager) OnTaskCreate(name string, phase HookPhase, handler func(*EventDataTaskCreate) error) string {
	return em.Register(EventTaskCreate, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventDataTaskCreate))
		},
	})
}

func (em *EventManager) OnTaskStart(name string, phase HookPhase, handler func(*EventDataTaskStart) error) string {
	return em.Register(EventTaskStart, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventDataTaskStart))
		},
	})
}

func (em *EventManager) OnTaskUpdateJob(name string, phase HookPhase, handler func(*EventDataTaskUpdateJob) error) string {
	return em.Register(EventTaskUpdateJob, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventDataTaskUpdateJob))
		},
	})
}

func (em *EventManager) OnTaskComplete(name string, handler func(*EventDataTaskComplete) error) string {
	return em.Register(EventTaskComplete, &Hook{
		Name:     name,
		Phase:    HookPost,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventDataTaskComplete))
		},
	})
}

/// listeners

func (em *EventManager) OnListenerStart(name string, phase HookPhase, handler func(*EventDataListenerStart) error) string {
	return em.Register(EventListenerStart, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventDataListenerStart))
		},
	})
}

func (em *EventManager) OnListenerStop(name string, phase HookPhase, handler func(*EventDataListenerStop) error) string {
	return em.Register(EventListenerStop, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventDataListenerStop))
		},
	})
}

/// downloads

func (em *EventManager) OnDownloadStart(name string, phase HookPhase, handler func(*EventDataDownloadStart) error) string {
	return em.Register(EventDownloadStart, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventDataDownloadStart))
		},
	})
}

func (em *EventManager) OnDownloadFinish(name string, handler func(*EventDataDownloadFinish) error) string {
	return em.Register(EventDownloadFinish, &Hook{
		Name:     name,
		Phase:    HookPost,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventDataDownloadFinish))
		},
	})
}

/// screenshots

func (em *EventManager) OnScreenshotAdd(name string, phase HookPhase, handler func(*EventDataScreenshotAdd) error) string {
	return em.Register(EventScreenshotAdd, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventDataScreenshotAdd))
		},
	})
}

func (em *EventManager) OnScreenshotRemove(name string, phase HookPhase, handler func(*EventDataScreenshotRemove) error) string {
	return em.Register(EventScreenshotRemove, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventDataScreenshotRemove))
		},
	})
}

/// tunnels

func (em *EventManager) OnTunnelStart(name string, phase HookPhase, handler func(*EventDataTunnelStart) error) string {
	return em.Register(EventTunnelStart, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventDataTunnelStart))
		},
	})
}

func (em *EventManager) OnTunnelStop(name string, phase HookPhase, handler func(*EventDataTunnelStop) error) string {
	return em.Register(EventTunnelStop, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventDataTunnelStop))
		},
	})
}

/// targets

func (em *EventManager) OnTargetAdd(name string, phase HookPhase, handler func(*EventDataTargetAdd) error) string {
	return em.Register(EventTargetAdd, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventDataTargetAdd))
		},
	})
}

func (em *EventManager) OnTargetEdit(name string, phase HookPhase, handler func(*EventDataTargetEdit) error) string {
	return em.Register(EventTargetEdit, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventDataTargetEdit))
		},
	})
}

func (em *EventManager) OnTargetRemove(name string, phase HookPhase, handler func(*EventDataTargetRemove) error) string {
	return em.Register(EventTargetRemove, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventDataTargetRemove))
		},
	})
}

/// pivots

func (em *EventManager) OnPivotCreate(name string, phase HookPhase, handler func(*EventDataPivotCreate) error) string {
	return em.Register(EventPivotCreate, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventDataPivotCreate))
		},
	})
}

func (em *EventManager) OnPivotRemove(name string, phase HookPhase, handler func(*EventDataPivotRemove) error) string {
	return em.Register(EventPivotRemove, &Hook{
		Name:     name,
		Phase:    phase,
		Priority: DefaultPriority,
		Handler: func(event any) error {
			return handler(event.(*EventDataPivotRemove))
		},
	})
}
