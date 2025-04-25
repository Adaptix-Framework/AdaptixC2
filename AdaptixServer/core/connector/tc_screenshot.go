package connector

import (
	"fmt"
	"github.com/gin-gonic/gin"
	"net/http"
)

type ScreenRemove struct {
	ScreenIdArray []string `json:"screen_id_array"`
}

func (tc *TsConnector) TcScreenshotRemove(ctx *gin.Context) {
	var screenRemove ScreenRemove
	err := ctx.ShouldBindJSON(&screenRemove)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid command data", "ok": false})
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
		ctx.JSON(http.StatusOK, gin.H{"message": "invalid command data", "ok": false})
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
