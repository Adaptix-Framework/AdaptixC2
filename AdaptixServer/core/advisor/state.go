package advisor

import "sync"

type AgentState struct {
	mu      sync.RWMutex
	Context AgentContext
	History []HistoryEntry
	maxHist int
	Recon   *ReconState
}

func NewAgentState(ctx AgentContext, maxHistory int) *AgentState {
	if maxHistory <= 0 {
		maxHistory = 10
	}
	return &AgentState{
		Context: ctx,
		History: make([]HistoryEntry, 0, maxHistory),
		maxHist: maxHistory,
		Recon:   NewReconState(),
	}
}

func (s *AgentState) AddHistory(entry HistoryEntry) {
	s.mu.Lock()
	defer s.mu.Unlock()

	s.History = append(s.History, entry)
	if len(s.History) > s.maxHist {
		s.History = s.History[len(s.History)-s.maxHist:]
	}
}

func (s *AgentState) GetHistory() []HistoryEntry {
	s.mu.RLock()
	defer s.mu.RUnlock()

	out := make([]HistoryEntry, len(s.History))
	copy(out, s.History)
	return out
}

func (s *AgentState) UpdateContext(ctx AgentContext) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.Context = ctx
}

func (s *AgentState) GetContext() AgentContext {
	s.mu.RLock()
	defer s.mu.RUnlock()
	return s.Context
}
