package connector

import (
	"AdaptixServer/core/profile"
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/token"
	"crypto/tls"
	"errors"
	"fmt"
	"net/http"
	"strings"
	"time"

	"github.com/Adaptix-Framework/axc2"
	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
)

type Teamserver interface {
	CreateOTP(otpType string, data interface{}) (string, error)
	ValidateOTP(token string) (string, interface{}, bool)

	TsClientExists(username string) bool
	TsClientDisconnect(username string)
	TsClientConnect(username string, version string, socket *websocket.Conn, clientType uint8, consoleTeamMode bool, subscriptions []string)
	TsClientSync(username string)
	TsClientSubscribe(username string, categories []string, consoleTeamMode *bool)

	TsListenerList() (string, error)
	TsListenerStart(listenerName string, configType string, config string, createTime int64, watermark string, customData []byte) error
	TsListenerEdit(listenerName string, configType string, config string) error
	TsListenerStop(listenerName string, configType string) error
	TsListenerPause(listenerName string, configType string) error
	TsListenerResume(listenerName string, configType string) error
	TsListenerGetProfile(listenerName string) (string, []byte, error)
	TsListenerInteralHandler(watermark string, data []byte) (string, error)

	TsAgentList() (string, error)
	TsAgentIsExists(agentId string) bool
	TsAgentCreate(agentCrc string, agentId string, beat []byte, listenerName string, ExternalIP string, Async bool) (adaptix.AgentData, error)
	TsAgentProcessData(agentId string, bodyData []byte) error
	TsAgentGetHostedAll(agentId string, maxDataSize int) ([]byte, error)
	TsAgentCommand(agentName string, agentId string, clientName string, hookId string, handlerId string, cmdline string, ui bool, args map[string]any) error
	TsAgentBuildSyncOnce(agentName string, config string, listenersName []string) ([]byte, string, error)

	TsAgentUpdateData(newAgentData adaptix.AgentData) error
	TsAgentUpdateDataPartial(agentId string, updateData interface{}) error
	TsAgentTerminate(agentId string, terminateTaskId string) error
	TsAgentRemove(agentId string) error
	TsAgentConsoleRemove(agentId string) error
	TsAgentSetTick(agentId string, listenerName string) error
	TsAgentTickUpdate()

	TsAgentConsoleOutput(agentId string, messageType int, message string, clearText string, store bool)
	TsAgentConsoleOutputClient(agentId string, client string, messageType int, message string, clearText string)
	TsAgentConsoleErrorCommand(agentId string, client string, cmdline string, message string, HookId string, HandlerId string)

	TsTaskCreate(agentId string, cmdline string, client string, taskData adaptix.TaskData)
	TsTaskUpdate(agentId string, data adaptix.TaskData)
	TsTaskGetAvailableAll(agentId string, availableSize int) ([]adaptix.TaskData, error)
	TsTaskCancel(agentId string, taskId string) error
	TsTaskDelete(agentId string, taskId string) error
	TsTaskPostHook(hookData adaptix.TaskData, jobIndex int) error
	TsTaskSave(hookData adaptix.TaskData) error
	TsTaskListCompleted(agentId string, limit int, offset int) ([]byte, error)

	TsChatSendMessage(username string, message string)

	TsDownloadAdd(agentId string, fileId string, fileName string, fileSize int64) error
	TsDownloadUpdate(fileId string, state int, data []byte) error
	TsDownloadClose(fileId string, reason int) error

	TsDownloadList() (string, error)
	TsDownloadSync(fileId string) (string, []byte, error)
	TsDownloadDelete(fileId []string) error
	TsDownloadGetFilepath(fileId string) (string, error)
	TsUploadGetFilepath(fileId string) (string, error)
	TsUploadGetFileContent(fileId string) ([]byte, error)

	TsScreenshotList() (string, error)
	TsScreenshotGetImage(screenId string) ([]byte, error)
	TsScreenshotDelete(screenId string) error
	TsScreenshotNote(screenId string, note string) error

	TsCredentilsList() (string, error)
	TsCredentilsAdd(creds []map[string]interface{}) error
	TsCredentilsEdit(credId string, username string, password string, realm string, credType string, tag string, storage string, host string) error
	TsCredentilsDelete(credsId []string) error
	TsCredentialsSetTag(credsId []string, tag string) error

	TsTargetsList() (string, error)
	TsTargetsAdd(targets []map[string]interface{}) error
	TsTargetsEdit(targetId string, computer string, domain string, address string, os int, osDesk string, tag string, info string, alive bool) error
	TsTargetDelete(targetsId []string) error
	TsTargetSetTag(targetsId []string, tag string) error
	TsTargetRemoveSessions(agentsId []string) error

	TsClientGuiDisksWindows(taskData adaptix.TaskData, drives []adaptix.ListingDrivesDataWin)
	TsClientGuiFilesStatus(taskData adaptix.TaskData)
	TsClientGuiFilesWindows(taskData adaptix.TaskData, path string, files []adaptix.ListingFileDataWin)
	TsClientGuiFilesUnix(taskData adaptix.TaskData, path string, files []adaptix.ListingFileDataUnix)
	TsClientGuiProcessWindows(taskData adaptix.TaskData, process []adaptix.ListingProcessDataWin)
	TsClientGuiProcessUnix(taskData adaptix.TaskData, process []adaptix.ListingProcessDataUnix)

	TsAgentTerminalCreateChannel(terminalData string, wsconn *websocket.Conn) error
	TsAgentBuildCreateChannel(buildData string, wsconn *websocket.Conn) error

	TsTunnelList() (string, error)
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
	TsTunnelConnectionClose(channelId int, writeOnly bool)
	TsTunnelConnectionHalt(channelId int, errorCode byte)
	TsTunnelConnectionResume(AgentId string, channelId int, ioDirect bool)
	TsTunnelConnectionData(channelId int, data []byte)

	TsServiceLoad(configPath string) error
	TsServiceUnload(serviceName string) error
	TsServiceCall(serviceName string, operator string, function string, args string)
	TsServiceList() (string, error)

	TsAxScriptLoadUser(name string, script string) error
	TsAxScriptUnloadUser(name string) error
	TsAxScriptList() (string, error)
	TsAxScriptCommands() (string, error)
	TsAxScriptResolveHooks(agentName string, agentId string, listenerRegName string, os int, cmdline string, args map[string]interface{}) (string, string, bool, error)
	TsAxScriptIsServerHook(id string) bool
	TsAxScriptParseAndExecute(agentId string, username string, cmdline string) error
	AxGetAgentContext(agentId string) (agentName string, listenerRegName string, osType int, err error)
}

