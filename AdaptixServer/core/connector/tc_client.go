package connector

import (
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/token"
	"errors"
	"net/http"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/gorilla/websocket"
)

type Credentials struct {
	Username string `json:"username"`
	Password string `json:"password"`
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

	accessToken, err := token.GenerateAccessToken(creds.Username)
	if err != nil {
		_ = ctx.Error(errors.New("could not generate access token"))
		return
	}

	refreshToken, err := token.GenerateRefreshToken(creds.Username)
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

	// 先断开旧连接（如果存在）
	if tc.teamserver.TsClientExists(username) {
		tc.teamserver.TsClientDisconnect(username)
	}

	var wsUpgrader websocket.Upgrader
	// 增加握手超时，适应Cloudflare隧道延迟
	wsUpgrader.HandshakeTimeout = 15 * time.Second
	wsConn, err := wsUpgrader.Upgrade(ctx.Writer, ctx.Request, nil)
	if err != nil {
		logs.Error("", "WebSocket upgrade error: "+err.Error())
		return
	}

	if wsConn == nil {
		logs.Error("", "WebSocket is nil")
		return
	}

	go tc.tcWebsocketConnect(username, wsConn)
}

func (tc *TsConnector) tcWebsocketConnect(username string, wsConn *websocket.Conn) {
	ws := wsConn
	ws.SetReadDeadline(time.Now().Add(90 * time.Second))
	ws.SetPongHandler(func(string) error {
		ws.SetReadDeadline(time.Now().Add(90 * time.Second))
		return nil
	})

	tc.teamserver.TsClientConnect(username, wsConn)

	// 心跳ticker
	ticker := time.NewTicker(30 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			ws.SetWriteDeadline(time.Now().Add(5 * time.Second))
			if err := ws.WriteMessage(websocket.PingMessage, []byte{}); err != nil {
				tc.teamserver.TsClientDisconnect(username)
				return
			}
		default:
			_, _, err := ws.ReadMessage()
			if err == nil {
				continue
			}
			tc.teamserver.TsClientDisconnect(username)
			return
		}
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
	// 增加握手超时，适应Cloudflare隧道延迟
	wsUpgrader.HandshakeTimeout = 15 * time.Second
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
