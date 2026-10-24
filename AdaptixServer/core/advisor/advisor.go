package advisor

import (
	"context"
	"encoding/json"
	"fmt"
	"strconv"
	"strings"
	"sync"
	"time"

	"AdaptixServer/core/eventing"
	"AdaptixServer/core/utils/logs"

	"github.com/Adaptix-Framework/axc2"
)

const TYPE_ADVISOR_RECOMMENDATION = 0xA1

type TeamserverInterface interface {
	GetAgent(agentId string) (adaptix.AgentData, bool)
	GetAllAgents() []adaptix.AgentData
	SyncAdvisor(packet interface{})
}

type Advisor struct {
	config    AdvisorConfig
	llm       LLMClient
	states    map[string]*AgentState
	mu        sync.RWMutex
	ts        TeamserverInterface
	hookID    string
	agentHook string
}

func NewAdvisor(config AdvisorConfig, ts TeamserverInterface) *Advisor {
	return &Advisor{
		config: config,
		llm:    NewClaudeClient(config),
		states: make(map[string]*AgentState),
		ts:     ts,
	}
}

func (a *Advisor) RegisterHooks(em *eventing.EventManager) {
	if !a.config.Enabled {
		logs.Info("", "AI Advisor: disabled in config")
		return
	}

	a.hookID = em.OnTaskComplete("advisor.task_complete", func(event *eventing.EventDataTaskComplete) error {
		if !event.Task.Completed {
			return nil
		}
		go a.handleTaskComplete(event.AgentId, event.Task)
		return nil
	})

	a.agentHook = em.OnAgentNew("advisor.agent_new", eventing.HookPost, func(event *eventing.EventDataAgentNew) error {
		if event.Restore {
			return nil
		}
		a.initAgentState(event.Agent)
		return nil
	})

	logs.Success("", "AI Advisor: enabled (model=%s)", a.config.Model)
}

func (a *Advisor) UnregisterHooks(em *eventing.EventManager) {
	if a.hookID != "" {
		em.Unregister(a.hookID)
	}
	if a.agentHook != "" {
		em.Unregister(a.agentHook)
	}
}

func (a *Advisor) initAgentState(agent adaptix.AgentData) {
	ctx := agentDataToContext(agent)

	a.mu.Lock()
	defer a.mu.Unlock()

	if _, exists := a.states[agent.Id]; !exists {
		a.states[agent.Id] = NewAgentState(ctx, a.config.MaxHistory)
	}
}

func (a *Advisor) getOrCreateState(agentId string) *AgentState {
	a.mu.RLock()
	state, ok := a.states[agentId]
	a.mu.RUnlock()
	if ok {
		return state
	}

	agentData, found := a.ts.GetAgent(agentId)
	if !found {
		a.mu.Lock()
		defer a.mu.Unlock()
		state = NewAgentState(AgentContext{AgentID: agentId}, a.config.MaxHistory)
		a.states[agentId] = state
		return state
	}

	ctx := agentDataToContext(agentData)
	a.mu.Lock()
	defer a.mu.Unlock()

	if state, ok := a.states[agentId]; ok {
		return state
	}
	state = NewAgentState(ctx, a.config.MaxHistory)
	a.states[agentId] = state
	return state
}

func (a *Advisor) buildMultiAgentSummary(excludeId string) string {
	allAgents := a.ts.GetAllAgents()
	if len(allAgents) <= 1 {
		return ""
	}

	var sb strings.Builder
	for _, agent := range allAgents {
		if agent.Id == excludeId {
			continue
		}

		elevStr := "no"
		if agent.Elevated {
			elevStr = "YES"
		}
		idShort := agent.Id
		if len(idShort) > 8 {
			idShort = idShort[:8]
		}
		fmt.Fprintf(&sb, "  [%s] %s\\%s@%s | %s | elevated=%s | PID %s (%s)\n",
			idShort, agent.Domain, agent.Username, agent.Computer,
			agent.OsDesc, elevStr, agent.Pid, agent.Process)

		a.mu.RLock()
		otherState, ok := a.states[agent.Id]
		a.mu.RUnlock()
		if ok && otherState.Recon != nil {
			discoveries := otherState.Recon.DiscoverySummary()
			if discoveries != "" {
				fmt.Fprintf(&sb, "    Discoveries:\n")
				for _, line := range strings.Split(strings.TrimRight(discoveries, "\n"), "\n") {
					fmt.Fprintf(&sb, "    %s\n", line)
				}
			}
		}
	}
	return sb.String()
}