type TsConnector struct {
	Interface          string
	Port               int
	Hash               string
	OnlyHash           bool
	Operators          map[string]string
	Endpoint           string
	Cert               string
	Key                string
	ManagePasswordHash string

	httpServer *profile.TsHttpServer

	Engine                 *gin.Engine
	teamserver             Teamserver
	apiGroup               *gin.RouterGroup
	publicGroup            *gin.RouterGroup
	dynamicEndpoints       map[string]gin.HandlerFunc
	dynamicPublicEndpoints map[string]gin.HandlerFunc
}

func tlsVersionFromString(v string) (uint16, error) {
	s := strings.TrimSpace(strings.ToUpper(v))
	s = strings.ReplaceAll(s, "_", "")
	s = strings.ReplaceAll(s, "-", "")

	switch s {
	case "", "DEFAULT":
		return 0, nil
	case "TLS10", "TLS1.0":
		return tls.VersionTLS10, nil
	case "TLS11", "TLS1.1":
		return tls.VersionTLS11, nil
	case "TLS12", "TLS1.2":
		return tls.VersionTLS12, nil
	case "TLS13", "TLS1.3":
		return tls.VersionTLS13, nil
	default:
		return 0, errors.New("unsupported TLS version: " + v)
	}
}

func tlsCipherSuiteFromString(name string) (uint16, error) {
	key := strings.TrimSpace(strings.ToUpper(name))
	key = strings.ReplaceAll(key, "-", "_")
	key = strings.ReplaceAll(key, " ", "_")

	switch key {
	case "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256":
		return tls.TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256, nil
	case "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384":
		return tls.TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384, nil
	case "TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256":
		return tls.TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, nil
	case "TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384":
		return tls.TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384, nil
	case "TLS_RSA_WITH_AES_128_GCM_SHA256":
		return tls.TLS_RSA_WITH_AES_128_GCM_SHA256, nil
	case "TLS_RSA_WITH_AES_256_GCM_SHA384":
		return tls.TLS_RSA_WITH_AES_256_GCM_SHA384, nil
	default:
		return 0, errors.New("unsupported cipher suite: " + name)
	}
}

