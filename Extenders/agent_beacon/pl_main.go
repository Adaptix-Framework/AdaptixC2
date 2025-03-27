package main

import (
	"bytes"
	"encoding/hex"
	"encoding/json"
	"errors"
	"math/rand"
	"os"
	"path/filepath"
	"time"
)

type (
	PluginType string
)

const (
	AGENT PluginType = "agent"

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

	TsPivotCreate(pivotId string, pAgentId string, chAgentId string, pivotName string, isRestore bool) error
	TsGetPivotInfoByName(pivotName string) (string, string, string)
	TsGetPivotInfoById(pivotId string) (string, string, string)
	TsPivotDelete(pivotId string) error

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
	TsAgentGuiExit(agentId string, username string) error

	TsClientGuiDisks(jsonTask string, jsonDrives string)
	TsClientGuiFiles(jsonTask string, path string, jsonFiles string)
	TsClientGuiFilesStatus(jsonTask string)
	TsClientGuiProcess(jsonTask string, jsonFiles string)

	TsTunnelCreateSocks4(AgentId string, Address string, Port int, FuncMsgConnectTCP func(channelId int, addr string, port int) []byte, FuncMsgWriteTCP func(channelId int, data []byte) []byte, FuncMsgClose func(channelId int) []byte) (string, error)
	TsTunnelCreateSocks5(AgentId string, Address string, Port int, FuncMsgConnectTCP, FuncMsgConnectUDP func(channelId int, addr string, port int) []byte, FuncMsgWriteTCP, FuncMsgWriteUDP func(channelId int, data []byte) []byte, FuncMsgClose func(channelId int) []byte) (string, error)
	TsTunnelCreateSocks5Auth(AgentId string, Address string, Port int, Username string, Password string, FuncMsgConnectTCP, FuncMsgConnectUDP func(channelId int, addr string, port int) []byte, FuncMsgWriteTCP, FuncMsgWriteUDP func(channelId int, data []byte) []byte, FuncMsgClose func(channelId int) []byte) (string, error)
	TsTunnelStopSocks(AgentId string, Port int)
	TsTunnelCreateLocalPortFwd(AgentId string, Address string, Port int, FwdAddress string, FwdPort int, FuncMsgConnect func(channelId int, addr string, port int) []byte, FuncMsgWrite func(channelId int, data []byte) []byte, FuncMsgClose func(channelId int) []byte) (string, error)
	TsTunnelStopLocalPortFwd(AgentId string, Port int)
	TsTunnelCreateRemotePortFwd(AgentId string, Port int, FwdAddress string, FwdPort int, FuncMsgReverse func(tunnelId int, port int) []byte, FuncMsgWrite func(channelId int, data []byte) []byte, FuncMsgClose func(channelId int) []byte) (string, error)
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
	ModuleObject ModuleExtender
	PluginPath   string
)

///// Struct

type ExtenderInfo struct {
	ModuleName string
	ModuleType PluginType
}

type AgentInfo struct {
	AgentName    string
	ListenerName []string
	AgentUI      string
	AgentCmd     string
}

type AgentData struct {
	Crc          string `json:"a_crc"`
	Id           string `json:"a_id"`
	Name         string `json:"a_name"`
	SessionKey   []byte `json:"a_session_key"`
	Listener     string `json:"a_listener"`
	Async        bool   `json:"a_async"`
	ExternalIP   string `json:"a_external_ip"`
	InternalIP   string `json:"a_internal_ip"`
	GmtOffset    int    `json:"a_gmt_offset"`
	Sleep        uint   `json:"a_sleep"`
	Jitter       uint   `json:"a_jitter"`
	Pid          string `json:"a_pid"`
	Tid          string `json:"a_tid"`
	Arch         string `json:"a_arch"`
	Elevated     bool   `json:"a_elevated"`
	Process      string `json:"a_process"`
	Os           int    `json:"a_os"`
	OsDesc       string `json:"a_os_desc"`
	Domain       string `json:"a_domain"`
	Computer     string `json:"a_computer"`
	Username     string `json:"a_username"`
	Impersonated string `json:"a_impersonated"`
	OemCP        int    `json:"a_oemcp"`
	ACP          int    `json:"a_acp"`
	CreateTime   int64  `json:"a_create_time"`
	LastTick     int    `json:"a_last_tick"`
	Tags         string `json:"a_tags"`
	Mark         string `json:"a_mark"`
	Color        string `json:"a_color"`
}

