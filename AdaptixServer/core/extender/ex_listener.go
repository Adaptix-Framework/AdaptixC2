package extender

import adaptix "github.com/Adaptix-Framework/axc2"

func (ex *AdaptixExtender) ExListenerCreate(listenerName string, configType string, config string, listenerCustomData []byte) (adaptix.ListenerData, []byte, error) {
	module, err := ex.getListenerModule(configType)
	if err != nil {
		return adaptix.ListenerData{}, nil, err
	}

	listener, listenerData, customData, err := module.Create(listenerName, config, listenerCustomData)
	if err != nil {
		return listenerData, customData, err
	}

	ex.activeListeners[listenerName] = listener

	return listenerData, customData, nil
}

func (ex *AdaptixExtender) ExListenerStart(listenerName string) error {
	listener, err := ex.getActiveListener(listenerName)
	if err != nil {
		return err
	}
	return listener.Start()
}

func (ex *AdaptixExtender) ExListenerEdit(listenerName string, config string) (adaptix.ListenerData, []byte, error) {
	listener, err := ex.getActiveListener(listenerName)
	if err != nil {
		return adaptix.ListenerData{}, nil, err
	}
	return listener.Edit(config)
}

func (ex *AdaptixExtender) ExListenerStop(listenerName string) error {
	listener, err := ex.getActiveListener(listenerName)
	if err != nil {
		return err
	}

	err = listener.Stop()
	delete(ex.activeListeners, listenerName)

	return err
}

func (ex *AdaptixExtender) ExListenerPause(listenerName string) error {
	listener, err := ex.getActiveListener(listenerName)
	if err != nil {
		return err
	}
	return listener.Stop()
}

func (ex *AdaptixExtender) ExListenerResume(listenerName string) error {
	listener, err := ex.getActiveListener(listenerName)
	if err != nil {
		return err
	}
	return listener.Start()
}

func (ex *AdaptixExtender) ExListenerGetProfile(listenerName string) ([]byte, error) {
	listener, err := ex.getActiveListener(listenerName)
	if err != nil {
		return nil, err
	}
	return listener.GetProfile()
}

func (ex *AdaptixExtender) ExListenerInternalHandler(listenerName string, data []byte) (string, error) {
	listener, err := ex.getActiveListener(listenerName)
	if err != nil {
		return "", err
	}
	return listener.InternalHandler(data)
}
