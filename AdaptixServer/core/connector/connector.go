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
	"net/http"
)

type Teamserver interface {
	TsClientConnect(username string, socket *websocket.Conn)
	TsClientDisconnect(username string)

	TsListenerStart(listenerName string, configType string, config string, customData []byte) error
	TsListenerEdit(listenerName string, configType string, config string) error
	TsListenerStop(listenerName string, configType string) error
	TsListenerGetProfile(listenerName string, listenerType string) ([]byte, error)

	TsAgentGenetate(agentName string, config string, listenerProfile []byte) ([]byte, error)
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

func default404Middleware() gin.HandlerFunc {
	return func(c *gin.Context) {
		if len(c.Errors) > 0 && !c.Writer.Written() {
			c.String(http.StatusNotFound, "Default - Resource not found")
			c.Abort()
			return
		}

		c.Next()

		if len(c.Errors) > 0 && !c.Writer.Written() {
			c.String(http.StatusNotFound, "Default - Resource not found")
		}
	}
}

func NewTsConnector(ts Teamserver, p profile.TsProfile) (*TsConnector, error) {
	gin.SetMode(gin.ReleaseMode)

	var connector = new(TsConnector)
	connector.Engine = gin.New()
	connector.teamserver = ts
	connector.Port = p.Port
	connector.Endpoint = p.Endpoint
	connector.Hash = krypt.SHA256([]byte(p.Password))
	connector.Key = p.Key
	connector.Cert = p.Cert

	connector.Engine.POST(p.Endpoint+"/login", default404Middleware(), connector.tcLogin)
	connector.Engine.POST(p.Endpoint+"/refresh", default404Middleware(), token.RefreshTokenHandler)

	connector.Engine.GET(p.Endpoint+"/connect", default404Middleware(), connector.tcConnect)

	connector.Engine.POST(p.Endpoint+"/listener/create", token.ValidateAccessToken(), default404Middleware(), connector.TcListenerStart)
	connector.Engine.POST(p.Endpoint+"/listener/edit", token.ValidateAccessToken(), default404Middleware(), connector.TcListenerEdit)
	connector.Engine.POST(p.Endpoint+"/listener/stop", token.ValidateAccessToken(), default404Middleware(), connector.TcListenerStop)

	connector.Engine.POST(p.Endpoint+"/agent/generate", token.ValidateAccessToken(), default404Middleware(), connector.TcAgentGenerate)
	connector.Engine.POST(p.Endpoint+"/agent/command", token.ValidateAccessToken(), default404Middleware(), connector.TcAgentCommand)
	connector.Engine.POST(p.Endpoint+"/agent/remove", token.ValidateAccessToken(), default404Middleware(), connector.TcAgentRemove)
	connector.Engine.POST(p.Endpoint+"/agent/exit", token.ValidateAccessToken(), default404Middleware(), connector.TcAgentExit)
	connector.Engine.POST(p.Endpoint+"/agent/settag", token.ValidateAccessToken(), default404Middleware(), connector.TcAgentSetTag)
	connector.Engine.POST(p.Endpoint+"/agent/task/stop", token.ValidateAccessToken(), default404Middleware(), connector.TcAgentTaskStop)
	connector.Engine.POST(p.Endpoint+"/agent/task/delete", token.ValidateAccessToken(), default404Middleware(), connector.TcAgentTaskDelete)

	connector.Engine.POST(p.Endpoint+"/browser/download/state", token.ValidateAccessToken(), default404Middleware(), connector.TcBrowserDownloadState)
	connector.Engine.POST(p.Endpoint+"/browser/download/start", token.ValidateAccessToken(), default404Middleware(), connector.TcBrowserDownload)
	connector.Engine.POST(p.Endpoint+"/browser/disks", token.ValidateAccessToken(), default404Middleware(), connector.TcBrowserDisks)
	connector.Engine.POST(p.Endpoint+"/browser/files", token.ValidateAccessToken(), default404Middleware(), connector.TcBrowserFiles)
	connector.Engine.POST(p.Endpoint+"/browser/upload", token.ValidateAccessToken(), default404Middleware(), connector.TcBrowserUpload)
	connector.Engine.POST(p.Endpoint+"/browser/process", token.ValidateAccessToken(), default404Middleware(), connector.TcBrowserProcess)

	connector.Engine.NoRoute(default404Middleware(), func(c *gin.Context) { _ = c.Error(errors.New("NoRoute")) })

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
