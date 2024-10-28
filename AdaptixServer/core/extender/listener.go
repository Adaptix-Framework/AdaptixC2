package extender

import "errors"

func (ex *AdaptixExtender) ListenerStart(listenerName string, configType string, config string) ([]byte, error) {
	var (
		err    error
		module *ModuleExtender
	)

	value, ok := ex.listenerModules.Get(configType)
	if ok {
		module = value.(*ModuleExtender)
		err = module.ListenerValid(config)
		if err != nil {
			return nil, err
		}
		return module.ListenerStart(listenerName, config)

	} else {
		return nil, errors.New("module not found")
	}
}

func (ex *AdaptixExtender) ListenerStop(listenerName string, configType string) error {
	var module *ModuleExtender

	value, ok := ex.listenerModules.Get(configType)
	if ok {
		module = value.(*ModuleExtender)
		return module.ListenerStop(listenerName)

	} else {
		return errors.New("module not found")
	}
}

func (ex *AdaptixExtender) ListenerEdit(listenerName string, configType string, config string) ([]byte, error) {
	var module *ModuleExtender

	value, ok := ex.listenerModules.Get(configType)
	if ok {
		module = value.(*ModuleExtender)
		return module.ListenerEdit(listenerName, config)

	} else {
		return nil, errors.New("module not found")
	}
}
