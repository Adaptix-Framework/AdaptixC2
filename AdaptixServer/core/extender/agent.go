package extender

import "errors"

func (ex *AdaptixExtender) AgentCreate(agentName string, beat []byte) ([]byte, error) {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentCreate(beat)
	} else {
		return nil, errors.New("module not found")
	}
}

func (ex *AdaptixExtender) AgentProcessData(agentName string, agentId string, data []byte) ([]byte, error) {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentProcessData(agentId, data)
	} else {
		return nil, errors.New("module not found")
	}
}

func (ex *AdaptixExtender) AgentPackData(agentName string, dataAgent []byte, dataTasks [][]byte) ([]byte, error) {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentPackData(dataAgent, dataTasks)
	} else {
		return nil, errors.New("module not found")
	}
}

func (ex *AdaptixExtender) AgentCommand(agentName string, agentObject []byte, args map[string]any) ([]byte, error) {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentCommand(agentObject, args)
	} else {
		return nil, errors.New("module not found")
	}
}
