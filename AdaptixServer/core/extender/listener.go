package extender

import "errors"

func (ex *AdaptixExtender) ListenerStart(listenerName string, configType string, config string) error {
	var (
		err    error
		module *ModuleExtender
	)

	if ex.listenerModules.Contains(configType) {
		value, ok := ex.listenerModules.Get(configType)
		if ok {
			module = value.(*ModuleExtender)
			err = module.ListenerValid(config)
			if err != nil {
				return err
			}
		}
	} else {
		return errors.New("module not found")
	}

	return nil
}
