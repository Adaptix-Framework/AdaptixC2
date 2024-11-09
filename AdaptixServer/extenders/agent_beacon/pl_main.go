package main

import (
	"bytes"
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"strconv"
)

const (
	TYPE_AGENT = "agent"

	TYPE_TASK = 1
	TYPE_JOB  = 2
)

type Teamserver interface {
	AgentRequest(agentType string, agentId string, beat []byte, bodyData []byte, listenerName string, ExternalIP string) ([]byte, error)
}

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
	AgentCmd     string
}

type AgentData struct {
	Crc        string   `json:"a_crc"`
	Id         string   `json:"a_id"`
	Name       string   `json:"a_name"`
	SessionKey []byte   `json:"a_session_key"`
	Listener   string   `json:"a_listener"`
	Async      bool     `json:"a_async"`
	ExternalIP string   `json:"a_external_ip"`
	InternalIP string   `json:"a_internal_ip"`
	GmtOffset  int      `json:"a_gmt_offset"`
	Sleep      uint     `json:"a_sleep"`
	Jitter     uint     `json:"a_jitter"`
	Pid        string   `json:"a_pid"`
	Tid        string   `json:"a_tid"`
	Arch       string   `json:"a_arch"`
	Elevated   bool     `json:"a_elevated"`
	Process    string   `json:"a_process"`
	Os         int      `json:"a_os"`
	OsDesc     string   `json:"a_os_desc"`
	Domain     string   `json:"a_domain"`
	Computer   string   `json:"a_computer"`
	Username   string   `json:"a_username"`
	OemCP      int      `json:"a_oemcp"`
	ACP        int      `json:"a_acp"`
	CreateTime int64    `json:"a_create_time"`
	LastTick   int      `json:"a_last_tick"`
	Tags       []string `json:"a_tags"`
}

type TaskData struct {
	TaskType    int    `json:"t_type"`
	TaskId      string `json:"t_task_id"`
	AgentId     string `json:"t_agent_id"`
	TaskData    []byte `json:"t_data"`
	CommandLine string `json:"t_command_line"`
	Sync        bool   `json:"t_sync"`
}

var ModuleObject ModuleExtender
var ModulePath string

////////////////////////////

const (
	SetName     = "beacon"
	SetListener = "BeaconHTTP"
	SetUiPath   = "_ui_agent.json"
	SetCmdPath  = "_cmd_agent.json"
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

////////////////////////////

func (m *ModuleExtender) AgentCreate(beat []byte) ([]byte, error) {
	var (
		buffer bytes.Buffer
		err    error
		agent  AgentData
	)

	packer := CreatePacker(beat)
	agent.Sleep = packer.ParseInt32()
	agent.Jitter = packer.ParseInt32()
	agent.ACP = int(packer.ParseInt16())
	agent.OemCP = int(packer.ParseInt16())
	agent.GmtOffset = int(packer.ParseInt8())
	agent.Pid = fmt.Sprintf("%v", packer.ParseInt16())
	agent.Tid = fmt.Sprintf("%v", packer.ParseInt16())

	buildNumber := packer.ParseInt32()
	majorVersion := packer.ParseInt8()
	minorVersion := packer.ParseInt8()
	internalIp := packer.ParseInt32()
	flag := packer.ParseInt8()

	agent.Arch = "x32"
	if (flag & 0b00000001) > 0 {
		agent.Arch = "x64"
	}

	systemArch := "x32"
	if (flag & 0b00000010) > 0 {
		systemArch = "x64"
	}

	agent.Elevated = false
	if (flag & 0b00000100) > 0 {
		agent.Elevated = true
	}

	IsServer := false
	if (flag & 0b00001000) > 0 {
		IsServer = true
	}

	agent.InternalIP = int32ToIPv4(internalIp)
	agent.Os, agent.OsDesc = GetOsVersion(majorVersion, minorVersion, buildNumber, IsServer, systemArch)

	agent.Async = true
	agent.SessionKey = packer.ParseBytes()
	agent.Domain = string(packer.ParseBytes())
	agent.Computer = string(packer.ParseBytes())
	agent.Username = ConvertCpToUTF8(string(packer.ParseBytes()), agent.ACP)
	agent.Process = ConvertCpToUTF8(string(packer.ParseBytes()), agent.ACP)

	err = json.NewEncoder(&buffer).Encode(agent)
	if err != nil {
		return nil, err
	}

	return buffer.Bytes(), nil
}

func (m *ModuleExtender) AgentProcessData(agentId string, beat []byte) ([]byte, error) {
	return nil, nil
}

func (m *ModuleExtender) AgentPackData(dataAgent []byte, dataTasks [][]byte) ([]byte, error) {
	var (
		agentData  AgentData
		tasksArray []TaskData
		array      []interface{}
		packData   []byte
		err        error
	)
	err = json.Unmarshal(dataAgent, &agentData)
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

	for _, taskData := range tasksArray {
		taskId, err := strconv.ParseInt(taskData.TaskId, 16, 32)
		if err != nil {
			return nil, err
		}
		array = append(array, int(taskId))
		array = append(array, taskData.TaskData)
	}

	packData, err = PackArray(array)
	if err != nil {
		return nil, err
	}

	size := make([]byte, 4)
	binary.LittleEndian.PutUint32(size, uint32(len(packData)))
	packData = append(size, packData...)

	return packData, nil
}

func (m *ModuleExtender) AgentCommand(agentObject []byte, args map[string]any) ([]byte, error) {
	var (
		agent    AgentData
		err      error
		array    []interface{}
		packData []byte
		buffer   bytes.Buffer
		taskType int = TYPE_TASK
	)

	err = json.Unmarshal(agentObject, &agent)
	if err != nil {
		return nil, err
	}

	command, ok := args["command"].(string)
	if !ok {
		return nil, errors.New("'command' must be set")
	}

	// Parse Command

	switch command {

	case "cp":
		src, ok := args["src"].(string)
		if !ok {
			return nil, errors.New("parameter 'src' must be set")
		}
		dst, ok := args["dst"].(string)
		if !ok {
			return nil, errors.New("parameter 'dst' must be set")
		}

		array = []interface{}{12, ConvertUTF8toCp(src, agent.ACP), ConvertUTF8toCp(dst, agent.ACP)}
		break

	default:
		return nil, errors.New(fmt.Sprintf("Command '%v' not found", command))
	}

	// Pack Command

	packData, err = PackArray(array)
	if err != nil {
		return nil, err
	}

	taskInfo := TaskData{
		TaskType: taskType,
		TaskData: packData,
		Sync:     true,
	}

	err = json.NewEncoder(&buffer).Encode(taskInfo)
	if err != nil {
		return nil, err
	}

	return buffer.Bytes(), nil
}
