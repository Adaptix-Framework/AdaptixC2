package extender

import "errors"

func (ex *AdaptixExtender) AgentCreateData(agentName string, beat []byte) ([]byte, error) {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentCreateData(beat)
	} else {
		return nil, errors.New("module not found")
	}
}

func (ex *AdaptixExtender) AgentProcess(agentName string, agentId string, data []byte) ([]byte, error) {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentProcess(agentId, data)
	} else {
		return nil, errors.New("module not found")
	}
}

func (ex *AdaptixExtender) AgentCommand(agentName string, data any) ([]byte, error) {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentCommand(data)
	} else {
		return nil, errors.New("module not found")
	}
}
