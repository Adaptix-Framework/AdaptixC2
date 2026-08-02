package extender

import (
	"fmt"

	"github.com/Adaptix-Framework/axc2/v2"
)

func (ex *AdaptixExtender) ExPluginAgentCall(agentType string, operator string, agentId int64, function string, args string) {
	module, err := ex.getAgentModule(agentType)
	if err != nil {
		if ex.ts != nil {
			payload := fmt.Sprintf(`{"action":"error","success":false,"error":"agent plugin not loaded: %s","agent_id":%d}`, agentType, agentId)
			ex.ts.TsPluginAgentSendDataClient(operator, agentId, payload)
		}
		return
	}
	module.Call(operator, agentId, function, args)
}

func (ex *AdaptixExtender) ExAgentGenerate(agentName string, generateConfig adaptix.BuildProfile) ([]byte, string, error) {
	module, err := ex.getAgentModule(agentName)
	if err != nil {
		return nil, "", err
	}

	agentProfile, err := module.GenerateProfiles(generateConfig)
	if err != nil {
		return nil, "", err
	}

	return module.BuildPayload(generateConfig, agentProfile)
}

func (ex *AdaptixExtender) ExAgentCreate(agentName string, beat []byte) (adaptix.AgentData, adaptix.AgentFunctions, error) {
	module, err := ex.getAgentModule(agentName)
	if err != nil {
		return adaptix.AgentData{}, adaptix.AgentFunctions{}, err
	}
	return module.CreateAgent(beat)
}

func (ex *AdaptixExtender) ExAgentRestore(agentName string, agentData adaptix.AgentData) (adaptix.AgentFunctions, error) {
	module, err := ex.getAgentModule(agentName)
	if err != nil {
		return adaptix.AgentFunctions{}, err
	}
	return module.AgentRestore(agentData), nil
}
