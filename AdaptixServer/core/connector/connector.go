package connector

import (
	"AdaptixServer/core/profile"
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/token"
	"crypto/tls"
	"errors"
	"fmt"
	"log"
	"net/http"
	"strings"
	"time"

	"github.com/Adaptix-Framework/axc2/v2"
	"github.com/Adaptix-Framework/axsafe"
	"github.com/gin-gonic/gin"
)

const SMALL_VERSION = "v2.0"

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
	teamserver             adaptix.Teamserver
	apiGroup               *gin.RouterGroup
	publicGroup            *gin.RouterGroup
	dynamicEndpoints       axsafe.Map[string, gin.HandlerFunc]
	dynamicPublicEndpoints axsafe.Map[string, gin.HandlerFunc]
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

func mustUsername(ctx *gin.Context) (string, bool) {
	value, exists := ctx.Get("username")
	if !exists {
		respondError(ctx, http.StatusOK, "Server error: username not found in context")
		return "", false
	}
	username, ok := value.(string)
	if !ok {
		respondError(ctx, http.StatusOK, "Server error: username is not a string")
		return "", false
	}
	return username, true
}

func respondError(ctx *gin.Context, status int, message string) {
	ctx.JSON(status, gin.H{"message": message, "ok": false})
}

func respondOK(ctx *gin.Context) {
	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}

func respondOKMessage(ctx *gin.Context, message any) {
	ctx.JSON(http.StatusOK, gin.H{"message": message, "ok": true})
}