type TaskData struct {
	Type        int    `json:"t_type"`
	TaskId      string `json:"t_task_id"`
	AgentId     string `json:"t_agent_id"`
	Client      string `json:"t_client"`
	User        string `json:"t_user"`
	Computer    string `json:"t_computer"`
	StartDate   int64  `json:"t_start_date"`
	FinishDate  int64  `json:"t_finish_date"`
	Data        []byte `json:"t_data"`
	CommandLine string `json:"t_command_line"`
	MessageType int    `json:"t_message_type"`
	Message     string `json:"t_message"`
	ClearText   string `json:"t_clear_text"`
	Completed   bool   `json:"t_completed"`
	Sync        bool   `json:"t_sync"`
}

type ConsoleMessageData struct {
	Message string `json:"m_message"`
	Status  int    `json:"m_status"`
	Text    string `json:"m_text"`
}

type ListingFileData struct {
	IsDir    bool   `json:"b_is_dir"`
	Size     int64  `json:"b_size"`
	Date     int64  `json:"b_date"`
	Filename string `json:"b_filename"`
}

type ListingProcessData struct {
	Pid         uint   `json:"b_pid"`
	Ppid        uint   `json:"b_ppid"`
	SessionId   uint   `json:"b_session_id"`
	Arch        string `json:"b_arch"`
	Context     string `json:"b_context"`
	ProcessName string `json:"b_process_name"`
}

type ListingDrivesData struct {
	Name string `json:"b_name"`
	Type string `json:"b_type"`
}

///// Module

func (m *ModuleExtender) InitPlugin(ts any) ([]byte, error) {
	var (
		buffer bytes.Buffer
		err    error
	)

	ModuleObject.ts = ts.(Teamserver)

	info := ExtenderInfo{
		ModuleName: SetName,
		ModuleType: AGENT,
	}

	err = json.NewEncoder(&buffer).Encode(info)
	if err != nil {
		return nil, err
	}

	return buffer.Bytes(), nil
}

func (m *ModuleExtender) AgentInit(pluginPath string) ([]byte, error) {
	var (
		buffer bytes.Buffer
		err    error
	)

	PluginPath = pluginPath

	uiPath := filepath.Join(PluginPath, SetUiPath)
	agentUI, err := os.ReadFile(uiPath)
	if err != nil {
		return nil, err
	}
	cmdPath := filepath.Join(PluginPath, SetCmdPath)
	agentCmd, err := os.ReadFile(cmdPath)
	if err != nil {
		return nil, err
	}

	info := AgentInfo{
		AgentName:    SetName,
		ListenerName: SetListeners,
		AgentUI:      string(agentUI),
		AgentCmd:     string(agentCmd),
	}

	err = json.NewEncoder(&buffer).Encode(info)
	if err != nil {
		return nil, err
	}

	return buffer.Bytes(), nil
}

func (m *ModuleExtender) AgentGenerate(config string, listenerWM string, listenerProfile []byte) ([]byte, string, error) {
	var (
		listenerMap  map[string]any
		agentProfile []byte
		err          error
	)

	err = json.Unmarshal(listenerProfile, &listenerMap)
	if err != nil {
		return nil, "", err
	}

	agentProfile, err = AgentGenerateProfile(config, listenerWM, listenerMap)
	if err != nil {
		return nil, "", err
	}

	return AgentGenerateBuild(config, agentProfile, listenerMap)
}

func (m *ModuleExtender) AgentCreate(beat []byte) ([]byte, error) {
	var (
		buffer bytes.Buffer
		err    error
		agent  AgentData
	)

	agent, err = CreateAgent(beat)
	if err != nil {
		return nil, err
	}

	err = json.NewEncoder(&buffer).Encode(agent)
	if err != nil {
		return nil, err
	}

	return buffer.Bytes(), nil
}

func (m *ModuleExtender) AgentCommand(client string, cmdline string, agentObject []byte, args map[string]any) error {
	var (
		taskData    TaskData
		agentData   AgentData
		messageData ConsoleMessageData
		command     string
		err         error
		ok          bool
		bufferTask  bytes.Buffer
	)

	err = json.Unmarshal(agentObject, &agentData)
	if err != nil {
		goto ERR
	}

	command, ok = args["command"].(string)
	if !ok {
		err = errors.New("'command' must be set")
		goto ERR
	}

	taskData, messageData, err = CreateTask(m.ts, agentData, command, args)
	if err != nil {
		goto ERR
	}

	err = json.NewEncoder(&bufferTask).Encode(taskData)
	if err != nil {
		goto ERR
	}

	m.ts.TsTaskCreate(agentData.Id, cmdline, client, bufferTask.Bytes())

	if len(messageData.Message) > 0 || len(messageData.Text) > 0 {
		m.ts.TsAgentConsoleOutput(agentData.Id, messageData.Status, messageData.Message, messageData.Text, false)
	}

ERR:
	return err
}