func (a *Advisor) handleTaskComplete(agentId string, task adaptix.TaskData) {
	state := a.getOrCreateState(agentId)

	success := task.MessageType != 6
	entry := HistoryEntry{
		Timestamp:   time.Now().UTC(),
		CommandLine: task.CommandLine,
		Output:      task.ClearText,
		Success:     success,
	}
	state.AddHistory(entry)

	if success && task.CommandLine != "" {
		categories := classifyCommand(task.CommandLine)
		for _, cat := range categories {
			state.Recon.MarkCompleted(cat)
		}

		discoveries := parseDiscoveries(task.CommandLine, task.ClearText)
		for cat, items := range discoveries {
			state.Recon.AddDiscovery(cat, items...)
		}
	}

	if task.CommandLine == "" {
		return
	}

	ctx, cancel := context.WithTimeout(context.Background(), 90*time.Second)
	defer cancel()

	agentCtx := state.GetContext()
	history := state.GetHistory()

	reconSummary := state.Recon.Summary()
	discoverySummary := state.Recon.DiscoverySummary()
	if discoverySummary != "" {
		reconSummary += "\n  Discoveries:\n" + discoverySummary
	}

	otherAgents := a.buildMultiAgentSummary(agentId)

	systemPrompt := BuildSystemPrompt(a.config.Scope)
	userPrompt := BuildUserPrompt(agentCtx, history, task.CommandLine, task.ClearText, a.config.MaxOutputLen, reconSummary, otherAgents)

	response, err := a.llm.Complete(ctx, systemPrompt, userPrompt)
	if err != nil {
		logs.Warn("", "AI Advisor: LLM error for agent %s: %v", agentId, err)
		return
	}

	rec, err := parseRecommendation(response)
	if err != nil {
		logs.Warn("", "AI Advisor: parse error for agent %s: %v", agentId, err)
		return
	}
	rec.AgentID = agentId

	packet := SyncPacketAdvisor{
		SpType:       TYPE_ADVISOR_RECOMMENDATION,
		AgentID:      rec.AgentID,
		Command:      rec.Command,
		Rationale:    rec.Rationale,
		LookFor:      rec.LookFor,
		Priority:     rec.Priority,
		Category:     rec.Category,
		OpsecNote:    rec.OpsecNote,
		Alternatives: rec.Alternatives,
		Date:         time.Now().UTC().Unix(),
	}

	a.ts.SyncAdvisor(packet)
	logs.Info("", "AI Advisor: recommendation for %s: [%s] %s", agentId, rec.Category, rec.Command)
}

func parseRecommendation(response string) (*Recommendation, error) {
	response = strings.TrimSpace(response)

	if idx := strings.Index(response, "{"); idx >= 0 {
		if end := strings.LastIndex(response, "}"); end > idx {
			response = response[idx : end+1]
		}
	}

	var rec Recommendation
	if err := json.Unmarshal([]byte(response), &rec); err != nil {
		return nil, fmt.Errorf("invalid JSON from LLM: %w (response: %.200s)", err, response)
	}

	if rec.Command == "" {
		return nil, fmt.Errorf("empty command in recommendation")
	}

	switch rec.Priority {
	case "high", "medium", "low":
	default:
		rec.Priority = "medium"
	}

	switch rec.Category {
	case "recon", "privesc", "lateral", "creds", "persist", "exfil":
	default:
		rec.Category = "recon"
	}

	return &rec, nil
}

func agentDataToContext(agent adaptix.AgentData) AgentContext {
	return AgentContext{
		AgentID:    agent.Id,
		Hostname:   agent.Computer,
		Username:   agent.Username,
		Domain:     agent.Domain,
		OS:         strconv.Itoa(agent.Os),
		OsDesc:     agent.OsDesc,
		Arch:       agent.Arch,
		Elevated:   agent.Elevated,
		PID:        agent.Pid,
		Process:    agent.Process,
		InternalIP: agent.InternalIP,
		ExternalIP: agent.ExternalIP,
	}
}