func limitTimeoutMiddleware(cfg profile.TsHTTPConfig) gin.HandlerFunc {
	return func(c *gin.Context) {
		timeout := time.Duration(cfg.RequestTimeoutSec) * time.Second
		if timeout <= 0 {
			timeout = 300 * time.Second
		}
		msg := cfg.RequestTimeoutMessage
		if msg == "" {
			msg = "504 Gateway Timeout"
		}

		handler := http.TimeoutHandler(
			http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
				c.Next()
			}),
			timeout,
			msg,
		)
		handler.ServeHTTP(c.Writer, c.Request)
	}
}

func default404Middleware(httpError profile.TsHttpError) gin.HandlerFunc {
	return func(c *gin.Context) {
		if len(c.Errors) > 0 && !c.Writer.Written() {
			for header, value := range httpError.Headers {
				c.Header(header, value)
			}
			c.String(httpError.Status, httpError.PageContent)
			c.Abort()
			return
		}

		c.Next()

		if len(c.Errors) > 0 && !c.Writer.Written() {
			for header, value := range httpError.Headers {
				c.Header(header, value)
			}
			c.String(httpError.Status, httpError.PageContent)
		}
	}
}

func NewTsConnector(ts Teamserver, tsProfile profile.TsProfile, httpServer profile.TsHttpServer) (*TsConnector, error) {
	gin.SetMode(gin.ReleaseMode)

	var connector = new(TsConnector)
	connector.Engine = gin.New()
	connector.Engine.Use(gin.Recovery())
	connector.teamserver = ts
	connector.Interface = tsProfile.Interface
	connector.Port = tsProfile.Port
	connector.Endpoint = tsProfile.Endpoint
	connector.Hash = krypt.SHA256([]byte(tsProfile.Password))
	connector.OnlyHash = tsProfile.OnlyPassword
	connector.Operators = make(map[string]string, len(tsProfile.Operators))
	for username, password := range tsProfile.Operators {
		connector.Operators[username] = krypt.SHA256([]byte(password))
	}
	connector.Key = tsProfile.Key
	connector.Cert = tsProfile.Cert
	if tsProfile.ManagePassword != "" {
		connector.ManagePasswordHash = krypt.SHA256([]byte(tsProfile.ManagePassword))
	}

	if httpServer.Error == nil {
		httpServer.Error = &profile.TsHttpError{}
	}
	if httpServer.Error.Status == 0 {
		httpServer.Error.Status = 404
	}
	if httpServer.Error.Headers == nil {
		httpServer.Error.Headers = map[string]string{}
	}

	if httpServer.HTTP == nil {
		httpServer.HTTP = &profile.TsHTTPConfig{}
	}
	if httpServer.HTTP.MaxHeaderBytes == 0 {
		httpServer.HTTP.MaxHeaderBytes = 8192
	}
	if httpServer.HTTP.RequestTimeoutSec == 0 {
		httpServer.HTTP.RequestTimeoutSec = 300
	}
	if httpServer.HTTP.RequestTimeoutMessage == "" {
		httpServer.HTTP.RequestTimeoutMessage = "504 Gateway Timeout"
	}

	if httpServer.TLS == nil {
		httpServer.TLS = &profile.TsTLSConfig{}
	}
	if httpServer.TLS.MinVersion == "" {
		httpServer.TLS.MinVersion = "TLS1.2"
	}
	if httpServer.TLS.MaxVersion == "" {
		httpServer.TLS.MaxVersion = "TLS1.3"
	}

	connector.httpServer = &httpServer
	connector.dynamicEndpoints = make(map[string]gin.HandlerFunc)
	connector.dynamicPublicEndpoints = make(map[string]gin.HandlerFunc)

	httpCfg := *httpServer.HTTP
	httpErr := *httpServer.Error

	public_group := connector.Engine.Group(tsProfile.Endpoint)
	public_group.Use(limitTimeoutMiddleware(httpCfg), default404Middleware(httpErr))
	connector.publicGroup = public_group

	login_group := connector.Engine.Group(tsProfile.Endpoint)
	login_group.Use(limitTimeoutMiddleware(httpCfg), default404Middleware(httpErr))
	{
		login_group.POST("/login", connector.tcLogin)
		login_group.POST("/refresh", token.RefreshTokenHandler)
	}

	otp_group := connector.Engine.Group(tsProfile.Endpoint)
	otp_group.Use(connector.validateOTPMiddleware(), default404Middleware(httpErr))
	{
		otp_group.POST("/otp/upload/temp", connector.tcOTP_UploadTemp)
		otp_group.GET("/otp/download/sync", connector.tcOTP_DownloadSync)
		otp_group.GET("/connect", connector.tcConnectOTP)
		otp_group.GET("/channel", connector.tcChannelOTP)
	}

	api_group := connector.Engine.Group(tsProfile.Endpoint)
	api_group.Use(limitTimeoutMiddleware(httpCfg), token.ValidateAccessToken(), default404Middleware(httpErr))
	connector.apiGroup = api_group
	{
		api_group.POST("/sync", connector.tcSync)
		api_group.POST("/subscribe", connector.tcSubscribe)
		api_group.POST("/otp/generate", connector.tcOTP_Generate)

		api_group.GET("/listener/list", connector.TcListenerList)
		api_group.POST("/listener/create", connector.TcListenerStart)
		api_group.POST("/listener/edit", connector.TcListenerEdit)
		api_group.POST("/listener/stop", connector.TcListenerStop)
		api_group.POST("/listener/pause", connector.TcListenerPause)
		api_group.POST("/listener/resume", connector.TcListenerResume)

		api_group.GET("/agent/list", connector.TcAgentList)
		api_group.POST("/agent/generate", connector.TcAgentGenerate)
		api_group.POST("/agent/remove", connector.TcAgentRemove)

		api_group.POST("/agent/command/file", connector.TcAgentCommandFile)
		api_group.POST("/agent/command/execute", connector.TcAgentCommandExecute)
		api_group.POST("/agent/command/raw", connector.TcAgentCommandRaw)
		api_group.POST("/agent/console/remove", connector.TcAgentConsoleRemove)
		api_group.POST("/agent/set/tag", connector.TcAgentSetTag)
		api_group.POST("/agent/set/mark", connector.TcAgentSetMark)
		api_group.POST("/agent/set/color", connector.TcAgentSetColor)
		api_group.POST("/agent/update/data", connector.TcAgentUpdateData)

		api_group.GET("/agent/task/list", connector.TcAgentTaskList)
		api_group.POST("/agent/task/cancel", connector.TcAgentTaskCancel)
		api_group.POST("/agent/task/delete", connector.TcAgentTaskDelete)
		api_group.POST("/agent/task/hook", connector.TcAgentTaskHook)
		api_group.POST("/agent/task/save", connector.TcAgentTaskSave)

		api_group.POST("/chat/send", connector.TcChatSendMessage)

		api_group.GET("/download/list", connector.TcDownloadList)
		api_group.POST("/download/sync", connector.TcGuiDownloadSync)
		api_group.POST("/download/delete", connector.TcGuiDownloadDelete)

		api_group.GET("/screen/list", connector.TcScreenshotList)
		api_group.GET("/screen/image", connector.TcScreenshotGetImage)
		api_group.POST("/screen/setnote", connector.TcScreenshotSetNote)
		api_group.POST("/screen/remove", connector.TcScreenshotRemove)

		api_group.GET("/creds/list", connector.TcCredentialsList)
		api_group.POST("/creds/add", connector.TcCredentialsAdd)
		api_group.POST("/creds/edit", connector.TcCredentialsEdit)
		api_group.POST("/creds/remove", connector.TcCredentialsRemove)
		api_group.POST("/creds/set/tag", connector.TcCredentialsSetTag)

		api_group.GET("/targets/list", connector.TcTargetsList)
		api_group.POST("/targets/add", connector.TcTargetsAdd)
		api_group.POST("/targets/edit", connector.TcTargetEdit)
		api_group.POST("/targets/remove", connector.TcTargetRemove)
		api_group.POST("/targets/set/tag", connector.TcTargetSetTag)

		api_group.GET("/tunnel/list", connector.TcTunnelList)
		api_group.POST("/tunnel/start/socks5", connector.TcTunnelStartSocks5)
		api_group.POST("/tunnel/start/socks4", connector.TcTunnelStartSocks4)
		api_group.POST("/tunnel/start/lportfwd", connector.TcTunnelStartLpf)
		api_group.POST("/tunnel/start/rportfwd", connector.TcTunnelStartRpf)
		api_group.POST("/tunnel/stop", connector.TcTunnelStop)
		api_group.POST("/tunnel/set/info", connector.TcTunnelSetIno)

		api_group.GET("/service/list", connector.TcServiceList)
		//api_group.POST("/service/load", connector.TcServiceLoad)
		//api_group.POST("/service/unload", connector.TcServiceUnload)
		api_group.POST("/service/call", connector.TcServiceCall)

		//api_group.POST("/axscript/list", connector.TcAxScriptList)
		//api_group.POST("/axscript/commands", connector.TcAxScriptCommands)
		//api_group.POST("/axscript/load", connector.TcAxScriptLoad)
		//api_group.POST("/axscript/unload", connector.TcAxScriptUnload)
	}

	connector.Engine.NoRoute(limitTimeoutMiddleware(httpCfg), default404Middleware(httpErr), func(c *gin.Context) { _ = c.Error(errors.New("NoRoute")) })

	return connector, nil
}

