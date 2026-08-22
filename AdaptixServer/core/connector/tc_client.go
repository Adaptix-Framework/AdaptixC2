package connector

import (
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/token"
	"crypto/subtle"
	"errors"
	"net/http"
	"strconv"
	"strings"

	"github.com/Adaptix-Framework/axc2/v2"
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
		if creds.Username == "" {
			_ = ctx.Error(errors.New("username is required"))
			return
		}
		if subtle.ConstantTimeCompare([]byte(recvHash), []byte(tc.Hash)) != 1 {
			_ = ctx.Error(errors.New("incorrect password"))
			return
		}
	} else {
		expected, ok := tc.Operators[creds.Username]
		if !ok || subtle.ConstantTimeCompare([]byte(recvHash), []byte(expected)) != 1 {
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

	ctx.JSON(http.StatusOK, gin.H{"access_token": accessToken, "refresh_token": refreshToken, "version": SMALL_VERSION})
}

func (tc *TsConnector) tcWebsocketConnect(username string, wsConn *websocket.Conn, clientType uint8, consoleTeamMode bool, subscriptions []string) {
	tc.teamserver.TsClientConnect(username, wsConn, clientType, consoleTeamMode, subscriptions)
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
	username, ok := mustUsername(ctx)
	if !ok {
		return
	}

	go tc.teamserver.TsClientSync(username)

	respondOKMessage(ctx, "sync started")
}

func (tc *TsConnector) tcSubscribe(ctx *gin.Context) {
	username, ok := mustUsername(ctx)
	if !ok {
		return
	}

	var req SubscribeRequest
	if err := ctx.ShouldBindJSON(&req); err != nil {
		respondError(ctx, http.StatusBadRequest, "Invalid subscribe request: "+err.Error())
		return
	}

	if len(req.Categories) == 0 {
		respondError(ctx, http.StatusBadRequest, "No categories specified")
		return
	}

	go tc.teamserver.TsClientSubscribe(username, req.Categories, req.ConsoleTeamMode)

	respondOKMessage(ctx, "subscribe started")
}

/// OTP

func (tc *TsConnector) tcConnectOTP(ctx *gin.Context) {
	otpType, _ := ctx.Get("otpType")
	if otpType != "connect" {
		_ = ctx.Error(errors.New("invalid OTP type"))
		return
	}

	data, _ := ctx.Get("otpData")
	wsData, ok := data.(ConnectOTPData)
	if !ok {
		_ = ctx.Error(errors.New("invalid OTP data"))
		return
	}

	exists := tc.teamserver.TsClientExists(wsData.Username)
	if exists {
		respondError(ctx, http.StatusNetworkAuthenticationRequired, "Client already connected")
		return
	}

	clientType := wsData.ClientType
	if clientType == 0 {
		clientType = ClientTypeUI
	}

	var wsUpgrader websocket.Upgrader
	wsUpgrader.CheckOrigin = func(r *http.Request) bool {
		return true
	}
	wsConn, err := wsUpgrader.Upgrade(ctx.Writer, ctx.Request, nil)
	if err != nil {
		tc.teamserver.TsLogAdd(adaptix.LogStatusError, 0, "server", "connector", "WebSocket upgrade error: %s", err.Error())
		return
	}

	if wsConn == nil {
		tc.teamserver.TsLogAdd(adaptix.LogStatusError, 0, "server", "connector", "WebSocket is nil")
		return
	}

	go tc.tcWebsocketConnect(wsData.Username, wsConn, clientType, wsData.ConsoleTeamMode, wsData.Subscriptions)
}

func (tc *TsConnector) tcChannelOTP(ctx *gin.Context) {
	otpTypeVal, _ := ctx.Get("otpType")
	otpType, _ := otpTypeVal.(string)
	if !strings.HasPrefix(otpType, "channel_") {
		_ = ctx.Error(errors.New("invalid OTP type"))
		return
	}

	data, _ := ctx.Get("otpData")
	wsData, ok := data.(*ChannelOTPData)
	if !ok || wsData == nil {
		_ = ctx.Error(errors.New("invalid OTP data"))
		return
	}

	exists := tc.teamserver.TsClientExists(wsData.Username)
	if !exists {
		respondError(ctx, http.StatusNetworkAuthenticationRequired, "Server error: client not connected")
		return
	}

	var wsUpgrader websocket.Upgrader
	wsUpgrader.CheckOrigin = func(r *http.Request) bool {
		return true
	}
	wsConn, err := wsUpgrader.Upgrade(ctx.Writer, ctx.Request, nil)
	if err != nil {
		tc.teamserver.TsLogAdd(adaptix.LogStatusError, 0, "server", "connector", "WebSocket upgrade error: %s", err.Error())
		return
	}

	if wsConn == nil {
		tc.teamserver.TsLogAdd(adaptix.LogStatusError, 0, "server", "connector", "WebSocket is nil")
		return
	}

	channelDataStr := string(wsData.ChannelData)

	switch otpType {
	case "channel_tunnel":
		if err := tc.teamserver.TsTunnelClientNewChannel(channelDataStr, wsConn, wsData.Username); err != nil {
			tc.teamserver.TsLogAdd(adaptix.LogStatusError, 0, "server", "connector", "Tunnel channel error: %s", err.Error())
			wsConn.Close()
		}

	case "channel_terminal":
		if err := tc.teamserver.TsAgentTerminalCreateChannel(channelDataStr, wsConn); err != nil {
			tc.teamserver.TsLogAdd(adaptix.LogStatusError, 0, "server", "connector", "Terminal channel error: %s", err.Error())
			wsConn.Close()
		}

	case "channel_agent_build":
		go func() {
			if err := tc.teamserver.TsAgentBuildCreateChannel(channelDataStr, wsConn, wsData.Username); err != nil {
				tc.teamserver.TsLogAdd(adaptix.LogStatusError, 0, "server", "connector", "Agent build channel error: %s", err.Error())
			}
		}()

	default:
		tc.teamserver.TsLogAdd(adaptix.LogStatusError, 0, "server", "connector", "Unknown channel type: %s", otpType)
		wsConn.Close()
	}
}

/// LOGS

func (tc *TsConnector) TcLogsList(ctx *gin.Context) {
	limit := 200
	offset := 0
	var beforeId int64 = 0
	sourceFilter := ctx.Query("source")
	categoryFilter := ctx.Query("category")
	contains := ctx.Query("q")

	if q := ctx.Query("limit"); q != "" {
		v, err := strconv.Atoi(q)
		if err != nil || v < 0 {
			respondError(ctx, http.StatusBadRequest, "limit must be a non-negative integer")
			return
		}
		if v > 2000 {
			v = 2000
		}
		limit = v
	}
	if q := ctx.Query("before_id"); q != "" {
		v, err := strconv.ParseInt(q, 10, 64)
		if err != nil || v < 0 {
			respondError(ctx, http.StatusBadRequest, "before_id must be a non-negative 64-bit integer")
			return
		}
		beforeId = v
	}
	if q := ctx.Query("offset"); q != "" {
		v, err := strconv.Atoi(q)
		if err != nil || v < 0 {
			respondError(ctx, http.StatusBadRequest, "offset must be a non-negative integer")
			return
		}
		offset = v
	}

	var (
		jsonData []byte
		err      error
	)
	if beforeId > 0 {
		jsonData, err = tc.teamserver.TsLogsGetPageBeforeIdFiltered(beforeId, limit, sourceFilter, categoryFilter, contains)
	} else {
		jsonData, err = tc.teamserver.TsLogsGetPageFiltered(offset, limit, sourceFilter, categoryFilter, contains)
	}
	if err != nil {
		respondError(ctx, http.StatusBadRequest, err.Error())
		return
	}

	ctx.Data(http.StatusOK, "application/json; charset=utf-8", jsonData)
}
