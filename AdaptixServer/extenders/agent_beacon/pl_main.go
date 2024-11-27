package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
	"runtime"
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

	MESSAGE_INFO    MessageType = 5
	MESSAGE_ERROR   MessageType = 6
	MESSAGE_SUCCESS MessageType = 7

	DOWNLOAD_STATE_RUNNING  = 0x1
	DOWNLOAD_STATE_STOPPED  = 0x2
	DOWNLOAD_STATE_FINISHED = 0x3
	DOWNLOAD_STATE_CANCELED = 0x4
)

type Teamserver interface {
	TsAgentRequest(agentType string, agentId string, beat []byte, bodyData []byte, listenerName string, ExternalIP string) ([]byte, error)
	TsAgentConsoleOutput(agentId string, messageType int, message string, clearText string)
	TsAgentUpdateData(newAgentObject []byte) error

	TsTaskQueueAddQuite(agentId string, taskObject []byte)
	TsTaskUpdate(agentId string, cTaskObject []byte)
	TsTaskQueueGetAvailable(agentId string, availableSize int) ([][]byte, error)

	TsDownloadAdd(agentId string, fileId string, fileName string, fileSize int) error
	TsDownloadUpdate(fileId string, state int, data []byte) error
	TsDownloadClose(fileId string, reason int) error
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

type ListingFileData struct {
	IsDir    bool   `json:"l_is_dir"`
	Size     uint64 `json:"l_size"`
	Date     uint64 `json:"l_date"`
	Filename string `json:"l_filename"`
}

type ListingProcessData struct {
	Pid         uint   `json:"l_pid"`
	Ppid        uint   `json:"l_ppid"`
	SessionId   uint   `json:"l_session_id"`
	Arch        string `json:"l_arch"`
	Context     string `json:"l_context"`
	ProcessName string `json:"l_process_name"`
}

type ListingDrivesData struct {
	Name string `json:"l_name"`
	Type string `json:"l_type"`
}

var ModuleObject ModuleExtender
var ModulePath string
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
	cmdPath := filepath.Join(dir, SetCmdPath)
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

func (m *ModuleExtender) AgentCommand(agentObject []byte, args map[string]any) ([]byte, string, error) {
	var (
		taskData TaskData
		agent    AgentData
		message  string
		err      error
		buffer   bytes.Buffer
	)

	err = json.Unmarshal(agentObject, &agent)
	if err != nil {
		return nil, "", err
	}

	command, ok := args["command"].(string)
	if !ok {
		return nil, "", errors.New("'command' must be set")
	}

	taskData, message, err = CreateTask(m.ts, agent, command, args)
	if err != nil {
		return nil, "", err
	}

	err = json.NewEncoder(&buffer).Encode(taskData)
	if err != nil {
		return nil, "", err
	}

	return buffer.Bytes(), message, nil
}

func (m *ModuleExtender) AgentPackData(agentObject []byte, dataTasks [][]byte) ([]byte, error) {
	var (
		agentData  AgentData
		tasksArray []TaskData
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

	return PackTasks(agentData, tasksArray)
}

func (m *ModuleExtender) AgentProcessData(agentObject []byte, packedData []byte) ([]byte, error) {
	var (
		agentData AgentData
		taskData  TaskData
		err       error
	)
	err = json.Unmarshal(agentObject, &agentData)
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

	ProcessTasksResult(m.ts, agentData, taskData, packedData)

	return nil, nil
}

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
		Type: JOB,
		Data: packData,
		Sync: false,
	}

	err = json.NewEncoder(&buffer).Encode(taskData)
	if err != nil {
		return nil, err
	}

	return buffer.Bytes(), nil
}
