package connector

import (
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/token"
	"errors"
	"net/http"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
)

type Credentials struct {
	Username string `json:"username"`
	Password string `json:"password"`
	Version  string `json:"version"` // Client version, e.g., "0.11.0"
}

type AccessJWT struct {
	AccessToken string `json:"access_token"`
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

	// Extract version from JWT token (empty string for old clients)
	versionValue, _ := ctx.Get("version")
	version, _ := versionValue.(string)

	exists = tc.teamserver.TsClientExists(username)
	if exists {
		ctx.JSON(http.StatusNetworkAuthenticationRequired, gin.H{"message": "Server error: invalid username type in context", "ok": false})
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

	go tc.tcWebsocketConnect(username, version, wsConn)
}

func (tc *TsConnector) tcWebsocketConnect(username string, version string, wsConn *websocket.Conn) {
	tc.teamserver.TsClientConnect(username, version, wsConn)
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
		ctx.JSON(http.StatusNetworkAuthenticationRequired, gin.H{"message": "Server error: invalid username type in context", "ok": false})
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

	switch ChannelType {

	case "tunnel":
		tunnelData := ctx.GetHeader("Channel-Data")
		_ = tc.teamserver.TsTunnelClientNewChannel(tunnelData, wsConn)

	case "terminal":
		terminalData := ctx.GetHeader("Channel-Data")
		_ = tc.teamserver.TsAgentTerminalCreateChannel(terminalData, wsConn)
	}
}
