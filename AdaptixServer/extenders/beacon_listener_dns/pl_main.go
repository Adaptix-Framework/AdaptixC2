package main

import (
	"bytes"
	"encoding/json"
	mrand "math/rand/v2"
	"strconv"
	"strings"
	"time"

	adaptix "github.com/Adaptix-Framework/axc2"
)

type Teamserver interface {
	TsAgentIsExists(agentId string) bool
	TsAgentCreate(agentCrc string, agentId string, beat []byte, listenerName string, ExternalIP string, Async bool) (adaptix.AgentData, error)
	TsAgentProcessData(agentId string, bodyData []byte) error
	TsAgentSetTick(agentId string, listenerName string) error
	TsAgentGetHostedAll(agentId string, maxDataSize int) ([]byte, error)
}

type PluginListener struct{}

var (
	ModuleDir       string
	ListenerDataDir string
	Ts              Teamserver
)

func InitPlugin(ts any, moduleDir string, listenerDir string) adaptix.PluginListener {
	ModuleDir = moduleDir
	ListenerDataDir = listenerDir
	Ts = ts.(Teamserver)
	return &PluginListener{}
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
		//conf.EncryptKey = normalizeEncryptKey(conf.EncryptKey)
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
	if conf.BurstSleep <= 0 {
		conf.BurstSleep = 50
	}
	if conf.BurstJitter < 0 || conf.BurstJitter > 90 {
		conf.BurstJitter = 0
	}
	if conf.TTL <= 0 {
		conf.TTL = 10
	}
	if conf.PktSize <= 0 || conf.PktSize > 64000 {
		conf.PktSize = defaultChunkSize
	}

	transport := &TransportDNS{
		//ts:             Ts,
		Name:           name,
		Config:         conf,
		rng:            mrand.New(mrand.NewPCG(uint64(time.Now().UnixNano()), uint64(time.Now().UnixNano()))),
		upFrags:        make(map[string]*dnsFragBuf),
		downFrags:      make(map[string]*dnsDownBuf),
		upDoneCache:    make(map[string]*dnsUpDone),
		localInflights: make(map[string]*localInflight),
		needsReset:     make(map[string]bool),
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
	if conf.BurstSleep > 0 {
		l.transport.Config.BurstSleep = conf.BurstSleep
	}
	if conf.BurstJitter >= 0 {
		l.transport.Config.BurstJitter = conf.BurstJitter
	}
	l.transport.Config.BurstEnabled = conf.BurstEnabled

	/////

	listenerData = adaptix.ListenerData{
		BindHost:  l.transport.Config.HostBind,
		BindPort:  strconv.Itoa(l.transport.Config.PortBind),
		AgentAddr: l.transport.Config.Domain,
		Status:    "Listen",
		Protocol:  "dns",
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

func (l *Listener) InternalHandler(data []byte) (string, error) {
	var agentId = ""

	/// START CODE HERE

	/// END CODE HERE

	return agentId, nil
}
