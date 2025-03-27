package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
)

const (
	LISTENER = "listener"

	INTERNAL = "internal"
	EXTERNAL = "external"
)

type Teamserver interface {
	TsClientDisconnect(username string)

	TsListenerStart(listenerName string, configType string, config string, watermark string, customData []byte) error
	TsListenerEdit(listenerName string, configType string, config string) error
	TsListenerStop(listenerName string, configType string) error
	TsListenerGetProfile(listenerName string, listenerType string) (string, []byte, error)
	TsListenerInteralHandler(watermark string, data []byte) (string, error)

	TsAgentIsExists(agentId string) bool
	TsAgentCreate(agentCrc string, agentId string, beat []byte, listenerName string, ExternalIP string, Async bool) error
	TsAgentProcessData(agentId string, bodyData []byte) error
	TsAgentGetHostedTasks(agentId string, maxDataSize int) ([]byte, error)
	TsAgentCommand(agentName string, agentId string, clientName string, cmdline string, args map[string]any) error
	TsAgentGenerate(agentName string, config string, listenerWM string, listenerProfile []byte) ([]byte, string, error)

	TsAgentUpdateData(newAgentObject []byte) error
	TsAgentImpersonate(agentId string, impersonated string, elevated bool) error
	TsAgentTerminate(agentId string, terminateTaskId string) error
	TsAgentRemove(agentId string) error
	TsAgentSetTag(agentId string, tag string) error
	TsAgentSetMark(agentId string, makr string) error
	TsAgentSetColor(agentId string, background string, foreground string, reset bool) error
	TsAgentTickUpdate()
	TsAgentConsoleOutput(agentId string, messageType int, message string, clearText string, store bool)
	TsAgentConsoleOutputClient(agentId string, client string, messageType int, message string, clearText string)

	TsTaskCreate(agentId string, cmdline string, client string, taskObject []byte)
	TsTaskUpdate(agentId string, cTaskObject []byte)
	TsTaskQueueGetAvailable(agentId string, availableSize int) ([][]byte, error)
	TsTaskStop(agentId string, taskId string) error
	TsTaskDelete(agentId string, taskId string) error

	TsDownloadAdd(agentId string, fileId string, fileName string, fileSize int) error
	TsDownloadUpdate(fileId string, state int, data []byte) error
	TsDownloadClose(fileId string, reason int) error
	TsDownloadSync(fileId string) (string, []byte, error)
	TsDownloadDelete(fileId string) error

	TsDownloadChangeState(fileId string, username string, command string) error
	TsAgentGuiDisks(agentId string, username string) error
	TsAgentGuiProcess(agentId string, username string) error
	TsAgentGuiFiles(agentId string, path string, username string) error
	TsAgentGuiUpload(agentId string, path string, content []byte, username string) error
	TsAgentGuiDownload(agentId string, path string, username string) error

	TsClientGuiDisks(jsonTask string, jsonDrives string)
	TsClientGuiFiles(jsonTask string, path string, jsonFiles string)
	TsClientGuiFilesStatus(jsonTask string)
	TsClientGuiProcess(jsonTask string, jsonFiles string)
	TsAgentGuiExit(agentId string, username string) error

	TsTunnelCreateSocks4(AgentId string, Address string, Port int, FuncMsgConnectTCP func(channelId int, addr string, port int) []byte, FuncMsgWriteTCP func(channelId int, data []byte) []byte, FuncMsgClose func(channelId int) []byte) (string, error)
	TsTunnelCreateSocks5(AgentId string, Address string, Port int, FuncMsgConnectTCP, FuncMsgConnectUDP func(channelId int, addr string, port int) []byte, FuncMsgWriteTCP, FuncMsgWriteUDP func(channelId int, data []byte) []byte, FuncMsgClose func(channelId int) []byte) (string, error)
	TsTunnelCreateSocks5Auth(AgentId string, Address string, Port int, Username string, Password string, FuncMsgConnectTCP, FuncMsgConnectUDP func(channelId int, addr string, port int) []byte, FuncMsgWriteTCP, FuncMsgWriteUDP func(channelId int, data []byte) []byte, FuncMsgClose func(channelId int) []byte) (string, error)
	TsTunnelStopSocks(AgentId string, Port int)
	TsTunnelCreateLocalPortFwd(AgentId string, Address string, Port int, FwdAddress string, FwdPort int, FuncMsgConnect func(channelId int, addr string, port int) []byte, FuncMsgWrite func(channelId int, data []byte) []byte, FuncMsgClose func(channelId int) []byte) (string, error)
	TsTunnelStopLocalPortFwd(AgentId string, Port int)
	TsTunnelConnectionClose(channelId int)
	TsTunnelConnectionResume(AgentId string, channelId int)
	TsTunnelConnectionData(channelId int, data []byte)
}