func (m *ModuleExtender) AgentPackData(agentObject []byte, maxDataSize int) ([]byte, error) {
	var (
		agentData  AgentData
		tasksArray []TaskData
		packedData []byte
		dataTasks  [][]byte
		err        error
	)
	err = json.Unmarshal(agentObject, &agentData)
	if err != nil {
		return nil, err
	}

	dataTasks, err = m.ts.TsTaskQueueGetAvailable(agentData.Id, maxDataSize)
	if err != nil {
		return nil, err
	}

	for _, value := range dataTasks {
		var taskData TaskData
		err = json.Unmarshal(value, &taskData)
		if err != nil {
			return nil, err
		}
		tasksArray = append(tasksArray, taskData)
	}

	packedData, err = PackTasks(agentData, tasksArray)
	if err != nil {
		return nil, err
	}

	return RC4Crypt(packedData, agentData.SessionKey)
}

func (m *ModuleExtender) AgentPivotPackData(pivotId string, data []byte) ([]byte, error) {

	packData, err := PackPivotTasks(pivotId, data)
	if err != nil {
		return nil, err
	}

	randomBytes := make([]byte, 16)
	rand.Read(randomBytes)
	uid := hex.EncodeToString(randomBytes)[:8]

	taskData := TaskData{
		TaskId: uid,
		Type:   TYPE_PROXY_DATA,
		Data:   packData,
		Sync:   false,
	}

	var taskObject bytes.Buffer
	_ = json.NewEncoder(&taskObject).Encode(taskData)

	return taskObject.Bytes(), nil
}

func (m *ModuleExtender) AgentProcessData(agentObject []byte, packedData []byte) ([]byte, error) {
	var (
		agentData   AgentData
		taskData    TaskData
		decryptData []byte
		err         error
	)
	err = json.Unmarshal(agentObject, &agentData)
	if err != nil {
		return nil, err
	}

	decryptData, err = RC4Crypt(packedData, agentData.SessionKey)
	if err != nil {
		return nil, err
	}

	taskData = TaskData{
		Type:        TYPE_TASK,
		AgentId:     agentData.Id,
		FinishDate:  time.Now().Unix(),
		MessageType: MESSAGE_SUCCESS,
		Completed:   true,
		Sync:        true,
	}

	resultTasks := ProcessTasksResult(m.ts, agentData, taskData, decryptData)

	for _, task := range resultTasks {
		var taskObject bytes.Buffer
		_ = json.NewEncoder(&taskObject).Encode(task)
		m.ts.TsTaskUpdate(agentData.Id, taskObject.Bytes())
	}

	return nil, nil
}

/// BROWSERS

func (m *ModuleExtender) AgentDownloadChangeState(agentObject []byte, newState int, fileId string) ([]byte, error) {
	var (
		packData  []byte
		agentData AgentData
		taskData  TaskData
		buffer    bytes.Buffer
		err       error
	)
	err = json.Unmarshal(agentObject, &agentData)
	if err != nil {
		return nil, err
	}

	packData, err = BrowserDownloadChangeState(fileId, newState)
	if err != nil {
		return nil, err
	}

	taskData = TaskData{
		Type: TYPE_BROWSER,
		Data: packData,
		Sync: false,
	}

	err = json.NewEncoder(&buffer).Encode(taskData)
	if err != nil {
		return nil, err
	}

	return buffer.Bytes(), nil
}

func (m *ModuleExtender) AgentBrowserDisks(agentObject []byte) ([]byte, error) {
	var (
		packData  []byte
		agentData AgentData
		taskData  TaskData
		buffer    bytes.Buffer
		err       error
	)
	err = json.Unmarshal(agentObject, &agentData)
	if err != nil {
		return nil, err
	}

	packData, err = BrowserDisks(agentData)
	if err != nil {
		return nil, err
	}

	taskData = TaskData{
		Type: TYPE_BROWSER,
		Data: packData,
		Sync: false,
	}

	err = json.NewEncoder(&buffer).Encode(taskData)
	if err != nil {
		return nil, err
	}

	return buffer.Bytes(), nil
}

