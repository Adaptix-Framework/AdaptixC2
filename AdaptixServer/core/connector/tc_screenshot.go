package connector

import (
	"fmt"
	"net/http"
	"strconv"

	"github.com/gin-gonic/gin"
)

func (tc *TsConnector) TcScreenshotList(ctx *gin.Context) {
	limit := 100
	offset := 0
	filterExpr := ctx.Query("q")
	sortCol := ctx.Query("sort")
	sortOrder := ctx.Query("order")

	if raw := ctx.Query("limit"); raw != "" {
		v, err := strconv.Atoi(raw)
		if err != nil || v < 0 {
			respondError(ctx, http.StatusBadRequest, "limit must be a non-negative integer")
			return
		}
		if v > 1000 {
			v = 1000
		}
		limit = v
	}

	if raw := ctx.Query("offset"); raw != "" {
		v, err := strconv.Atoi(raw)
		if err != nil || v < 0 {
			respondError(ctx, http.StatusBadRequest, "offset must be a non-negative integer")
			return
		}
		offset = v
	}

	jsonData, err := tc.teamserver.TsScreenshotsGetPage(offset, limit, filterExpr, sortCol, sortOrder)
	if err != nil {
		respondError(ctx, http.StatusBadRequest, err.Error())
		return
	}

	ctx.Data(http.StatusOK, "application/json; charset=utf-8", jsonData)
}

func (tc *TsConnector) TcScreenshotGetImage(ctx *gin.Context) {
	screenId, err := strconv.ParseInt(ctx.Query("screen_id"), 10, 64)
	if err != nil || screenId <= 0 {
		respondError(ctx, http.StatusBadRequest, "screen_id must be a positive integer")
		return
	}

	content, err := tc.teamserver.TsScreenshotGetImage(screenId)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}

	ctx.Data(http.StatusOK, "image/png", content)
}

type ScreenRemove struct {
	ScreenIdArray []int64 `json:"screen_id_array"`
}

func (tc *TsConnector) TcScreenshotRemove(ctx *gin.Context) {
	var screenRemove ScreenRemove
	err := ctx.ShouldBindJSON(&screenRemove)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
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

		respondError(ctx, http.StatusOK, message)
		return
	}

	respondOK(ctx)
}

type ScreenNote struct {
	ScreenIdArray []int64 `json:"screen_id_array"`
	Note          string  `json:"note"`
}

func (tc *TsConnector) TcScreenshotSetNote(ctx *gin.Context) {
	var (
		screenNote ScreenNote
		err        error
	)

	err = ctx.ShouldBindJSON(&screenNote)
	if err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
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

		respondError(ctx, http.StatusOK, message)
		return
	}

	respondOK(ctx)
}
