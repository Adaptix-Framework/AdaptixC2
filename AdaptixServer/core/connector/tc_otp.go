package connector

import (
	"encoding/json"
	"errors"
	"net/http"
	"os"

	"github.com/gin-gonic/gin"
)

type AccessOTP struct {
	Type string          `json:"type"`
	Data json.RawMessage `json:"data,omitempty"`
}

type ConnectOTPData struct {
	Username        string
	Version         string
	ClientType      uint8    `json:"client_type,omitempty"`
	ConsoleTeamMode bool     `json:"console_team_mode,omitempty"`
	Subscriptions   []string `json:"subscriptions,omitempty"`
}

type ChannelOTPData struct {
	Username    string
	ChannelData json.RawMessage
}

type FileOTPData struct {
	Id string `json:"id"`
}

func (tc *TsConnector) validateOTPMiddleware() gin.HandlerFunc {
	return func(ctx *gin.Context) {
		otp := ctx.Query("otp")
		if otp == "" {
			_ = ctx.Error(errors.New("authorization token required"))
			return
		}

		otpType, data, ok := tc.teamserver.ValidateOTP(otp)
		if !ok {
			_ = ctx.Error(errors.New("invalid or expired OTP"))
			return
		}

		ctx.Set("otpType", otpType)
		ctx.Set("otpData", data)
		ctx.Next()
	}
}

func (tc *TsConnector) tcOTP_Generate(ctx *gin.Context) {
	var (
		accessOTP AccessOTP
		data      interface{}
	)

	err := ctx.ShouldBindJSON(&accessOTP)
	if err != nil {
		_ = ctx.Error(errors.New("invalid request body"))
		return
	}
	if len(accessOTP.Data) == 0 {
		ctx.JSON(http.StatusBadRequest, gin.H{"message": "data is required", "ok": false})
		return
	}

	switch accessOTP.Type {

	case "connect":
		usernameStr, version, ok := tc.extractUserContext(ctx)
		if !ok {
			return
		}

		var connectData ConnectOTPData
		err := json.Unmarshal(accessOTP.Data, &connectData)
		if err != nil {
			ctx.JSON(http.StatusBadRequest, gin.H{"message": "invalid connect data: " + err.Error(), "ok": false})
			return
		}

		connectData.Username = usernameStr
		connectData.Version = version
		data = connectData

	case "channel_tunnel", "channel_terminal", "channel_agent_build":
		usernameStr, _, ok := tc.extractUserContext(ctx)
		if !ok {
			return
		}

		data = &ChannelOTPData{
			Username:    usernameStr,
			ChannelData: accessOTP.Data,
		}

	case "download", "tmp_upload":
		var fileData FileOTPData
		err := json.Unmarshal(accessOTP.Data, &fileData)
		if err != nil {
			ctx.JSON(http.StatusBadRequest, gin.H{"message": "invalid file data: " + err.Error(), "ok": false})
			return
		}
		data = fileData.Id

	default:
		ctx.JSON(http.StatusBadRequest, gin.H{"message": "unknown OTP type", "ok": false})
		return
	}

	otp, err := tc.teamserver.CreateOTP(accessOTP.Type, data)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": otp, "ok": true})
}

func (tc *TsConnector) extractUserContext(ctx *gin.Context) (string, string, bool) {
	username, exists := ctx.Get("username")
	if !exists {
		ctx.JSON(http.StatusOK, gin.H{"message": "username not found in context", "ok": false})
		return "", "", false
	}
	usernameStr, ok := username.(string)
	if !ok || usernameStr == "" {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid username in context", "ok": false})
		return "", "", false
	}

	versionValue, _ := ctx.Get("version")
	version, _ := versionValue.(string)

	return usernameStr, version, true
}

func (tc *TsConnector) tcOTP_DownloadSync(ctx *gin.Context) {
	otpType, _ := ctx.Get("otpType")
	if otpType != "download" {
		_ = ctx.Error(errors.New("invalid OTP type"))
		return
	}

	data, _ := ctx.Get("otpData")
	fileId, ok := data.(string)
	if !ok || fileId == "" {
		_ = ctx.Error(errors.New("invalid OTP data"))
		return
	}

	path, err := tc.teamserver.TsDownloadGetFilepath(fileId)
	if err != nil {
		ctx.String(http.StatusNotFound, err.Error())
		return
	}

	file, err := os.Open(path)
	if err != nil {
		ctx.String(http.StatusNotFound, "File not found")
		return
	}
	defer func(file *os.File) {
		_ = file.Close()
	}(file)

	fileInfo, err := file.Stat()
	if err != nil {
		ctx.String(http.StatusInternalServerError, "Cannot get file info")
		return
	}

	ctx.Header("Content-Disposition", "attachment; filename="+fileInfo.Name())
	ctx.Header("Content-Type", "application/octet-stream")
	ctx.Header("Content-Length", string(rune(fileInfo.Size())))

	ctx.File(path)
}

func (tc *TsConnector) tcOTP_UploadTemp(ctx *gin.Context) {
	otpType, _ := ctx.Get("otpType")
	if otpType != "tmp_upload" {
		_ = ctx.Error(errors.New("invalid OTP type"))
		return
	}

	data, _ := ctx.Get("otpData")
	fileId, ok := data.(string)
	if !ok || fileId == "" {
		_ = ctx.Error(errors.New("invalid OTP data"))
		return
	}

	savePath, err := tc.teamserver.TsUploadGetFilepath(fileId)
	if err != nil {
		ctx.String(http.StatusNotFound, err.Error())
		return
	}

	ctx.Request.Body = http.MaxBytesReader(ctx.Writer, ctx.Request.Body, 2<<30)
	file, err := ctx.FormFile("file")
	if err != nil {
		ctx.String(http.StatusNotFound, err.Error())
		return
	}

	err = ctx.SaveUploadedFile(file, savePath)
	if err != nil {
		ctx.String(http.StatusNotFound, err.Error())
		return
	}

	ctx.String(http.StatusOK, "")
}