func (m *ModuleExtender) AgentBrowserProcess(agentObject []byte) ([]byte, error) {
	var (
		packData  []byte
		agentData AgentData
		taskData  TaskData
		buffer    bytes.Buffer
		err       error
	)
	err = json.Unmarshal(agentObject, &agentData)
	if err != nil {
		return nil, err
	}

	packData, err = BrowserProcess(agentData)
	if err != nil {
		return nil, err
	}

	taskData = TaskData{
		Type: TYPE_BROWSER,
		Data: packData,
		Sync: false,
	}

	err = json.NewEncoder(&buffer).Encode(taskData)
	if err != nil {
		return nil, err
	}

	return buffer.Bytes(), nil
}

func (m *ModuleExtender) AgentBrowserFiles(path string, agentObject []byte) ([]byte, error) {
	var (
		packData  []byte
		agentData AgentData
		taskData  TaskData
		buffer    bytes.Buffer
		err       error
	)
	err = json.Unmarshal(agentObject, &agentData)
	if err != nil {
		return nil, err
	}

	packData, err = BrowserFiles(path, agentData)
	if err != nil {
		return nil, err
	}

	taskData = TaskData{
		Type: TYPE_BROWSER,
		Data: packData,
		Sync: false,
	}

	err = json.NewEncoder(&buffer).Encode(taskData)
	if err != nil {
		return nil, err
	}

	return buffer.Bytes(), nil
}

func (m *ModuleExtender) AgentBrowserUpload(path string, content []byte, agentObject []byte) ([]byte, error) {
	var (
		packData  []byte
		agentData AgentData
		taskData  TaskData
		buffer    bytes.Buffer
		err       error
	)
	err = json.Unmarshal(agentObject, &agentData)
	if err != nil {
		return nil, err
	}

	packData, err = BrowserUpload(m.ts, path, content, agentData)
	if err != nil {
		return nil, err
	}

	taskData = TaskData{
		Type: TYPE_BROWSER,
		Data: packData,
		Sync: false,
	}

	err = json.NewEncoder(&buffer).Encode(taskData)
	if err != nil {
		return nil, err
	}

	return buffer.Bytes(), nil
}

func (m *ModuleExtender) AgentBrowserDownload(path string, agentObject []byte) ([]byte, error) {
	var (
		packData  []byte
		agentData AgentData
		taskData  TaskData
		buffer    bytes.Buffer
		err       error
	)
	err = json.Unmarshal(agentObject, &agentData)
	if err != nil {
		return nil, err
	}

	packData, err = BrowserDownload(path, agentData)
	if err != nil {
		return nil, err
	}

	taskData = TaskData{
		Type: TYPE_TASK,
		Data: packData,
		Sync: true,
	}

	err = json.NewEncoder(&buffer).Encode(taskData)
	if err != nil {
		return nil, err
	}

	return buffer.Bytes(), nil
}

func (m *ModuleExtender) AgentBrowserJobKill(jobId string) ([]byte, error) {
	var (
		packData []byte
		taskData TaskData
		buffer   bytes.Buffer
		err      error
	)

	packData, err = BrowserJobKill(jobId)
	if err != nil {
		return nil, err
	}

	taskData = TaskData{
		Type: TYPE_TASK,
		Data: packData,
		Sync: false,
	}

	err = json.NewEncoder(&buffer).Encode(taskData)
	if err != nil {
		return nil, err
	}

	return buffer.Bytes(), nil
}

func (m *ModuleExtender) AgentBrowserExit(agentObject []byte) ([]byte, error) {
	var (
		packData  []byte
		agentData AgentData
		taskData  TaskData
		buffer    bytes.Buffer
		err       error
	)
	err = json.Unmarshal(agentObject, &agentData)
	if err != nil {
		return nil, err
	}

	packData, err = BrowserExit(agentData)
	if err != nil {
		return nil, err
	}

	taskData = TaskData{
		Type: TYPE_TASK,
		Data: packData,
		Sync: true,
	}

	err = json.NewEncoder(&buffer).Encode(taskData)
	if err != nil {
		return nil, err
	}

	return buffer.Bytes(), nil
}

/// SYNC

