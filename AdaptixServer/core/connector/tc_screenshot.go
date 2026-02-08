package connector

import (
	"fmt"
	"net/http"

	"github.com/gin-gonic/gin"
)

func (tc *TsConnector) TcScreenshotList(ctx *gin.Context) {
	jsonScreen, err := tc.teamserver.TsScreenshotList()
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.Data(http.StatusOK, "application/json; charset=utf-8", []byte(jsonScreen))
}

func (tc *TsConnector) TcScreenshotGetImage(ctx *gin.Context) {
	screenId := ctx.Query("screen_id")
	if screenId == "" {
		ctx.JSON(http.StatusBadRequest, gin.H{"message": "screen_id is required", "ok": false})
		return
	}

	content, err := tc.teamserver.TsScreenshotGetImage(screenId)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.Data(http.StatusOK, "image/png", content)
}

type ScreenRemove struct {
	ScreenIdArray []string `json:"screen_id_array"`
}

func (tc *TsConnector) TcScreenshotRemove(ctx *gin.Context) {
	var screenRemove ScreenRemove
	err := ctx.ShouldBindJSON(&screenRemove)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
		return
	}

	var errorsSlice []string
	for _, screenId := range screenRemove.ScreenIdArray {
		err = tc.teamserver.TsScreenshotDelete(screenId)
		if err != nil {
			errorsSlice = append(errorsSlice, err.Error())
		}
	}

	if len(errorsSlice) > 0 {
		message := ""
		for i, errorMessage := range errorsSlice {
			message += fmt.Sprintf("%d. %s\n", i+1, errorMessage)
		}

		ctx.JSON(http.StatusOK, gin.H{"message": message, "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}

type ScreenNote struct {
	ScreenIdArray []string `json:"screen_id_array"`
	Note          string   `json:"note"`
}

func (tc *TsConnector) TcScreenshotSetNote(ctx *gin.Context) {
	var (
		screenNote ScreenNote
		err        error
	)

	err = ctx.ShouldBindJSON(&screenNote)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid JSON data", "ok": false})
		return
	}

	var errorsSlice []string
	for _, screenId := range screenNote.ScreenIdArray {
		err = tc.teamserver.TsScreenshotNote(screenId, screenNote.Note)
		if err != nil {
			errorsSlice = append(errorsSlice, err.Error())
		}
	}

	if len(errorsSlice) > 0 {
		message := ""
		for i, errorMessage := range errorsSlice {
			message += fmt.Sprintf("%d. %s\n", i+1, errorMessage)
		}

		ctx.JSON(http.StatusOK, gin.H{"message": message, "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "", "ok": true})
}
