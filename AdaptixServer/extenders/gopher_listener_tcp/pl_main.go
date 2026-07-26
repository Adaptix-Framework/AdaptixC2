package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"strconv"
	"strings"

	"github.com/Adaptix-Framework/axc2/v2"
)

type PluginListener struct{}

const (
	logSrc = "listener"
	logCtg = "Gopher_TCP"
)

var (
	ModuleDir       string
	ListenerDataDir string
	Ts              adaptix.Teamserver
)

func InitPlugin(ts any, moduleDir string, listenerDir string) adaptix.PluginListener {
	ModuleDir = moduleDir
	ListenerDataDir = listenerDir
	Ts = ts.(adaptix.Teamserver)
	return &PluginListener{}
}

func (p *PluginListener) Create(name string, config string, customData []byte) (adaptix.ExtenderListener, adaptix.ListenerData, []byte, error) {
	var (
		listener     *Listener
		listenerData adaptix.ListenerData
		customdData  []byte
		conf         TransportConfig
		err          error
	)

	/// START CODE HERE

	if customData == nil {
		if err = validConfig(config); err != nil {
			return nil, listenerData, customdData, err
		}

		err = json.Unmarshal([]byte(config), &conf)
		if err != nil {
			return nil, listenerData, customdData, err
		}

		conf.Callback_addresses = strings.ReplaceAll(conf.Callback_addresses, " ", "")
		conf.Callback_addresses = strings.ReplaceAll(conf.Callback_addresses, "\n", ", ")
		conf.Callback_addresses = strings.TrimSuffix(conf.Callback_addresses, ", ")

		conf.Protocol = "tcp"
	} else {
		err = json.Unmarshal(customData, &conf)
		if err != nil {
			return nil, listenerData, customdData, err
		}
	}

	transport := &TransportTCP{
		Name:          name,
		Config:        conf,
		AgentConnects: NewMap[int64, Connection](),
		JobConnects:   NewMap[string, Connection](),
		Active:        false,
	}

	listenerData = adaptix.ListenerData{
		BindHost:  transport.Config.HostBind,
		BindPort:  strconv.Itoa(transport.Config.PortBind),
		AgentAddr: conf.Callback_addresses,
		Status:    "Stopped",
	}

	if transport.Config.Ssl {
		listenerData.Protocol = "mtls"
	}

	var buffer bytes.Buffer
	err = json.NewEncoder(&buffer).Encode(transport.Config)
	if err != nil {
		return nil, listenerData, customdData, err
	}
	customdData = buffer.Bytes()

	listener = &Listener{transport: transport}

	/// END CODE HERE

	return listener, listenerData, customdData, nil
}

func (l *Listener) Start() error {
	/// START CODE HERE

	return l.transport.Start(Ts)

	/// END CODE HERE
}

func (l *Listener) Edit(config string) (adaptix.ListenerData, []byte, error) {
	var (
		listenerData adaptix.ListenerData
		customdData  []byte
		conf         TransportConfig
		err          error
	)

	err = json.Unmarshal([]byte(config), &conf)
	if err != nil {
		return listenerData, customdData, err
	}

	/// START CODE HERE

	conf.Callback_addresses = strings.ReplaceAll(conf.Callback_addresses, " ", "")
	conf.Callback_addresses = strings.ReplaceAll(conf.Callback_addresses, "\n", ", ")
	conf.Callback_addresses = strings.TrimSuffix(conf.Callback_addresses, ", ")

	l.transport.Config.Callback_addresses = conf.Callback_addresses
	l.transport.Config.TcpBanner = conf.TcpBanner
	l.transport.Config.ErrorAnswer = conf.ErrorAnswer
	l.transport.Config.Timeout = conf.Timeout

	listenerData = adaptix.ListenerData{
		BindHost:  l.transport.Config.HostBind,
		BindPort:  strconv.Itoa(l.transport.Config.PortBind),
		AgentAddr: l.transport.Config.Callback_addresses,
		Status:    "Listen",
	}
	if !l.transport.Active {
		listenerData.Status = "Closed"
	}

	var buffer bytes.Buffer
	err = json.NewEncoder(&buffer).Encode(l.transport.Config)
	if err != nil {
		return listenerData, customdData, err
	}
	customdData = buffer.Bytes()

	/// END CODE HERE

	return listenerData, customdData, nil
}

func (l *Listener) Stop() error {
	/// START CODE HERE

	return l.transport.Stop()

	/// END CODE HERE
}

func (l *Listener) GetProfile() ([]byte, error) {
	var buffer bytes.Buffer

	/// START CODE HERE

	err := json.NewEncoder(&buffer).Encode(l.transport.Config)
	if err != nil {
		return nil, err
	}

	/// END CODE HERE

	return buffer.Bytes(), nil
}

func (l *Listener) InternalHandler(data []byte) (int64, error) {
	var agentId int64 = 0

	/// START CODE HERE

	/// END CODE HERE

	return agentId, errors.New("not implemented")
}
