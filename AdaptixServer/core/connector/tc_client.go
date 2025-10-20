package connector

import (
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/token"
	"errors"
	"net"
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
	// 允许所有Origin，因为通过Cloudflare隧道时Origin会不匹配，且已有JWT验证保护
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

	// 清理函数：只有当socket仍然是当前用户的连接时才断开
	defer func() {
		// 检查这个socket是否仍然是当前用户的活跃连接
		if tc.teamserver.TsClientSocketMatch(username, wsConn) {
			tc.teamserver.TsClientDisconnect(username)
		}
	}()

	// 启动心跳发送goroutine
	go func() {
		for {
			if !tc.teamserver.TsClientSocketMatch(username, wsConn) {
				return
			}
			select {
			case <-ticker.C:
				if err := tc.teamserver.TsClientWriteControl(username, websocket.PingMessage, nil); err != nil {
					if ne, ok := err.(net.Error); ok && ne.Timeout() {
						continue
					}
					ws.Close()
					return
				}
			}
		}
	}()

	// 主循环：阻塞读取消息（包括客户端发送的Pong响应）
	for {
		if !tc.teamserver.TsClientSocketMatch(username, wsConn) {
			return
		}
		_, _, err := ws.ReadMessage()
		if err != nil {
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

	// 启动异步同步，立即返回200响应
	// 数据会通过WebSocket（/connect）以二进制消息形式发送到客户端
	go tc.teamserver.TsClientSync(username)

	// 立即返回200 OK响应，让HTTP请求完成
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
	// 增加握手超时，适应Cloudflare隧道延迟
	wsUpgrader.HandshakeTimeout = 15 * time.Second
	// 允许所有Origin，因为通过Cloudflare隧道时Origin会不匹配，且已有JWT验证保护
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
