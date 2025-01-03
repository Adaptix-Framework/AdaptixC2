package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"regexp"
	"strconv"
	"strings"
)

const (
	SetType     = EXTERNAL
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

	if conf.Callback_servers == "" {
		return errors.New("callback_servers is required")
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

	matched, err := regexp.MatchString(`^/[a-zA-Z0-9\.\=\-]+(/[a-zA-Z0-9\.\=\-]+)*$`, conf.Uri)
	if err != nil || !matched {
		return errors.New("uri invalid")
	}

	if conf.HttpMethod == "" {
		return errors.New("http_method is required")
	}

	if conf.ParameterName == "" {
		return errors.New("hb_header is required")
	}

	if conf.UserAgent == "" {
		return errors.New("user_agent is required")
	}

	if !strings.Contains(conf.WebPageOutput, "<<<PAYLOAD_DATA>>>") {
		return errors.New("page-payload must contain '<<<PAYLOAD_DATA>>>' template")
	}

	/// END CODE

	return nil
}

func CreateListenerDataAndStart(name string, configData string, listenerCustomData []byte) (ListenerData, []byte, any, error) {
	var (
		listenerData ListenerData
		customdData  []byte
	)

	/// START CODE HERE

	var (
		listener *HTTP
		conf     HTTPConfig
		err      error
	)

	if listenerCustomData == nil {
		err = json.Unmarshal([]byte(configData), &conf)
		if err != nil {
			return listenerData, customdData, listener, err
		}

		conf.Callback_servers = strings.ReplaceAll(conf.Callback_servers, " ", "")
		conf.Callback_servers = strings.ReplaceAll(conf.Callback_servers, "\n", ", ")
		conf.Callback_servers = strings.TrimSuffix(conf.Callback_servers, ", ")

		conf.HostsAgent = strings.Split(conf.Callback_servers, ", ")

		conf.RequestHeaders = strings.TrimRight(conf.RequestHeaders, " \n\t\r") + "\n"
		conf.RequestHeaders = strings.ReplaceAll(conf.RequestHeaders, "\n", "\r\n")

		conf.ResponseHeaders = make(map[string]string)
		headerLine := strings.Split(conf.Server_headers, "\n")
		for _, line := range headerLine {
			line = strings.TrimSpace(line)
			if line == "" {
				continue
			}

			parts := strings.SplitN(line, ":", 2)
			if len(parts) != 2 {
				continue
			}
			key := strings.TrimSpace(parts[0])
			value := strings.TrimSpace(parts[1])

			conf.ResponseHeaders[key] = value
		}

		conf.Protocol = "http"
		conf.EncryptKey = []byte("\x0c\xff\x01\xb5\xfc\x46\x90\x57\x61\x98\x25\xe1\x87\x57\x21\x2e")

	} else {
		err = json.Unmarshal([]byte(listenerCustomData), &conf)
		if err != nil {
			return listenerData, customdData, listener, err
		}
	}

	listener = NewConfigHttp(name)
	listener.Config = conf

	err = listener.Start()
	if err != nil {
		return listenerData, customdData, listener, err
	}

	listenerData = ListenerData{
		BindHost:  listener.Config.HostBind,
		BindPort:  listener.Config.PortBind,
		AgentHost: conf.Callback_servers,
		AgentPort: listener.Config.PortAgent,
		Status:    "Listen",
	}

	if !listener.Active {
		listenerData.Status = "Closed"
	}

	var buffer bytes.Buffer
	err = json.NewEncoder(&buffer).Encode(listener.Config)
	if err != nil {
		return listenerData, customdData, listener, nil
	}
	customdData = buffer.Bytes()

	/// END CODE

	return listenerData, customdData, listener, nil
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

func GetProfile(name string, listenerObject any) ([]byte, bool) {
	var (
		object bytes.Buffer
		ok     bool = false
	)

	/// START CODE HERE

	listener := listenerObject.(*HTTP)
	if listener.Name == name {
		_ = json.NewEncoder(&object).Encode(listener.Config)
		ok = true
	}

	/// END CODE

	return object.Bytes(), ok
}
