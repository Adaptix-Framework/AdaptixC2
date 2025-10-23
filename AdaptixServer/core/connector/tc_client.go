package connector

import (
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/token"
	"errors"
	"net/http"
	"sync"
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

	// 获取或创建该用户的连接锁
	lockValue, exists := tc.connectLocks.Get(username)
	var userLock *sync.Mutex
	if !exists {
		userLock = &sync.Mutex{}
		tc.connectLocks.Put(username, userLock)
	} else {
		userLock = lockValue.(*sync.Mutex)
	}

	// 加锁，防止同一用户的并发连接请求互相干扰
	userLock.Lock()
	defer userLock.Unlock()

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

// writePump handles all writes to the websocket connection (Gorilla official pattern).
// It is the ONLY goroutine that writes to the websocket, ensuring concurrency safety.
// Ping heartbeats are sent from this same goroutine to avoid lock contention.
func (tc *TsConnector) writePump(username string, ws *websocket.Conn) {
	ticker := time.NewTicker(30 * time.Second) // Ping interval
	defer func() {
		ticker.Stop()
		ws.Close()
	}()

	sendChan := tc.teamserver.TsClientSendChannel(username)
	if sendChan == nil {
		return
	}
	heartbeatStop := tc.teamserver.TsClientHeartbeatStop(username)

	for {
		select {
		case message, ok := <-sendChan:
			ws.SetWriteDeadline(time.Now().Add(10 * time.Second))
			if !ok {
				// The send channel was closed, close the websocket
				ws.WriteMessage(websocket.CloseMessage, []byte{})
				return
			}

			// Write the first message
			if err := ws.WriteMessage(websocket.BinaryMessage, message); err != nil {
				return
			}

			// Batch optimization: drain the send channel and write all pending messages
			// This significantly improves performance when there are many queued messages
			n := len(sendChan)
			for i := 0; i < n; i++ {
				msg := <-sendChan
				if err := ws.WriteMessage(websocket.BinaryMessage, msg); err != nil {
					return
				}
			}

		case <-ticker.C:
			// Send Ping heartbeat
			ws.SetWriteDeadline(time.Now().Add(5 * time.Second))
			if err := ws.WriteMessage(websocket.PingMessage, nil); err != nil {
				return
			}

		case <-heartbeatStop:
			// Client disconnect signal
			return
		}
	}
}

// readPump handles all reads from the websocket connection.
// It is the ONLY goroutine that reads from the websocket.
func (tc *TsConnector) readPump(username string, ws *websocket.Conn) {
	defer func() {
		// Check if this socket is still the active connection for the user
		if tc.teamserver.TsClientSocketMatch(username, ws) {
			tc.teamserver.TsClientDisconnect(username)
		}
	}()

	// Set read deadline and pong handler
	ws.SetReadDeadline(time.Now().Add(90 * time.Second)) // Restored to 90s
	ws.SetPongHandler(func(string) error {
		ws.SetReadDeadline(time.Now().Add(90 * time.Second))
		return nil
	})

	// Read loop - blocks on ReadMessage
	for {
		if !tc.teamserver.TsClientSocketMatch(username, ws) {
			return
		}
		_, _, err := ws.ReadMessage()
		if err != nil {
			return
		}
	}
}

func (tc *TsConnector) tcWebsocketConnect(username string, wsConn *websocket.Conn) {
	// Register the client
	tc.teamserver.TsClientConnect(username, wsConn)

	// Launch writePump (handles all writes including Ping heartbeats)
	go tc.writePump(username, wsConn)

	// Launch readPump (handles all reads including Pong responses)
	// This function blocks until the connection is closed
	tc.readPump(username, wsConn)
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