func (tc *TsConnector) endpointKey(method string, path string) string {
	return method + ":" + path
}

func (tc *TsConnector) RegisterEndpoint(method string, path string, handler func(c *gin.Context)) error {
	if tc.apiGroup == nil {
		return errors.New("api group not initialized")
	}

	key := tc.endpointKey(method, path)

	if _, exists := tc.dynamicEndpoints[key]; !exists {
		dispatcher := func(c *gin.Context) {
			if h, ok := tc.dynamicEndpoints[key]; ok {
				h(c)
			} else {
				c.JSON(404, gin.H{"error": "endpoint not found"})
			}
		}

		switch method {
		case "GET":
			tc.apiGroup.GET(path, dispatcher)
		case "POST":
			tc.apiGroup.POST(path, dispatcher)
		case "PUT":
			tc.apiGroup.PUT(path, dispatcher)
		case "DELETE":
			tc.apiGroup.DELETE(path, dispatcher)
		case "PATCH":
			tc.apiGroup.PATCH(path, dispatcher)
		default:
			return errors.New("unsupported HTTP method: " + method)
		}
	}

	tc.dynamicEndpoints[key] = handler
	return nil
}

func (tc *TsConnector) UnregisterEndpoint(method string, path string) error {
	key := tc.endpointKey(method, path)
	if _, exists := tc.dynamicEndpoints[key]; !exists {
		return errors.New("endpoint not registered: " + key)
	}
	delete(tc.dynamicEndpoints, key)
	return nil
}

