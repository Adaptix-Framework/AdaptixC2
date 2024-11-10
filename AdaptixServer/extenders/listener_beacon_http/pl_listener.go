package main

import (
	"encoding/json"
	"errors"
	"regexp"
	"strconv"
)

const (
	SetType     = TYPE_EXTERNAL
	SetProtocol = "http"
	SetName     = "BeaconHTTP"
	SetUiPath   = "_ui_listener.json"
)

var ListenersObject []any //*HTTP

func ValidateListenerConfig(data string) error {

	/// START CODE HERE

	var (
		err  error
		conf HTTPConfig
	)

	err = json.Unmarshal([]byte(data), &conf)
	if err != nil {
		return err
	}

	if conf.HostBind == "" {
		return errors.New("HostBind is required")
	}

	if conf.HostAgent == "" {
		return errors.New("HostAgent is required")
	}

	portBind, err := strconv.Atoi(conf.PortBind)
	if err != nil {
		return errors.New("PortBind must be an integer")
	}

	if portBind < 1 || portBind > 65535 {
		return errors.New("PortBind must be in the range 1-65535")
	}

	portAgent, err := strconv.Atoi(conf.PortBind)
	if err != nil {
		return errors.New("PortAgent must be an integer")
	}

	if portAgent < 1 || portAgent > 65535 {
		return errors.New("PortAgent must be in the range 1-65535")
	}

	matched, err := regexp.MatchString(`^/[a-zA-Z0-9]+(/[a-zA-Z0-9]+)*$`, conf.Uri)
	if err != nil || !matched {
		return errors.New("uri invalid")
	}

	/// END CODE

	return nil
}

func CreateListenerDataAndStart(name string, configData string) (ListenerData, any, error) {
	var listenerData ListenerData
	//var listener any

	/// START CODE HERE

	var (
		listener *HTTP
		conf     HTTPConfig
		err      error
	)

	err = json.Unmarshal([]byte(configData), &conf)
	if err != nil {
		return listenerData, listener, err
	}

	listener = NewConfigHttp(name)
	listener.Config = conf

	err = listener.Start()
	if err != nil {
		return listenerData, listener, err
	}

	listenerData = ListenerData{
		BindHost:  listener.Config.HostBind,
		BindPort:  listener.Config.PortBind,
		AgentHost: listener.Config.HostAgent,
		AgentPort: listener.Config.PortAgent,
		Status:    "Listen",
	}

	if !listener.Active {
		listenerData.Status = "Closed"
	}

	/// END CODE

	return listenerData, listener, nil
}

func EditListenerData(name string, listenerObject any, configData string) (ListenerData, bool) {
	var (
		listenerData ListenerData
		ok           bool = false
	)

	/// START CODE HERE

	var (
		err  error
		conf HTTPConfig
	)

	listener := listenerObject.(*HTTP)
	if listener.Name == name {

		err = json.Unmarshal([]byte(configData), &conf)
		if err != nil {
			return listenerData, false
		}

		listener.Config.Uri = conf.Uri

		listenerData = ListenerData{
			BindHost:  listener.Config.HostBind,
			BindPort:  listener.Config.PortBind,
			AgentHost: listener.Config.HostAgent,
			AgentPort: listener.Config.PortAgent,
			Status:    "Listen",
		}
		if !listener.Active {
			listenerData.Status = "Closed"
		}
		ok = true
	}

	/// END CODE

	return listenerData, ok
}

func StopListener(name string, listenerObject any) (bool, error) {
	var (
		err error = nil
		ok  bool  = false
	)

	/// START CODE HERE

	listener := listenerObject.(*HTTP)
	if listener.Name == name {
		err = listener.Stop()
		ok = true
	}

	/// END CODE

	return ok, err
}
