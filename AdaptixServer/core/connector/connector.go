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
	CreateOTP(otpType string, id string) (string, error)
	ValidateOTP() gin.HandlerFunc

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
	TsAgentCreate(agentCrc string, agentId string, beat []byte, listenerName string, ExternalIP string, Async bool) (adaptix.AgentData, error)
	TsAgentProcessData(agentId string, bodyData []byte) error
	TsAgentGetHostedAll(agentId string, maxDataSize int) ([]byte, error)
	TsAgentCommand(agentName string, agentId string, clientName string, hookId string, cmdline string, ui bool, args map[string]any) error
	TsAgentGenerate(agentName string, config string, listenerWM string, listenerProfile []byte) ([]byte, string, error)

	TsAgentUpdateData(newAgentData adaptix.AgentData) error
	TsAgentTerminate(agentId string, terminateTaskId string) error
	TsAgentRemove(agentId string) error
	TsAgentConsoleRemove(agentId string) error
	TsAgentSetTag(agentId string, tag string) error
	TsAgentSetMark(agentId string, makr string) error
	TsAgentSetColor(agentId string, background string, foreground string, reset bool) error
	TsAgentSetImpersonate(agentId string, impersonated string, elevated bool) error
	TsAgentTickUpdate()
	TsAgentConsoleOutput(agentId string, messageType int, message string, clearText string, store bool)
	TsAgentConsoleOutputClient(agentId string, client string, messageType int, message string, clearText string)

	TsTaskCreate(agentId string, cmdline string, client string, taskData adaptix.TaskData)
	TsTaskUpdate(agentId string, data adaptix.TaskData)
	TsTaskGetAvailableAll(agentId string, availableSize int) ([]adaptix.TaskData, error)
	TsTaskCancel(agentId string, taskId string) error
	TsTaskDelete(agentId string, taskId string) error
	TsTaskPostHook(hookData adaptix.TaskData, jobIndex int) error

	TsDownloadAdd(agentId string, fileId string, fileName string, fileSize int) error
	TsDownloadUpdate(fileId string, state int, data []byte) error
	TsDownloadClose(fileId string, reason int) error

	TsDownloadSync(fileId string) (string, []byte, error)
	TsDownloadDelete(fileId string) error
	TsDownloadGetFilepath(fileId string) (string, error)
	TsUploadGetFilepath(fileId string) (string, error)
	TsUploadGetFileContent(fileId string) ([]byte, error)

	TsScreenshotDelete(screenId string) error
	TsScreenshotNote(screenId string, note string) error

	TsCredentilsAdd(creds []map[string]interface{}) error
	TsCredentilsEdit(credId string, username string, password string, realm string, credType string, tag string, storage string, host string) error
	TsCredentilsDelete(credId string) error
	TsCredentialsSetTag(credsId []string, tag string) error

	TsTargetsAdd(targets []map[string]interface{}) error
	TsTargetsEdit(targetId string, computer string, domain string, address string, os int, osDesk string, tag string, info string, alive bool) error
	TsTargetDelete(targetId string) error
	TsTargetSetTag(targetsId []string, tag string) error

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
	Interface string
	Port      int
	Hash      string
	Endpoint  string
	Cert      string
	Key       string

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
	connector.Interface = tsProfile.Interface
	connector.Port = tsProfile.Port
	connector.Endpoint = tsProfile.Endpoint
	connector.Hash = krypt.SHA256([]byte(tsProfile.Password))
	connector.Key = tsProfile.Key
	connector.Cert = tsProfile.Cert

	connector.Engine.POST(tsProfile.Endpoint+"/login", default404Middleware(tsResponse), connector.tcLogin)
	connector.Engine.POST(tsProfile.Endpoint+"/refresh", default404Middleware(tsResponse), token.RefreshTokenHandler)
	connector.Engine.POST(tsProfile.Endpoint+"/sync", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.tcSync)

	connector.Engine.POST(tsProfile.Endpoint+"/otp/generate", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.tcOTP_Generate)
	connector.Engine.POST(tsProfile.Endpoint+"/otp/upload/temp", ts.ValidateOTP(), default404Middleware(tsResponse), connector.tcOTP_UploadTemp)
	connector.Engine.GET(tsProfile.Endpoint+"/otp/download/sync", ts.ValidateOTP(), default404Middleware(tsResponse), connector.tcOTP_DownloadSync)

	connector.Engine.GET(tsProfile.Endpoint+"/connect", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.tcConnect)
	connector.Engine.GET(tsProfile.Endpoint+"/channel", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.tcChannel)

	connector.Engine.POST(tsProfile.Endpoint+"/listener/create", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcListenerStart)
	connector.Engine.POST(tsProfile.Endpoint+"/listener/edit", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcListenerEdit)
	connector.Engine.POST(tsProfile.Endpoint+"/listener/stop", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcListenerStop)

	connector.Engine.POST(tsProfile.Endpoint+"/agent/generate", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcAgentGenerate)
	connector.Engine.POST(tsProfile.Endpoint+"/agent/remove", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcAgentRemove)

	connector.Engine.POST(tsProfile.Endpoint+"/agent/command/file", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcAgentCommandFile)
	connector.Engine.POST(tsProfile.Endpoint+"/agent/command/execute", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcAgentCommandExecute)
	connector.Engine.POST(tsProfile.Endpoint+"/agent/console/remove", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcAgentConsoleRemove)
	connector.Engine.POST(tsProfile.Endpoint+"/agent/set/tag", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcAgentSetTag)
	connector.Engine.POST(tsProfile.Endpoint+"/agent/set/mark", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcAgentSetMark)
	connector.Engine.POST(tsProfile.Endpoint+"/agent/set/color", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcAgentSetColor)
	connector.Engine.POST(tsProfile.Endpoint+"/agent/set/impersonate", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcAgentSetImpersonate)

	connector.Engine.POST(tsProfile.Endpoint+"/agent/task/cancel", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcAgentTaskCancel)
	connector.Engine.POST(tsProfile.Endpoint+"/agent/task/delete", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcAgentTaskDelete)
	connector.Engine.POST(tsProfile.Endpoint+"/agent/task/hook", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcAgentTaskHook)

	connector.Engine.POST(tsProfile.Endpoint+"/download/sync", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcGuiDownloadSync)
	connector.Engine.POST(tsProfile.Endpoint+"/download/delete", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcGuiDownloadDelete)

	connector.Engine.POST(tsProfile.Endpoint+"/screen/setnote", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcScreenshotSetNote)
	connector.Engine.POST(tsProfile.Endpoint+"/screen/remove", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcScreenshotRemove)

	connector.Engine.POST(tsProfile.Endpoint+"/creds/add", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcCredentialsAdd)
	connector.Engine.POST(tsProfile.Endpoint+"/creds/edit", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcCredentialsEdit)
	connector.Engine.POST(tsProfile.Endpoint+"/creds/remove", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcCredentialsRemove)
	connector.Engine.POST(tsProfile.Endpoint+"/creds/set/tag", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcCredentialsSetTag)

	connector.Engine.POST(tsProfile.Endpoint+"/targets/add", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcTargetsAdd)
	connector.Engine.POST(tsProfile.Endpoint+"/targets/edit", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcTargetEdit)
	connector.Engine.POST(tsProfile.Endpoint+"/targets/remove", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcTargetRemove)
	connector.Engine.POST(tsProfile.Endpoint+"/targets/set/tag", token.ValidateAccessToken(), default404Middleware(tsResponse), connector.TcTargetSetTag)

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
	host := fmt.Sprintf("%s:%d", tc.Interface, tc.Port)
	err := tc.Engine.RunTLS(host, tc.Cert, tc.Key)
	if err != nil {
		logs.Error("", "Failed to start HTTP Server: "+err.Error())
		return
	}
	*finished <- true
}