func NewTsConnector(ts adaptix.Teamserver, tsProfile profile.TsProfile, httpServer profile.TsHttpServer) (*TsConnector, error) {
	gin.SetMode(gin.ReleaseMode)

	var connector = new(TsConnector)
	connector.Engine = gin.New()
	// connector.Engine.Use(gin.LoggerWithWriter(ts.TsLogWriter(adaptix.LogStatusInfo, "server:gin"))) /// ToDo
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
	connector.dynamicEndpoints = axsafe.NewMap[string, gin.HandlerFunc]()
	connector.dynamicPublicEndpoints = axsafe.NewMap[string, gin.HandlerFunc]()

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
		api_group.POST("/listener/connector", connector.TcListenerConnector)
		api_group.POST("/listener/tags", connector.TcListenerSetTags)

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

		api_group.GET("/group/list", connector.TcGroupList)
		api_group.POST("/group/create", connector.TcGroupCreate)
		api_group.POST("/group/rename", connector.TcGroupRename)
		api_group.POST("/group/delete", connector.TcGroupDelete)
		api_group.POST("/group/members", connector.TcGroupMembers)
		api_group.POST("/group/reparent", connector.TcGroupReparent)

		api_group.GET("/agent/task/list", connector.TcAgentTaskList)
		api_group.GET("/agent/console/list", connector.TcAgentConsoleList)
		api_group.GET("/agent/console/search", connector.TcAgentConsoleSearch)
		api_group.POST("/agent/task/cancel", connector.TcAgentTaskCancel)
		api_group.POST("/agent/task/delete", connector.TcAgentTaskDelete)
		api_group.POST("/agent/task/hook", connector.TcAgentTaskHook)
		api_group.POST("/agent/task/save", connector.TcAgentTaskSave)

		api_group.GET("/logs/list", connector.TcLogsList)

		api_group.POST("/chat/send", connector.TcChatSendMessage)
		api_group.POST("/chat/:id/edit", connector.TcChatEditMessage)
		api_group.POST("/chat/:id/delete", connector.TcChatDeleteMessage)
		api_group.POST("/chat/:id/react", connector.TcChatReaction)
		api_group.GET("/chat/todo", connector.TcChatGetTodo)
		api_group.POST("/chat/todo", connector.TcChatUpdateTodo)
		api_group.GET("/chat/history", connector.TcChatHistory)
		api_group.GET("/chat/search", connector.TcChatSearch)
		api_group.POST("/chat/clear", connector.TcChatClear)

		api_group.GET("/download/list", connector.TcDownloadList)
		api_group.POST("/download/sync", connector.TcGuiDownloadSync)
		api_group.POST("/download/delete", connector.TcGuiDownloadDelete)
		api_group.POST("/download/set/tag", connector.TcDownloadSetTag)

		api_group.GET("/upload/list", connector.TcUploadList)
		api_group.POST("/upload/delete", connector.TcUploadDelete)

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

	if !tc.dynamicEndpoints.Contains(key) {
		dispatcher := func(c *gin.Context) {
			if h, ok := tc.dynamicEndpoints.Get(key); ok {
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

	tc.dynamicEndpoints.Put(key, handler)
	return nil
}

func (tc *TsConnector) UnregisterEndpoint(method string, path string) error {
	key := tc.endpointKey(method, path)
	if !tc.dynamicEndpoints.Contains(key) {
		return errors.New("endpoint not registered: " + key)
	}
	tc.dynamicEndpoints.Delete(key)
	return nil
}

func (tc *TsConnector) EndpointExists(method string, path string) bool {
	key := tc.endpointKey(method, path)
	return tc.dynamicEndpoints.Contains(key)
}

func (tc *TsConnector) RegisterPublicEndpoint(method string, path string, handler func(c *gin.Context)) error {
	if tc.publicGroup == nil {
		return errors.New("public group not initialized")
	}

	key := tc.endpointKey(method, path)

	if !tc.dynamicPublicEndpoints.Contains(key) {
		dispatcher := func(c *gin.Context) {
			if h, ok := tc.dynamicPublicEndpoints.Get(key); ok {
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

	tc.dynamicPublicEndpoints.Put(key, handler)
	return nil
}

func (tc *TsConnector) UnregisterPublicEndpoint(method string, path string) error {
	key := tc.endpointKey(method, path)
	if !tc.dynamicPublicEndpoints.Contains(key) {
		return errors.New("public endpoint not registered: " + key)
	}
	tc.dynamicPublicEndpoints.Delete(key)
	return nil
}

func (tc *TsConnector) PublicEndpointExists(method string, path string) bool {
	key := tc.endpointKey(method, path)
	return tc.dynamicPublicEndpoints.Contains(key)
}

func (tc *TsConnector) Start(finished *chan bool) {
	host := fmt.Sprintf("%s:%d", tc.Interface, tc.Port)

	if tc.httpServer == nil || tc.httpServer.HTTP == nil || tc.httpServer.TLS == nil {
		tc.teamserver.TsLogAdd(adaptix.LogStatusError, 0, "connector", "HTTP server configuration is not initialized")
		return
	}

	httpCfg := *tc.httpServer.HTTP
	tlsCfgProfile := *tc.httpServer.TLS

	minTLS, err := tlsVersionFromString(tlsCfgProfile.MinVersion)
	if err != nil {
		tc.teamserver.TsLogAdd(adaptix.LogStatusError, 0, "connector", "Invalid TLS min_version: %s", err.Error())
		return
	}
	maxTLS, err := tlsVersionFromString(tlsCfgProfile.MaxVersion)
	if err != nil {
		tc.teamserver.TsLogAdd(adaptix.LogStatusError, 0, "connector", "Invalid TLS max_version: %s", err.Error())
		return
	}
	if minTLS != 0 && maxTLS != 0 && minTLS > maxTLS {
		tc.teamserver.TsLogAdd(adaptix.LogStatusError, 0, "connector", "Invalid TLS version range: min_version (%v) must be <= max_version (%v)", tlsCfgProfile.MinVersion, tlsCfgProfile.MaxVersion)
		return
	}

	var cipherSuites []uint16
	if tlsCfgProfile.CipherSuites != nil {
		cipherSuites = make([]uint16, 0, len(tlsCfgProfile.CipherSuites))
		for _, cs := range tlsCfgProfile.CipherSuites {
			id, err := tlsCipherSuiteFromString(cs)
			if err != nil {
				tc.teamserver.TsLogAdd(adaptix.LogStatusError, 0, "connector", "Invalid TLS cipher_suites: %s", err.Error())
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
		ErrorLog:       log.New(tc.teamserver.TsLogWriter(adaptix.LogStatusWarn, "server:http"), "", 0),
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
		tc.teamserver.TsLogAdd(adaptix.LogStatusError, 0, "connector", "Failed to start HTTP Server: %s", err.Error())
		*finished <- true
		return
	}
	*finished <- true
}
