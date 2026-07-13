package extender

import "github.com/Adaptix-Framework/axc2/v2"

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
