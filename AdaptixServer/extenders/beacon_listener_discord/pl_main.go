package main

import (
	"bytes"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"regexp"

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
		conf         ConfigDiscord
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

		conf.encryptKeyBytes, err = hex.DecodeString(conf.EncryptKey)
		if err != nil {
			return nil, listenerData, customdData, fmt.Errorf("invalid encrypt_key hex: %v", err)
		}

	} else {
		err = json.Unmarshal(customData, &conf)
		if err != nil {
			return nil, listenerData, customdData, err
		}

		conf.encryptKeyBytes, err = hex.DecodeString(conf.EncryptKey)
		if err != nil {
			return nil, listenerData, customdData, fmt.Errorf("invalid encrypt_key hex: %v", err)
		}
	}

	transport := &TransportDiscord{
		Name:   name,
		Config: conf,
		Active: false,
	}

	listenerData = adaptix.ListenerData{
		BindHost:  "discord",
		BindPort:  "0",
		AgentAddr: conf.WebhookUrl,
		Protocol:  "discord",
		Status:    "Stopped",
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
		conf         ConfigDiscord
		customdData  []byte
		err          error
	)

	err = json.Unmarshal([]byte(config), &conf)
	if err != nil {
		return listenerData, customdData, err
	}

	/// START CODE HERE

	l.transport.Config.WebhookUrl = conf.WebhookUrl
	l.transport.Config.PollInterval = conf.PollInterval
	l.transport.Config.Cleanup = conf.Cleanup

	listenerData = adaptix.ListenerData{
		BindHost:  "discord",
		BindPort:  "0",
		AgentAddr: l.transport.Config.WebhookUrl,
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

	// Return only what the beacon needs: webhook URL, channel IDs, poll interval, encrypt key
	profile := map[string]any{
		"protocol":       "discord",
		"webhook_url":    l.transport.Config.WebhookUrl,
		"bot_token":      l.transport.Config.BotToken,
		"channel_beacon": l.transport.Config.ChannelBeacon,
		"channel_tasks_id": l.transport.Config.ChannelTasks,
		"poll_interval":  l.transport.Config.PollInterval,
		"encrypt_key":    l.transport.Config.EncryptKey,
		"cleanup":        l.transport.Config.Cleanup,
	}

	err := json.NewEncoder(&buffer).Encode(profile)
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

func validConfig(config string) error {
	var conf ConfigDiscord
	err := json.Unmarshal([]byte(config), &conf)
	if err != nil {
		return err
	}

	if conf.BotToken == "" {
		return errors.New("bot_token is required")
	}

	if conf.ChannelBeacon == "" {
		return errors.New("channel_beacon is required")
	}
	matchChan, _ := regexp.MatchString("^[0-9]+$", conf.ChannelBeacon)
	if !matchChan {
		return errors.New("channel_beacon must be a numeric Discord channel ID")
	}

	if conf.ChannelTasks == "" {
		return errors.New("channel_tasks is required")
	}
	matchChan, _ = regexp.MatchString("^[0-9]+$", conf.ChannelTasks)
	if !matchChan {
		return errors.New("channel_tasks must be a numeric Discord channel ID")
	}

	if conf.WebhookUrl == "" {
		return errors.New("webhook_url is required")
	}
	matchWebhook, _ := regexp.MatchString("^https://discord\\.com/api/webhooks/", conf.WebhookUrl)
	if !matchWebhook {
		return errors.New("webhook_url must be a valid Discord webhook URL")
	}

	if conf.PollInterval < 1 || conf.PollInterval > 60 {
		return errors.New("poll_interval must be between 1 and 60 seconds")
	}

	match, _ := regexp.MatchString("^[0-9a-f]{64}$", conf.EncryptKey)
	if len(conf.EncryptKey) != 64 || !match {
		return errors.New("encrypt_key must be 64 hex characters (32 bytes for AES-256)")
	}

	return nil
}
