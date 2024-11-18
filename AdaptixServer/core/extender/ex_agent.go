package extender

import "errors"

func (ex *AdaptixExtender) ExAgentCreate(agentName string, beat []byte) ([]byte, error) {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentCreate(beat)
	} else {
		return nil, errors.New("module not found")
	}
}

func (ex *AdaptixExtender) ExAgentProcessData(agentName string, agentObject []byte, packedData []byte) ([]byte, error) {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentProcessData(agentObject, packedData)
	} else {
		return nil, errors.New("module not found")
	}
}

func (ex *AdaptixExtender) ExAgentPackData(agentName string, agentObject []byte, dataTasks [][]byte) ([]byte, error) {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentPackData(agentObject, dataTasks)
	} else {
		return nil, errors.New("module not found")
	}
}

func (ex *AdaptixExtender) ExAgentCommand(agentName string, agentObject []byte, args map[string]any) ([]byte, string, error) {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentCommand(agentObject, args)
	} else {
		return nil, "", errors.New("module not found")
	}
}

func (ex *AdaptixExtender) ExAgentDownloadChangeState(agentName string, agentObject []byte, newState int, fileId string) error {
	var module *ModuleExtender

	value, ok := ex.agentModules.Get(agentName)
	if ok {
		module = value.(*ModuleExtender)
		return module.AgentDownloadChangeState(agentObject, newState, fileId)
	} else {
		return errors.New("module not found")
	}
}