func SyncBrowserDisks(ts Teamserver, task TaskData, drivesSlice []ListingDrivesData) {
	var (
		jsonDrives string
		jsonTask   string
		jsonData   []byte
		err        error
	)

	jsonData, err = json.Marshal(drivesSlice)
	if err != nil {
		return
	}
	jsonDrives = string(jsonData)

	jsonData, err = json.Marshal(task)
	if err != nil {
		return
	}
	jsonTask = string(jsonData)

	ts.TsClientGuiDisks(jsonTask, jsonDrives)
}

func SyncBrowserFiles(ts Teamserver, task TaskData, path string, filesSlice []ListingFileData) {
	var (
		jsonDrives string
		jsonTask   string
		jsonData   []byte
		err        error
	)

	jsonData, err = json.Marshal(filesSlice)
	if err != nil {
		return
	}
	jsonDrives = string(jsonData)

	jsonData, err = json.Marshal(task)
	if err != nil {
		return
	}
	jsonTask = string(jsonData)

	ts.TsClientGuiFiles(jsonTask, path, jsonDrives)
}

func SyncBrowserFilesStatus(ts Teamserver, task TaskData) {
	var (
		jsonTask string
		jsonData []byte
		err      error
	)

	jsonData, err = json.Marshal(task)
	if err != nil {
		return
	}
	jsonTask = string(jsonData)

	ts.TsClientGuiFilesStatus(jsonTask)
}

func SyncBrowserProcess(ts Teamserver, task TaskData, processlist []ListingProcessData) {
	var (
		jsonProcess string
		jsonTask    string
		jsonData    []byte
		err         error
	)

	jsonData, err = json.Marshal(processlist)
	if err != nil {
		return
	}
	jsonProcess = string(jsonData)

	jsonData, err = json.Marshal(task)
	if err != nil {
		return
	}
	jsonTask = string(jsonData)

	ts.TsClientGuiProcess(jsonTask, jsonProcess)
}

/// TYPE_TUNNEL

func TunnelMessageConnectTCP(channelId int, address string, port int) []byte {
	var (
		packData []byte
		taskData TaskData
		buffer   bytes.Buffer
	)

	packData, _ = TunnelCreateTCP(channelId, address, port)

	taskData = TaskData{
		Type: TYPE_PROXY_DATA,
		Data: packData,
		Sync: false,
	}

	_ = json.NewEncoder(&buffer).Encode(taskData)
	return buffer.Bytes()
}

func TunnelMessageConnectUDP(channelId int, address string, port int) []byte {
	var (
		packData []byte
		taskData TaskData
		buffer   bytes.Buffer
	)

	packData, _ = TunnelCreateUDP(channelId, address, port)

	taskData = TaskData{
		Type: TYPE_PROXY_DATA,
		Data: packData,
		Sync: false,
	}

	_ = json.NewEncoder(&buffer).Encode(taskData)
	return buffer.Bytes()
}

func TunnelMessageWriteTCP(channelId int, data []byte) []byte {
	var (
		packData []byte
		taskData TaskData
		buffer   bytes.Buffer
	)

	packData, _ = TunnelWriteTCP(channelId, data)

	taskData = TaskData{
		Type: TYPE_PROXY_DATA,
		Data: packData,
		Sync: false,
	}

	_ = json.NewEncoder(&buffer).Encode(taskData)
	return buffer.Bytes()
}

func TunnelMessageWriteUDP(channelId int, data []byte) []byte {
	var (
		packData []byte
		taskData TaskData
		buffer   bytes.Buffer
	)

	packData, _ = TunnelWriteUDP(channelId, data)

	taskData = TaskData{
		Type: TYPE_PROXY_DATA,
		Data: packData,
		Sync: false,
	}

	_ = json.NewEncoder(&buffer).Encode(taskData)
	return buffer.Bytes()
}

func TunnelMessageClose(channelId int) []byte {
	var (
		packData []byte
		taskData TaskData
		buffer   bytes.Buffer
	)

	packData, _ = TunnelClose(channelId)

	taskData = TaskData{
		Type: TYPE_PROXY_DATA,
		Data: packData,
		Sync: false,
	}

	_ = json.NewEncoder(&buffer).Encode(taskData)
	return buffer.Bytes()
}

func TunnelMessageReverse(tunnelId int, port int) []byte {
	var (
		packData []byte
		taskData TaskData
		buffer   bytes.Buffer
	)

	packData, _ = TunnelReverse(tunnelId, port)

	taskData = TaskData{
		Type: TYPE_PROXY_DATA,
		Data: packData,
		Sync: false,
	}

	_ = json.NewEncoder(&buffer).Encode(taskData)
	return buffer.Bytes()
}
