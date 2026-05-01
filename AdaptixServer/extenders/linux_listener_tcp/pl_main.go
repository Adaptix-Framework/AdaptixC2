package main

import (
	"bytes"
	"crypto/aes"
	"crypto/cipher"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"

	adaptix "github.com/Adaptix-Framework/axc2"
	"github.com/vmihailenco/msgpack/v5"
)

type Teamserver interface {
	TsAgentIsExists(agentId string) bool
	TsAgentCreate(agentCrc string, agentId string, beat []byte, listenerName string, ExternalIP string, Async bool) (adaptix.AgentData, error)
}

type PluginListener struct{}

var (
	ModuleDir       string
	ListenerDataDir string
	Ts              Teamserver
)

// Msgpack structs matching Linux agent's build_init_msg format
type StartMsg struct {
	Id   int    `msgpack:"id"`
	Data []byte `msgpack:"data"`
}

type InitPack struct {
	Id   uint   `msgpack:"id"`
	Type uint   `msgpack:"type"`
	Data []byte `msgpack:"data"`
}

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
		customdData  []byte
		conf         TransportConfig
		err          error
	)

	if customData == nil {
		if err = validConfig(config); err != nil {
			return nil, listenerData, customdData, err
		}

		err = json.Unmarshal([]byte(config), &conf)
		if err != nil {
			return nil, listenerData, customdData, err
		}

		conf.Protocol = "bind_tcp"
	} else {
		err = json.Unmarshal(customData, &conf)
		if err != nil {
			return nil, listenerData, customdData, err
		}
	}

	transport := &TransportTCP{
		Name:   name,
		Config: conf,
		Active: false,
	}

	listenerData = adaptix.ListenerData{
		BindHost:  "",
		BindPort:  "",
		AgentAddr: fmt.Sprintf("0.0.0.0:%d", transport.Config.Port),
		Status:    "Stopped",
	}

	var buffer bytes.Buffer
	err = json.NewEncoder(&buffer).Encode(transport.Config)
	if err != nil {
		return nil, listenerData, customdData, err
	}
	customdData = buffer.Bytes()

	listener = &Listener{transport: transport}

	return listener, listenerData, customdData, nil
}

func (l *Listener) Start() error {
	l.transport.Active = true
	return nil
}

func (l *Listener) Edit(config string) (adaptix.ListenerData, []byte, error) {
	var (
		listenerData adaptix.ListenerData
		customdData  []byte
	)

	listenerData = adaptix.ListenerData{
		BindHost:  "",
		BindPort:  "",
		AgentAddr: fmt.Sprintf("0.0.0.0:%d", l.transport.Config.Port),
		Status:    "Listen",
	}

	var buffer bytes.Buffer
	err := json.NewEncoder(&buffer).Encode(l.transport.Config)
	if err != nil {
		return listenerData, customdData, err
	}
	customdData = buffer.Bytes()

	return listenerData, customdData, nil
}

func (l *Listener) Stop() error {
	l.transport.Active = false
	return nil
}

func (l *Listener) GetProfile() ([]byte, error) {
	var buffer bytes.Buffer

	err := json.NewEncoder(&buffer).Encode(l.transport.Config)
	if err != nil {
		return nil, err
	}

	return buffer.Bytes(), nil
}

func (l *Listener) InternalHandler(data []byte) (string, error) {
	var agentId = ""

	// Decrypt with AES-128-GCM (16-byte key)
	encKey, err := hex.DecodeString(l.transport.Config.EncryptKey)
	if err != nil {
		return "", err
	}
	if len(encKey) != 16 {
		return "", errors.New("encrypt_key must be 16 bytes for AES-128")
	}

	block, err := aes.NewCipher(encKey)
	if err != nil {
		return "", err
	}
	gcm, err := cipher.NewGCM(block)
	if err != nil {
		return "", err
	}

	nonceSize := gcm.NonceSize()
	if len(data) < nonceSize+gcm.Overhead() {
		return "", errors.New("beat ciphertext too short")
	}

	nonce, ciphertext := data[:nonceSize], data[nonceSize:]
	plaintext, err := gcm.Open(nil, nonce, ciphertext, nil)
	if err != nil {
		return "", errors.New("aes-128-gcm decrypt error")
	}

	// Parse msgpack: StartMsg{id, data} → InitPack{id, type, data}
	var startMsg StartMsg
	if err = msgpack.Unmarshal(plaintext, &startMsg); err != nil {
		return "", fmt.Errorf("msgpack StartMsg decode error: %v", err)
	}

	var initPack InitPack
	if err = msgpack.Unmarshal(startMsg.Data, &initPack); err != nil {
		return "", fmt.Errorf("msgpack InitPack decode error: %v", err)
	}

	agentType := fmt.Sprintf("%08x", initPack.Type)
	agentId = fmt.Sprintf("%08x", initPack.Id)

	if !Ts.TsAgentIsExists(agentId) {
		_, err = Ts.TsAgentCreate(agentType, agentId, initPack.Data, l.transport.Name, "", false)
		if err != nil {
			return agentId, err
		}
	}

	return agentId, nil
}
