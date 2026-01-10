package extender

import adaptix "github.com/Adaptix-Framework/axc2"

func (ex *AdaptixExtender) ExAgentGenerate(agentName string, config string, listenerWM string, listenerProfile []byte, builderId string) ([]byte, string, error) {
	module, err := ex.getAgentModule(agentName)
	if err != nil {
		return nil, "", err
	}

	agentConfig, err := module.GenerateConfig(config, listenerWM, listenerProfile)
	if err != nil {
		return nil, "", err
	}

	return module.BuildPayload(config, agentConfig, listenerProfile, builderId)
}

func (ex *AdaptixExtender) ExAgentCreate(agentName string, beat []byte) (adaptix.AgentData, adaptix.ExtenderAgent, error) {
	module, err := ex.getAgentModule(agentName)
	if err != nil {
		return adaptix.AgentData{}, nil, err
	}
	return module.CreateAgent(beat)
}

func (ex *AdaptixExtender) ExAgentGetExtender(agentName string) (adaptix.ExtenderAgent, error) {
	module, err := ex.getAgentModule(agentName)
	if err != nil {
		return nil, err
	}
	return module.GetExtender(), nil
}
