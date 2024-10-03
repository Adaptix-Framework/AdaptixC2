package main

import (
	"bytes"
	"encoding/json"
	"os"
	"path/filepath"
	"runtime"
)

const (
	TYPE_AGENT = "agent"
)

type Teamserver interface{}

type ModuleExtender struct {
	ts Teamserver
}

type ModuleInfo struct {
	ModuleName string
	ModuleType string
}

type AgentInfo struct {
	AgentName    string
	ListenerName string
	AgentUI      string
}

var ModuleObject ModuleExtender
var ModulePath string

////////////////////////////

const (
	SetName     = "beacon"
	SetListener = "BeaconHTTP"
	SetUiPath   = "ui_agent.json"
)

////////////////////////////

func (m *ModuleExtender) InitPlugin(ts any) ([]byte, error) {
	var (
		buffer bytes.Buffer
		err    error
	)

	ModuleObject.ts = ts.(Teamserver)

	info := ModuleInfo{
		ModuleName: SetName,
		ModuleType: TYPE_AGENT,
	}

	err = json.NewEncoder(&buffer).Encode(info)
	if err != nil {
		return nil, err
	}

	return buffer.Bytes(), nil
}

func (m *ModuleExtender) AgentInit() ([]byte, error) {
	var (
		buffer bytes.Buffer
		err    error
	)

	_, file, _, _ := runtime.Caller(0)
	dir := filepath.Dir(file)
	uiPath := filepath.Join(dir, SetUiPath)
	agentUI, err := os.ReadFile(uiPath)
	if err != nil {
		return nil, err
	}

	info := AgentInfo{
		AgentName:    SetName,
		ListenerName: SetListener,
		AgentUI:      string(agentUI),
	}

	err = json.NewEncoder(&buffer).Encode(info)
	if err != nil {
		return nil, err
	}

	return buffer.Bytes(), nil
}

////////////////////////////

func (m *ModuleExtender) AgentValid(data string) error {
	return nil
}