type ModuleExtender struct {
	ts Teamserver
}

var (
	ModuleObject     ModuleExtender
	ListenerDataPath string
	PluginPath       string
	ListenersObject  []any //*SMB
)

///// Struct

type ExtenderInfo struct {
	ModuleName string
	ModuleType string
}

type ListenerInfo struct {
	Type             string
	ListenerProtocol string
	ListenerName     string
	ListenerUI       string
}

type ListenerData struct {
	Name      string `json:"l_name"`
	Type      string `json:"l_type"`
	BindHost  string `json:"l_bind_host"`
	BindPort  string `json:"l_bind_port"`
	AgentHost string `json:"l_agent_host"`
	AgentPort string `json:"l_agent_port"`
	Status    string `json:"l_status"`
	Data      string `json:"l_data"`
	Watermark string `json:"l_watermark"`
}

/// Module

func (m *ModuleExtender) InitPlugin(ts any) ([]byte, error) {
	var (
		buffer bytes.Buffer
		err    error
	)

	ModuleObject.ts = ts.(Teamserver)

	info := ExtenderInfo{
		ModuleName: SetName,
		ModuleType: LISTENER,
	}

	err = json.NewEncoder(&buffer).Encode(info)
	if err != nil {
		return nil, err
	}

	return buffer.Bytes(), nil
}

func (m *ModuleExtender) ListenerInit(pluginPath string, listenerDataPath string) ([]byte, error) {
	var (
		buffer bytes.Buffer
		err    error
	)

	ListenerDataPath = listenerDataPath
	PluginPath = pluginPath

	uiPath := filepath.Join(PluginPath, SetUiPath)
	listenerUI, err := os.ReadFile(uiPath)
	if err != nil {
		return nil, err
	}

	info := ListenerInfo{
		Type:             SetType,
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

func (m *ModuleExtender) ListenerValid(data string) error {
	return m.HandlerListenerValid(data)
}

func (m *ModuleExtender) ListenerStart(name string, data string, listenerCustomData []byte) ([]byte, []byte, error) {
	var (
		err          error
		listenerData ListenerData
		customData   []byte
		buffer       bytes.Buffer
		listener     any
	)

	listenerData, customData, listener, err = m.HandlerCreateListenerDataAndStart(name, data, listenerCustomData)

	err = json.NewEncoder(&buffer).Encode(listenerData)
	if err != nil {
		return nil, customData, err
	}

	ListenersObject = append(ListenersObject, listener)

	return buffer.Bytes(), customData, nil
}

func (m *ModuleExtender) ListenerEdit(name string, data string) ([]byte, []byte, error) {
	var (
		err          error
		buffer       bytes.Buffer
		listenerData ListenerData
		customData   []byte
		ok           bool
	)

	for _, value := range ListenersObject {

		listenerData, customData, ok = m.HandlerEditListenerData(name, value, data)
		if ok {
			err = json.NewEncoder(&buffer).Encode(listenerData)
			if err != nil {
				return nil, nil, err
			}
			return buffer.Bytes(), customData, nil
		}
	}
	return nil, nil, errors.New("listener not found")
}

func (m *ModuleExtender) ListenerStop(name string) error {
	var (
		index int
		err   error
		ok    bool
	)

	for ind, value := range ListenersObject {
		ok, err = m.HandlerListenerStop(name, value)
		if ok {
			index = ind
			break
		}
	}

	if ok {
		ListenersObject = append(ListenersObject[:index], ListenersObject[index+1:]...)
	} else {
		return errors.New("listener not found")
	}

	return err
}

func (m *ModuleExtender) ListenerGetProfile(name string) ([]byte, error) {
	var (
		profile []byte
		ok      bool
	)

	for _, value := range ListenersObject {

		profile, ok = m.HandlerListenerGetProfile(name, value)
		if ok {
			return profile, nil
		}

	}
	return nil, errors.New("listener not found")
}

func (m *ModuleExtender) ListenerInteralHandler(name string, data []byte) (string, error) {

	for _, value := range ListenersObject {

		id, err, ok := m.HandlerListenerInteralHandler(name, data, value)
		if ok {
			return id, err
		}

	}
	return "", errors.New("listener not found")
}
