package connector

import (
	"errors"
	"github.com/gin-gonic/gin"
	"net/http"
	"os"
)

type AccessOTP struct {
	Type string `json:"type"`
	Id   string `json:"id"`
}

func (tc *TsConnector) tcOTP_Generate(ctx *gin.Context) {
	var (
		accessOTP AccessOTP
		answer    gin.H
	)

	err := ctx.ShouldBindJSON(&accessOTP)
	if err != nil {
		_ = ctx.Error(errors.New("invalid otp"))
		return
	}

	otp, err := tc.teamserver.CreateOTP(accessOTP.Type, accessOTP.Id)
	if err != nil {
		answer = gin.H{"message": err.Error(), "ok": false}
	} else {
		answer = gin.H{"message": otp, "ok": true}
	}

	ctx.JSON(http.StatusOK, answer)
}

func (tc *TsConnector) tcOTP_DownloadSync(ctx *gin.Context) {

	value, exists := ctx.Get("objectId")
	if !exists {
		ctx.String(http.StatusNotFound, "Server error: objectId not found in context")
		return
	}

	fileId, ok := value.(string)
	if !ok {
		ctx.String(http.StatusNotFound, "Server error: invalid fileId type in context")
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

	value, exists := ctx.Get("objectId")
	if !exists {
		ctx.String(http.StatusNotFound, "Server error: objectId not found in context")
		return
	}

	fileId, ok := value.(string)
	if !ok {
		ctx.String(http.StatusNotFound, "Server error: invalid fileId type in context")
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
