package connector

import (
	"AdaptixServer/core/profile"
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/token"
	"errors"
	"fmt"
	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
	"os"
)

type Teamserver interface {
	TsClientConnect(username string, socket *websocket.Conn)
	TsClientDisconnect(username string)

	TsListenerStart(listenerName string, configType string, config string, customData []byte) error
	TsListenerEdit(listenerName string, configType string, config string) error
	TsListenerStop(listenerName string, configType string) error
	TsListenerGetProfile(listenerName string, listenerType string) ([]byte, error)

	TsAgentGenetate(agentName string, config string, listenerProfile []byte) ([]byte, string, error)
	TsAgentRequest(agentType string, agentId string, beat []byte, bodyData []byte, listenerName string, ExternalIP string) ([]byte, error)
	TsAgentConsoleOutput(agentId string, messageType int, message string, clearText string)
	TsAgentUpdateData(newAgentObject []byte) error
	TsAgentCommand(agentName string, agentId string, username string, cmdline string, args map[string]any) error
	TsAgentCtxExit(agentId string, username string) error
	TsAgentRemove(agentId string) error
	TsAgentSetTag(agentId string, tag string) error

	TsTaskQueueAddQuite(agentId string, taskObject []byte)
	TsTaskUpdate(agentId string, cTaskObject []byte)
	TsTaskQueueGetAvailable(agentId string, availableSize int) ([][]byte, error)
	TsTaskStop(agentId string, taskId string) error
	TsTaskDelete(agentId string, taskId string) error

	TsDownloadAdd(agentId string, fileId string, fileName string, fileSize int) error
	TsDownloadUpdate(fileId string, state int, data []byte) error
	TsDownloadClose(fileId string, reason int) error
	TsDownloadSync(fileId string) (string, []byte, error)
	TsDownloadDelete(fileId string) error

	TsDownloadChangeState(fileId string, username string, command string) error
	TsAgentBrowserDisks(agentId string, username string) error
	TsAgentBrowserProcess(agentId string, username string) error
	TsAgentBrowserFiles(agentId string, path string, username string) error
	TsAgentBrowserUpload(agentId string, path string, content []byte, username string) error
	TsAgentBrowserDownload(agentId string, path string, username string) error

	TsClientBrowserDisks(jsonTask string, jsonDrives string)
	TsClientBrowserFiles(jsonTask string, path string, jsonFiles string)
	TsClientBrowserFilesStatus(jsonTask string)
	TsClientBrowserProcess(jsonTask string, jsonFiles string)

	TsTunnelStop(TunnelId string) error
	TsTunnelSetInfo(TunnelId string, Info string) error
	TsTunnelCreateSocks4(AgentId string, Address string, Port int, FuncMsgConnectTCP func(channelId int, addr string, port int) []byte, FuncMsgWriteTCP func(channelId int, data []byte) []byte, FuncMsgClose func(channelId int) []byte) (string, error)
	TsTunnelCreateSocks5(AgentId string, Address string, Port int, FuncMsgConnectTCP, FuncMsgConnectUDP func(channelId int, addr string, port int) []byte, FuncMsgWriteTCP, FuncMsgWriteUDP func(channelId int, data []byte) []byte, FuncMsgClose func(channelId int) []byte) (string, error)
	TsTunnelCreateSocks5Auth(AgentId string, Address string, Port int, Username string, Password string, FuncMsgConnectTCP, FuncMsgConnectUDP func(channelId int, addr string, port int) []byte, FuncMsgWriteTCP, FuncMsgWriteUDP func(channelId int, data []byte) []byte, FuncMsgClose func(channelId int) []byte) (string, error)
	TsTunnelStopSocks(AgentId string, Port int)
	TsTunnelCreateLocalPortFwd(AgentId string, Address string, Port int, FwdAddress string, FwdPort int, FuncMsgConnect func(channelId int, addr string, port int) []byte, FuncMsgWrite func(channelId int, data []byte) []byte, FuncMsgClose func(channelId int) []byte) (string, error)
	TsTunnelStopLocalPortFwd(AgentId string, Port int)
	TsTunnelConnectionClose(channelId int)
	TsTunnelConnectionResume(AgentId string, channelId int)
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

func default404Middleware(tsResponce profile.TsResponse) gin.HandlerFunc {
	return func(c *gin.Context) {
		if len(c.Errors) > 0 && !c.Writer.Written() {
			for header, value := range tsResponce.Headers {
				c.Header(header, value)
			}
			c.String(tsResponce.Status, tsResponce.PageContent)
			c.Abort()
			return
		}

		c.Next()

		if len(c.Errors) > 0 && !c.Writer.Written() {
			for header, value := range tsResponce.Headers {
				c.Header(header, value)
			}
			c.String(tsResponce.Status, tsResponce.PageContent)
		}
	}
}

func NewTsConnector(ts Teamserver, tsProfile profile.TsProfile, tsResponce profile.TsResponse) (*TsConnector, error) {
	gin.SetMode(gin.ReleaseMode)

	if tsResponce.PagePath != "" {
		fileContent, _ := os.ReadFile(tsResponce.PagePath)
		tsResponce.PageContent = string(fileContent)
	}

	var connector = new(TsConnector)
	connector.Engine = gin.New()
	connector.teamserver = ts
	connector.Port = tsProfile.Port
	connector.Endpoint = tsProfile.Endpoint
	connector.Hash = krypt.SHA256([]byte(tsProfile.Password))
	connector.Key = tsProfile.Key
	connector.Cert = tsProfile.Cert

	connector.Engine.POST(tsProfile.Endpoint+"/login", default404Middleware(tsResponce), connector.tcLogin)
	connector.Engine.POST(tsProfile.Endpoint+"/refresh", default404Middleware(tsResponce), token.RefreshTokenHandler)

	connector.Engine.GET(tsProfile.Endpoint+"/connect", default404Middleware(tsResponce), connector.tcConnect)

	connector.Engine.POST(tsProfile.Endpoint+"/listener/create", token.ValidateAccessToken(), default404Middleware(tsResponce), connector.TcListenerStart)
	connector.Engine.POST(tsProfile.Endpoint+"/listener/edit", token.ValidateAccessToken(), default404Middleware(tsResponce), connector.TcListenerEdit)
	connector.Engine.POST(tsProfile.Endpoint+"/listener/stop", token.ValidateAccessToken(), default404Middleware(tsResponce), connector.TcListenerStop)

	connector.Engine.POST(tsProfile.Endpoint+"/agent/generate", token.ValidateAccessToken(), default404Middleware(tsResponce), connector.TcAgentGenerate)
	connector.Engine.POST(tsProfile.Endpoint+"/agent/command", token.ValidateAccessToken(), default404Middleware(tsResponce), connector.TcAgentCommand)
	connector.Engine.POST(tsProfile.Endpoint+"/agent/remove", token.ValidateAccessToken(), default404Middleware(tsResponce), connector.TcAgentRemove)
	connector.Engine.POST(tsProfile.Endpoint+"/agent/exit", token.ValidateAccessToken(), default404Middleware(tsResponce), connector.TcAgentExit)
	connector.Engine.POST(tsProfile.Endpoint+"/agent/settag", token.ValidateAccessToken(), default404Middleware(tsResponce), connector.TcAgentSetTag)

	connector.Engine.POST(tsProfile.Endpoint+"/agent/task/stop", token.ValidateAccessToken(), default404Middleware(tsResponce), connector.TcAgentTaskStop)
	connector.Engine.POST(tsProfile.Endpoint+"/agent/task/delete", token.ValidateAccessToken(), default404Middleware(tsResponce), connector.TcAgentTaskDelete)

	connector.Engine.POST(tsProfile.Endpoint+"/browser/download/state", token.ValidateAccessToken(), default404Middleware(tsResponce), connector.TcBrowserDownloadState)
	connector.Engine.POST(tsProfile.Endpoint+"/browser/download/start", token.ValidateAccessToken(), default404Middleware(tsResponce), connector.TcBrowserDownload)
	connector.Engine.POST(tsProfile.Endpoint+"/browser/disks", token.ValidateAccessToken(), default404Middleware(tsResponce), connector.TcBrowserDisks)
	connector.Engine.POST(tsProfile.Endpoint+"/browser/files", token.ValidateAccessToken(), default404Middleware(tsResponce), connector.TcBrowserFiles)
	connector.Engine.POST(tsProfile.Endpoint+"/browser/upload", token.ValidateAccessToken(), default404Middleware(tsResponce), connector.TcBrowserUpload)
	connector.Engine.POST(tsProfile.Endpoint+"/browser/process", token.ValidateAccessToken(), default404Middleware(tsResponce), connector.TcBrowserProcess)

	connector.Engine.POST(tsProfile.Endpoint+"/tunnel/stop", token.ValidateAccessToken(), default404Middleware(tsResponce), connector.TcTunnelStop)
	connector.Engine.POST(tsProfile.Endpoint+"/tunnel/setinfo", token.ValidateAccessToken(), default404Middleware(tsResponce), connector.TcTunnelSetIno)

	connector.Engine.NoRoute(default404Middleware(tsResponce), func(c *gin.Context) { _ = c.Error(errors.New("NoRoute")) })

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
