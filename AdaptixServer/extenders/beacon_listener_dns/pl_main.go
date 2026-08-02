package main

import (
	"bytes"
	"encoding/json"
	"strconv"
	"strings"

	"github.com/Adaptix-Framework/axc2/v2"
)

const (
	logSrc = "listener"
	logCtg = "beacon_dns"
)

type PluginListener struct{}

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

func (p *PluginListener) Call(operator string, listenerName string, function string, args string) {
	_ = operator
	_ = listenerName
	_ = function
	_ = args
}

func (p *PluginListener) Create(name string, config string, customData []byte) (adaptix.ExtenderListener, adaptix.ListenerData, []byte, error) {
	var (
		listener     *Listener
		listenerData adaptix.ListenerData
		conf         TransportConfig
		customdData  []byte
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
	} else {
		err = json.Unmarshal(customData, &conf)
		if err != nil {
			return nil, listenerData, customdData, err
		}
	}
	conf.Protocol = "dns"
	for _, d := range strings.Split(conf.Domain, ",") {
		d = strings.TrimSpace(d)
		d = strings.ToLower(d)
		d = strings.TrimSuffix(d, ".")
		if d != "" {
			conf.Domains = append(conf.Domains, d)
		}
	}
	if conf.TTL <= 0 {
		conf.TTL = 10
	}
	if conf.PktSize <= 0 || conf.PktSize > 64000 {
		conf.PktSize = defaultChunkSize
	}
	if conf.BurstSleep <= 0 {
		conf.BurstSleep = 50
	}
	if conf.BurstJitter < 0 || conf.BurstJitter > 90 {
		conf.BurstJitter = 0
	}

	transport := &TransportDNS{
		Name:   name,
		Config: conf,
	}

	listenerData = adaptix.ListenerData{
		BindHost:  transport.Config.HostBind,
		BindPort:  strconv.Itoa(transport.Config.PortBind),
		AgentAddr: transport.Config.Domain,
		Status:    "Stopped",
		Protocol:  "dns",
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
		conf         TransportConfig
		customdData  []byte
		err          error
	)

	err = json.Unmarshal([]byte(config), &conf)
	if err != nil {
		return listenerData, customdData, err
	}

	/// START CODE HERE

	if conf.Domain != "" {
		l.transport.Config.Domain = conf.Domain
		l.transport.Config.Domains = nil
		for _, d := range strings.Split(conf.Domain, ",") {
			d = strings.TrimSpace(d)
			d = strings.ToLower(d)
			d = strings.TrimSuffix(d, ".")
			if d != "" {
				l.transport.Config.Domains = append(l.transport.Config.Domains, d)
			}
		}
	}
	if conf.TTL != 0 {
		l.transport.Config.TTL = conf.TTL
	}
	if conf.PktSize != 0 {
		l.transport.Config.PktSize = conf.PktSize
	}
	l.transport.Config.BurstEnabled = conf.BurstEnabled
	if conf.BurstSleep > 0 {
		l.transport.Config.BurstSleep = conf.BurstSleep
	}
	if conf.BurstJitter >= 0 && conf.BurstJitter <= 90 {
		l.transport.Config.BurstJitter = conf.BurstJitter
	}

	listenerData = adaptix.ListenerData{
		BindHost:  l.transport.Config.HostBind,
		BindPort:  strconv.Itoa(l.transport.Config.PortBind),
		AgentAddr: l.transport.Config.Domain,
		Status:    "Listen",
		Protocol:  "dns",
	}
	if !l.transport.IsActive() {
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

	return agentId, nil
}
