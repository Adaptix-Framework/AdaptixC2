package connector

import (
	"AdaptixServer/core/profile"
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/token"
	"errors"
	"fmt"
	"github.com/Adaptix-Framework/axc2"
	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
	"os"
)

type Teamserver interface {
	TsClientExists(username string) bool
	TsClientDisconnect(username string)
	TsClientSync(username string)
	TsClientConnect(username string, socket *websocket.Conn)

	TsListenerStart(listenerName string, configType string, config string, watermark string, customData []byte) error
	TsListenerEdit(listenerName string, configType string, config string) error
	TsListenerStop(listenerName string, configType string) error
	TsListenerGetProfile(listenerName string, listenerType string) (string, []byte, error)
	TsListenerInteralHandler(watermark string, data []byte) (string, error)

	TsAgentIsExists(agentId string) bool
	TsAgentCreate(agentCrc string, agentId string, beat []byte, listenerName string, ExternalIP string, Async bool) error
	TsAgentProcessData(agentId string, bodyData []byte) error
	TsAgentGetHostedTasksAll(agentId string, maxDataSize int) ([]byte, error)
	TsAgentCommand(agentName string, agentId string, clientName string, cmdline string, args map[string]any) error
	TsAgentGenerate(agentName string, config string, operatingSystem string, listenerWM string, listenerProfile []byte) ([]byte, string, error)

	TsAgentUpdateData(newAgentData adaptix.AgentData) error
	TsAgentImpersonate(agentId string, impersonated string, elevated bool) error
	TsAgentTerminate(agentId string, terminateTaskId string) error
	TsAgentRemove(agentId string) error
	TsAgentSetTag(agentId string, tag string) error
	TsAgentSetMark(agentId string, makr string) error
	TsAgentSetColor(agentId string, background string, foreground string, reset bool) error
	TsAgentTickUpdate()
	TsAgentConsoleOutput(agentId string, messageType int, message string, clearText string, store bool)
	TsAgentConsoleOutputClient(agentId string, client string, messageType int, message string, clearText string)

	TsTaskCreate(agentId string, cmdline string, client string, taskData adaptix.TaskData)
	TsTaskUpdate(agentId string, data adaptix.TaskData)
	TsTaskGetAvailableAll(agentId string, availableSize int) ([]adaptix.TaskData, error)
	TsTaskStop(agentId string, taskId string) error
	TsTaskDelete(agentId string, taskId string) error

	TsDownloadAdd(agentId string, fileId string, fileName string, fileSize int) error
	TsDownloadUpdate(fileId string, state int, data []byte) error
	TsDownloadClose(fileId string, reason int) error
	///
	TsDownloadSync(fileId string) (string, []byte, error)
	TsDownloadDelete(fileId string) error
	///
	TsDownloadTaskStart(agentId string, path string, username string) error
	TsDownloadTaskCancel(fileId string, clientName string) error
	TsDownloadTaskResume(fileId string, clientName string) error
	TsDownloadTaskPause(fileId string, clientName string) error

	TsScreenshotDelete(screenId string) error
	TsScreenshotNote(screenId string, note string) error

	TsAgentGuiDisks(agentId string, username string) error
	TsAgentGuiProcess(agentId string, username string) error
	TsAgentGuiFiles(agentId string, path string, username string) error
	TsAgentGuiUpload(agentId string, path string, content []byte, username string) error

	TsAgentGuiExit(agentId string, username string) error

	TsClientGuiDisks(taskData adaptix.TaskData, jsonDrives string)
	TsClientGuiFiles(taskData adaptix.TaskData, path string, jsonFiles string)
	TsClientGuiFilesStatus(taskData adaptix.TaskData)
	TsClientGuiProcess(taskData adaptix.TaskData, jsonFiles string)

	TsAgentTerminalCreateChannel(terminalData string, wsconn *websocket.Conn) error

	TsTunnelClientStart(AgentId string, Listen bool, Type int, Info string, Lhost string, Lport int, Client string, Thost string, Tport int, AuthUser string, AuthPass string) (string, error)
	TsTunnelClientNewChannel(TunnelData string, wsconn *websocket.Conn) error
	TsTunnelClientStop(TunnelId string, Client string) error
	TsTunnelStop(TunnelId string) error
	TsTunnelClientSetInfo(TunnelId string, Info string) error
	TsTunnelCreateSocks4(AgentId string, Info string, Lhost string, Lport int) (string, error)
	TsTunnelCreateSocks5(AgentId string, Info string, Lhost string, Lport int, UseAuth bool, Username string, Password string) (string, error)
	TsTunnelCreateLportfwd(AgentId string, Info string, Lhost string, Lport int, Thost string, Tport int) (string, error)
	TsTunnelStopSocks(AgentId string, Port int)
	TsTunnelStopLportfwd(AgentId string, Port int)
	TsTunnelStopRportfwd(AgentId string, Port int)
	TsTunnelConnectionClose(channelId int)
	TsTunnelConnectionResume(AgentId string, channelId int, ioDirect bool)
	TsTunnelConnectionData(channelId int, data []byte)
}

type TsConnector struct {
	Port     int
	Hash     string
	Endpoint string
	Cert     string
	Key      string

	Engine     *gin.Engine
	teamserver Teamserver
}

