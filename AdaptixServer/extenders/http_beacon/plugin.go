package main

import (
	"bytes"
	"encoding/json"
	"os"
	"path/filepath"
	"runtime"
)

const (
	TYPE_LISTENER = "listener"

	TYPE_INTERNAL = "internal"
	TYPE_EXTERNAL = "external"
)

type Teamserver interface{}

type ModuleExtender struct {
	ts Teamserver
}

type ModuleInfo struct {
	ModuleName string
	ModuleType string
}

type ListenerInfo struct {
	ListenerType     string
	ListenerProtocol string
	ListenerName     string
	ListenerUI       string
}

var ModuleObject ModuleExtender

////////////////////////////

const (
	SetType     = "external"
	SetProtocol = "http"
	SetName     = "BeaconHTTP"
	SetUiPath   = "ui_listener.json"
)

////////////////////////////

func (m *ModuleExtender) InitPlugin(ts any) ([]byte, error) {
	var (
		buffer bytes.Buffer
		err    error
	)

	ModuleObject.ts = ts.(Teamserver)

	info := ModuleInfo{
		ModuleName: "BeaconHTTP",
		ModuleType: TYPE_LISTENER,
	}

	err = json.NewEncoder(&buffer).Encode(info)
	if err != nil {
		return nil, err
	}

	return buffer.Bytes(), nil
}

////////////////////////////

func (m *ModuleExtender) ListenerInit() ([]byte, error) {
	var (
		buffer bytes.Buffer
		err    error
	)

	_, file, _, _ := runtime.Caller(0)
	dir := filepath.Dir(file)
	uiPath := filepath.Join(dir, SetUiPath)
	listenerUI, err := os.ReadFile(uiPath)
	if err != nil {
		return nil, err
	}

	info := ListenerInfo{
		ListenerType:     SetType,
		ListenerProtocol: SetProtocol,
		ListenerName:     SetName,
		ListenerUI:       string(listenerUI),
	}

	err = json.NewEncoder(&buffer).Encode(info)
	if err != nil {
		return nil, err
	}

	return buffer.Bytes(), nil
}
