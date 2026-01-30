package connector

import (
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/token"
	"errors"
	"net/http"
	"strconv"
	"strings"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
)

const (
	ClientTypeUI  uint8 = 1
	ClientTypeWEB uint8 = 2
	ClientTypeCLI uint8 = 3
)

type Credentials struct {
	Username string `json:"username"`
	Password string `json:"password"`
	Version  string `json:"version"`
}

type AccessJWT struct {
	AccessToken string `json:"access_token"`
}

type SubscribeRequest struct {
	Categories      []string `json:"categories"`
	ConsoleTeamMode *bool    `json:"console_team_mode,omitempty"`
}

func (tc *TsConnector) tcLogin(ctx *gin.Context) {
	var (
		creds Credentials
		err   error
	)

	err = ctx.ShouldBindJSON(&creds)
	if err != nil {
		_ = ctx.Error(errors.New("invalid credentials"))
		return
	}

	recvHash := krypt.SHA256([]byte(creds.Password))

	if tc.OnlyHash {
		if recvHash != tc.Hash {
			_ = ctx.Error(errors.New("incorrect password"))
			return
		}
	} else {
		if recvHash != tc.Operators[creds.Username] {
			_ = ctx.Error(errors.New("incorrect password"))
			return
		}
	}

	accessToken, err := token.GenerateAccessToken(creds.Username, creds.Version)
	if err != nil {
		_ = ctx.Error(errors.New("could not generate access token"))
		return
	}

	refreshToken, err := token.GenerateRefreshToken(creds.Username, creds.Version)
	if err != nil {
		_ = ctx.Error(errors.New("could not generate refresh token"))
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"access_token": accessToken, "refresh_token": refreshToken})
}

func (tc *TsConnector) tcConnect(ctx *gin.Context) {
	value, exists := ctx.Get("username")
	if !exists {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: username not found in context", "ok": false})
		return
	}
	username, ok := value.(string)
	if !ok {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: invalid username type in context", "ok": false})
		return
	}

	versionValue, _ := ctx.Get("version")
	version, _ := versionValue.(string)

	exists = tc.teamserver.TsClientExists(username)
	if exists {
		ctx.JSON(http.StatusNetworkAuthenticationRequired, gin.H{"message": "Server error: invalid username type in context", "ok": false})
		return
	}

	clientTypeHeader := ctx.GetHeader("Client-Type")
	clientType := ClientTypeUI
	if clientTypeHeader != "" {
		if ct, err := strconv.Atoi(clientTypeHeader); err == nil {
			clientType = uint8(ct)
		}
	}

	teamModeHeader := ctx.GetHeader("Console-Team-Mode")
	consoleTeamMode := teamModeHeader == "true" || teamModeHeader == "1"

	subscriptionsHeader := ctx.GetHeader("Subscriptions")
	var subscriptions []string
	if subscriptionsHeader != "" {
		subscriptions = strings.Split(subscriptionsHeader, ",")
		for i := range subscriptions {
			subscriptions[i] = strings.TrimSpace(subscriptions[i])
		}
	}

	var wsUpgrader websocket.Upgrader
	wsUpgrader.CheckOrigin = func(r *http.Request) bool {
		return true
	}
	wsConn, err := wsUpgrader.Upgrade(ctx.Writer, ctx.Request, nil)
	if err != nil {
		logs.Error("", "WebSocket upgrade error: "+err.Error())
		return
	}

	if wsConn == nil {
		logs.Error("", "WebSocket is nil")
		return
	}

	go tc.tcWebsocketConnect(username, version, wsConn, clientType, consoleTeamMode, subscriptions)
}

func (tc *TsConnector) tcWebsocketConnect(username string, version string, wsConn *websocket.Conn, clientType uint8, consoleTeamMode bool, subscriptions []string) {
	tc.teamserver.TsClientConnect(username, version, wsConn, clientType, consoleTeamMode, subscriptions)
	for {
		_, _, err := wsConn.ReadMessage()
		if err == nil {
			continue
		}

		tc.teamserver.TsClientDisconnect(username)
		break
	}
}

func (tc *TsConnector) tcSync(ctx *gin.Context) {
	value, exists := ctx.Get("username")
	if !exists {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: username not found in context", "ok": false})
		return
	}
	username, ok := value.(string)
	if !ok {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: invalid username type in context", "ok": false})
		return
	}

	go tc.teamserver.TsClientSync(username)

	ctx.JSON(http.StatusOK, gin.H{"message": "sync started", "ok": true})
}

func (tc *TsConnector) tcSubscribe(ctx *gin.Context) {
	value, exists := ctx.Get("username")
	if !exists {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: username not found in context", "ok": false})
		return
	}
	username, ok := value.(string)
	if !ok {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: invalid username type in context", "ok": false})
		return
	}

	var req SubscribeRequest
	if err := ctx.ShouldBindJSON(&req); err != nil {
		ctx.JSON(http.StatusBadRequest, gin.H{"message": "Invalid subscribe request: " + err.Error(), "ok": false})
		return
	}

	if len(req.Categories) == 0 {
		ctx.JSON(http.StatusBadRequest, gin.H{"message": "No categories specified", "ok": false})
		return
	}

	go tc.teamserver.TsClientSubscribe(username, req.Categories, req.ConsoleTeamMode)

	ctx.JSON(http.StatusOK, gin.H{"message": "subscribe started", "ok": true})
}

func (tc *TsConnector) tcChannel(ctx *gin.Context) {
	value, exists := ctx.Get("username")
	if !exists {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: username not found in context", "ok": false})
		return
	}
	username, ok := value.(string)
	if !ok {
		ctx.JSON(http.StatusOK, gin.H{"message": "Server error: invalid username type in context", "ok": false})
		return
	}

	exists = tc.teamserver.TsClientExists(username)
	if !exists {
		ctx.JSON(http.StatusNetworkAuthenticationRequired, gin.H{"message": "Server error: client not connected", "ok": false})
		return
	}

	var wsUpgrader websocket.Upgrader
	wsUpgrader.CheckOrigin = func(r *http.Request) bool {
		return true
	}
	wsConn, err := wsUpgrader.Upgrade(ctx.Writer, ctx.Request, nil)
	if err != nil {
		logs.Error("", "WebSocket upgrade error: "+err.Error())
		return
	}

	if wsConn == nil {
		logs.Error("", "WebSocket is nil")
		return
	}

	ChannelType := ctx.GetHeader("Channel-Type")
	ChannelData := ctx.GetHeader("Channel-Data")

	switch ChannelType {
	case "tunnel":
		if err := tc.teamserver.TsTunnelClientNewChannel(ChannelData, wsConn); err != nil {
			logs.Error("", "Tunnel channel error: "+err.Error())
			wsConn.Close()
		}

	case "terminal":
		if err := tc.teamserver.TsAgentTerminalCreateChannel(ChannelData, wsConn); err != nil {
			logs.Error("", "Terminal channel error: "+err.Error())
			wsConn.Close()
		}

	case "agent_build":
		go func() {
			if err := tc.teamserver.TsAgentBuildCreateChannel(ChannelData, wsConn); err != nil {
				logs.Error("", "Agent build channel error: "+err.Error())
			}
		}()

	default:
		logs.Error("", "Unknown channel type: "+ChannelType)
		wsConn.Close()
	}
}