func (tc *TsConnector) EndpointExists(method string, path string) bool {
	key := tc.endpointKey(method, path)
	_, exists := tc.dynamicEndpoints[key]
	return exists
}

func (tc *TsConnector) RegisterPublicEndpoint(method string, path string, handler func(c *gin.Context)) error {
	if tc.publicGroup == nil {
		return errors.New("public group not initialized")
	}

	key := tc.endpointKey(method, path)

	if _, exists := tc.dynamicPublicEndpoints[key]; !exists {
		dispatcher := func(c *gin.Context) {
			if h, ok := tc.dynamicPublicEndpoints[key]; ok {
				h(c)
			} else {
				c.JSON(404, gin.H{"error": "endpoint not found"})
			}
		}

		switch method {
		case "GET":
			tc.publicGroup.GET(path, dispatcher)
		case "POST":
			tc.publicGroup.POST(path, dispatcher)
		case "PUT":
			tc.publicGroup.PUT(path, dispatcher)
		case "DELETE":
			tc.publicGroup.DELETE(path, dispatcher)
		case "PATCH":
			tc.publicGroup.PATCH(path, dispatcher)
		default:
			return errors.New("unsupported HTTP method: " + method)
		}
	}

	tc.dynamicPublicEndpoints[key] = handler
	return nil
}