func default404Middleware(tsResponse profile.TsResponse) gin.HandlerFunc {
	return func(c *gin.Context) {
		if len(c.Errors) > 0 && !c.Writer.Written() {
			for header, value := range tsResponse.Headers {
				c.Header(header, value)
			}
			c.String(tsResponse.Status, tsResponse.PageContent)
			c.Abort()
			return
		}

		c.Next()

		if len(c.Errors) > 0 && !c.Writer.Written() {
			for header, value := range tsResponse.Headers {
				c.Header(header, value)
			}
			c.String(tsResponse.Status, tsResponse.PageContent)
		}
	}
}

func NewTsConnector(ts Teamserver, tsProfile profile.TsProfile, tsResponse profile.TsResponse) (*TsConnector, error) {
	gin.SetMode(gin.ReleaseMode)

	if tsResponse.PagePath != "" {
		fileContent, _ := os.ReadFile(tsResponse.PagePath)
		tsResponse.PageContent = string(fileContent)
	}

	var connector = new(TsConnector)
	connector.Engine = gin.New()
	connector.teamserver = ts
	connector.Port = tsProfile.Port
	connector.Endpoint = tsProfile.Endpoint
	connector.Hash = krypt.SHA256([]byte(tsProfile.Password))
	connector.Key = tsProfile.Key
	connector.Cert = tsProfile.Cert

	connector.Engine.POST(tsProfile.Endpoint+"/login", default404Middleware(tsResponse), connector.tcLogin)
	connector.Engine.POST(tsProfile.Endpoint+"/refresh", default404Middleware(tsResponse), token.RefreshTokenHandler)
	connector.Engine.POST(tsProfile.Endpoint+"/sync", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.tcSync)

	connector.Engine.GET(tsProfile.Endpoint+"/connect", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.tcConnect)
	connector.Engine.GET(tsProfile.Endpoint+"/channel", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.tcChannel)

	connector.Engine.POST(tsProfile.Endpoint+"/listener/create", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcListenerStart)
	connector.Engine.POST(tsProfile.Endpoint+"/listener/edit", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcListenerEdit)
	connector.Engine.POST(tsProfile.Endpoint+"/listener/stop", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcListenerStop)

	connector.Engine.POST(tsProfile.Endpoint+"/agent/generate", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcAgentGenerate)
	connector.Engine.POST(tsProfile.Endpoint+"/agent/command", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcAgentCommand)
	connector.Engine.POST(tsProfile.Endpoint+"/agent/remove", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcAgentRemove)
	connector.Engine.POST(tsProfile.Endpoint+"/agent/exit", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcAgentExit)
	connector.Engine.POST(tsProfile.Endpoint+"/agent/settag", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcAgentSetTag)
	connector.Engine.POST(tsProfile.Endpoint+"/agent/setmark", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcAgentSetMark)
	connector.Engine.POST(tsProfile.Endpoint+"/agent/setcolor", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcAgentSetColor)

	connector.Engine.POST(tsProfile.Endpoint+"/agent/task/stop", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcAgentTaskStop)
	connector.Engine.POST(tsProfile.Endpoint+"/agent/task/delete", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcAgentTaskDelete)

	connector.Engine.POST(tsProfile.Endpoint+"/download/sync", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcGuiDownloadSync)
	connector.Engine.POST(tsProfile.Endpoint+"/download/delete", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcGuiDownloadDelete)
	connector.Engine.POST(tsProfile.Endpoint+"/download/start", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcGuiDownloadStart)
	connector.Engine.POST(tsProfile.Endpoint+"/download/cancel", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcGuiDownloadCancel)
	connector.Engine.POST(tsProfile.Endpoint+"/download/resume", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcGuiDownloadResume)
	connector.Engine.POST(tsProfile.Endpoint+"/download/pause", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcGuiDownloadPause)

	connector.Engine.POST(tsProfile.Endpoint+"/browser/disks", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcGuiDisks)
	connector.Engine.POST(tsProfile.Endpoint+"/browser/files", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcGuiFiles)
	connector.Engine.POST(tsProfile.Endpoint+"/browser/upload", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcGuiUpload)
	connector.Engine.POST(tsProfile.Endpoint+"/browser/process", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcGuiProcess)

	connector.Engine.POST(tsProfile.Endpoint+"/screen/setnote", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcScreenshotSetNote)
	connector.Engine.POST(tsProfile.Endpoint+"/screen/remove", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcScreenshotRemove)

	connector.Engine.POST(tsProfile.Endpoint+"/tunnel/start/socks5", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcTunnelStartSocks5)
	connector.Engine.POST(tsProfile.Endpoint+"/tunnel/start/socks4", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcTunnelStartSocks4)
	connector.Engine.POST(tsProfile.Endpoint+"/tunnel/start/lportfwd", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcTunnelStartLpf)
	connector.Engine.POST(tsProfile.Endpoint+"/tunnel/start/rportfwd", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcTunnelStartRpf)
	connector.Engine.POST(tsProfile.Endpoint+"/tunnel/stop", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcTunnelStop)
	connector.Engine.POST(tsProfile.Endpoint+"/tunnel/setinfo", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcTunnelSetIno)

	connector.Engine.NoRoute(default404Middleware(tsResponse), func(c *gin.Context) { _ = c.Error(errors.New("NoRoute")) })

	return connector, nil
}

func (tc *TsConnector) Start(finished *chan bool) {
	host := fmt.Sprintf(":%d", tc.Port)
	err := tc.Engine.RunTLS(host, tc.Cert, tc.Key)
	if err != nil {
		logs.Error("", "Failed to start HTTP Server: "+err.Error())
		return
	}
	*finished <- true
}
