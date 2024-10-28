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
