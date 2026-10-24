package server

import (
	"AdaptixServer/core/advisor"

	"github.com/Adaptix-Framework/axc2"
)

func (ts *Teamserver) GetAgent(agentId string) (adaptix.AgentData, bool) {
	value, ok := ts.Agents.Get(agentId)
	if !ok {
		return adaptix.AgentData{}, false
	}
	agent, ok := value.(*Agent)
	if !ok {
		return adaptix.AgentData{}, false
	}
	return agent.GetData(), true
}

func (ts *Teamserver) GetAllAgents() []adaptix.AgentData {
	var agents []adaptix.AgentData
	ts.Agents.ForEach(func(key string, value interface{}) bool {
		agent, ok := value.(*Agent)
		if ok {
			agents = append(agents, agent.GetData())
		}
		return true
	})
	return agents
}

func (ts *Teamserver) SyncAdvisor(packet interface{}) {
	ts.TsSyncAllClients(packet)
}

func (ts *Teamserver) InitAdvisor() {
	if ts.Profile.Advisor == nil {
		return
	}

	config := advisor.AdvisorConfig{
		Enabled:      ts.Profile.Advisor.Enabled,
		Provider:     ts.Profile.Advisor.Provider,
		ApiKey:       ts.Profile.Advisor.ApiKey,
		Model:        ts.Profile.Advisor.Model,
		Scope:        ts.Profile.Advisor.Scope,
		Endpoint:     ts.Profile.Advisor.Endpoint,
		MaxHistory:   ts.Profile.Advisor.MaxHistory,
		RedactCreds:  ts.Profile.Advisor.RedactCreds,
		MaxOutputLen: ts.Profile.Advisor.MaxOutputLen,
	}

	ts.Advisor = advisor.NewAdvisor(config, ts)
	ts.Advisor.RegisterHooks(ts.EventManager)
}
