package extender

import adaptix "github.com/Adaptix-Framework/axc2"

func (ex *AdaptixExtender) ExAgentGenerate(agentName string, config string, listenerWM string, listenerProfile []byte) ([]byte, string, error) {
	factory, err := ex.getAgentFactory(agentName)
	if err != nil {
		return nil, "", err
	}

	agentConfig, err := factory.GenerateConfig(config, listenerWM, listenerProfile)
	if err != nil {
		return nil, "", err
	}

	return factory.BuildPayload(config, agentConfig, listenerProfile)
}

func (ex *AdaptixExtender) ExAgentCreate(agentName string, beat []byte) (adaptix.AgentData, adaptix.AgentHandler, error) {
	factory, err := ex.getAgentFactory(agentName)
	if err != nil {
		return adaptix.AgentData{}, nil, err
	}
	return factory.CreateAgent(beat)
}