func (tc *TsConnector) UnregisterPublicEndpoint(method string, path string) error {
	key := tc.endpointKey(method, path)
	if _, exists := tc.dynamicPublicEndpoints[key]; !exists {
		return errors.New("public endpoint not registered: " + key)
	}
	delete(tc.dynamicPublicEndpoints, key)
	return nil
}

func (tc *TsConnector) PublicEndpointExists(method string, path string) bool {
	key := tc.endpointKey(method, path)
	_, exists := tc.dynamicPublicEndpoints[key]
	return exists
}

func (tc *TsConnector) Start(finished *chan bool) {
	host := fmt.Sprintf("%s:%d", tc.Interface, tc.Port)

	if tc.httpServer == nil || tc.httpServer.HTTP == nil || tc.httpServer.TLS == nil {
		logs.Error("", "HTTP server configuration is not initialized")
		return
	}

	httpCfg := *tc.httpServer.HTTP
	tlsCfgProfile := *tc.httpServer.TLS

	minTLS, err := tlsVersionFromString(tlsCfgProfile.MinVersion)
	if err != nil {
		logs.Error("", "Invalid TLS min_version: "+err.Error())
		return
	}
	maxTLS, err := tlsVersionFromString(tlsCfgProfile.MaxVersion)
	if err != nil {
		logs.Error("", "Invalid TLS max_version: "+err.Error())
		return
	}
	if minTLS != 0 && maxTLS != 0 && minTLS > maxTLS {
		logs.Error("", "Invalid TLS version range: min_version (%v) must be <= max_version (%v)", tlsCfgProfile.MinVersion, tlsCfgProfile.MaxVersion)
		return
	}

	var cipherSuites []uint16
	if tlsCfgProfile.CipherSuites != nil {
		cipherSuites = make([]uint16, 0, len(tlsCfgProfile.CipherSuites))
		for _, cs := range tlsCfgProfile.CipherSuites {
			id, err := tlsCipherSuiteFromString(cs)
			if err != nil {
				logs.Error("", "Invalid TLS cipher_suites: "+err.Error())
				return
			}
			cipherSuites = append(cipherSuites, id)
		}
	}

	tlsConfig := &tls.Config{
		PreferServerCipherSuites: false,
	}
	if minTLS != 0 {
		tlsConfig.MinVersion = minTLS
	}
	if maxTLS != 0 {
		tlsConfig.MaxVersion = maxTLS
	}
	if cipherSuites != nil {
		tlsConfig.CipherSuites = cipherSuites
	}
	if tlsCfgProfile.PreferServerCipherSuites != nil {
		tlsConfig.PreferServerCipherSuites = *tlsCfgProfile.PreferServerCipherSuites
	}

	server := &http.Server{
		Addr:           host,
		Handler:        tc.Engine,
		TLSConfig:      tlsConfig,
		ReadTimeout:    time.Duration(httpCfg.ReadTimeoutSec) * time.Second,
		WriteTimeout:   time.Duration(httpCfg.WriteTimeoutSec) * time.Second,
		IdleTimeout:    time.Duration(httpCfg.IdleTimeoutSec) * time.Second,
		MaxHeaderBytes: httpCfg.MaxHeaderBytes,
	}
	if httpCfg.ReadHeaderTimeoutSec > 0 {
		server.ReadHeaderTimeout = time.Duration(httpCfg.ReadHeaderTimeoutSec) * time.Second
	}
	server.SetKeepAlivesEnabled(!httpCfg.DisableKeepAlives)
	if httpCfg.EnableHTTP2 != nil && !*httpCfg.EnableHTTP2 {
		server.TLSNextProto = make(map[string]func(*http.Server, *tls.Conn, http.Handler))
	}

	err = server.ListenAndServeTLS(tc.Cert, tc.Key)
	//err := tc.Engine.RunTLS(host, tc.Cert, tc.Key)
	if err != nil {
		logs.Error("", "Failed to start HTTP Server: "+err.Error())
		return
	}
	*finished <- true
}
