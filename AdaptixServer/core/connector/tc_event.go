package connector

import (
	"net/http"
	"strconv"
	"strings"

	"github.com/gin-gonic/gin"
)

type EventHandlerIdData struct {
	ID string `json:"id"`
}

type EventMuteData struct {
	Event string `json:"event"`
}

// TcEventHandlersList — GET /events/handlers?offset=&limit=&q=&event=&source=&group=&enabled=
func (tc *TsConnector) TcEventHandlersList(ctx *gin.Context) {
	offset := 0
	limit := 50
	if raw := ctx.Query("offset"); raw != "" {
		v, err := strconv.Atoi(raw)
		if err != nil || v < 0 {
			respondError(ctx, http.StatusBadRequest, "offset must be a non-negative integer")
			return
		}
		offset = v
	}
	if raw := ctx.Query("limit"); raw != "" {
		v, err := strconv.Atoi(raw)
		if err != nil || v < 0 {
			respondError(ctx, http.StatusBadRequest, "limit must be a non-negative integer")
			return
		}
		if v > 1000 {
			v = 1000
		}
		if v == 0 {
			v = 50
		}
		limit = v
	}

	q := ctx.Query("q")
	event := ctx.Query("event")
	source := ctx.Query("source")
	group := ctx.Query("group")
	enabled := ctx.Query("enabled")

	jsonData, err := tc.teamserver.TsEventHandlersGetPage(offset, limit, q, event, source, group, enabled)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}
	ctx.Data(http.StatusOK, "application/json; charset=utf-8", jsonData)
}

func (tc *TsConnector) TcEventHandlersListPost(ctx *gin.Context) {
	jsonHandlers, err := tc.teamserver.TsEventHandlersList()
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}
	respondJSONItems(ctx, jsonHandlers)
}

// Body: { id?, name, group?, description?, event, script, enabled?, filters? }
func (tc *TsConnector) TcEventHandlerRegister(ctx *gin.Context) {
	raw, err := ctx.GetRawData()
	if err != nil || len(raw) == 0 {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}
	operator, _ := mustUsername(ctx)
	jsonHandler, err := tc.teamserver.TsEventHandlerRegister(string(raw), operator)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}
	respondJSONRawOK(ctx, jsonHandler)
}

func (tc *TsConnector) TcEventHandlerGet(ctx *gin.Context) {
	var data EventHandlerIdData
	if err := ctx.ShouldBindJSON(&data); err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}
	if data.ID == "" {
		respondError(ctx, http.StatusOK, "id is required")
		return
	}
	jsonHandler, err := tc.teamserver.TsEventHandlerGet(data.ID)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}
	respondJSONRawOK(ctx, jsonHandler)
}

func (tc *TsConnector) TcEventHandlerEnable(ctx *gin.Context) {
	var data EventHandlerIdData
	if err := ctx.ShouldBindJSON(&data); err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}
	if data.ID == "" {
		respondError(ctx, http.StatusOK, "id is required")
		return
	}
	if err := tc.teamserver.TsEventHandlerEnable(data.ID); err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}
	respondOK(ctx)
}

func (tc *TsConnector) TcEventHandlerDisable(ctx *gin.Context) {
	var data EventHandlerIdData
	if err := ctx.ShouldBindJSON(&data); err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}
	if data.ID == "" {
		respondError(ctx, http.StatusOK, "id is required")
		return
	}
	if err := tc.teamserver.TsEventHandlerDisable(data.ID); err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}
	respondOK(ctx)
}

func (tc *TsConnector) TcEventHandlerRemove(ctx *gin.Context) {
	var data EventHandlerIdData
	if err := ctx.ShouldBindJSON(&data); err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}
	if data.ID == "" {
		respondError(ctx, http.StatusOK, "id is required")
		return
	}
	if err := tc.teamserver.TsEventHandlerRemove(data.ID); err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}
	respondOK(ctx)
}

func (tc *TsConnector) TcEventMute(ctx *gin.Context) {
	var data EventMuteData
	if err := ctx.ShouldBindJSON(&data); err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}
	if data.Event == "" {
		respondError(ctx, http.StatusOK, "event is required")
		return
	}
	if err := tc.teamserver.TsEventMute(data.Event); err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}
	respondOK(ctx)
}

func (tc *TsConnector) TcEventUnmute(ctx *gin.Context) {
	var data EventMuteData
	if err := ctx.ShouldBindJSON(&data); err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}
	if data.Event == "" {
		respondError(ctx, http.StatusOK, "event is required")
		return
	}
	if err := tc.teamserver.TsEventUnmute(data.Event); err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}
	respondOK(ctx)
}

func (tc *TsConnector) TcEventTypesList(ctx *gin.Context) {
	raw, err := tc.teamserver.TsEventTypesList()
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}
	ctx.Data(http.StatusOK, "application/json; charset=utf-8", []byte(raw))
}

type EventEmitReq struct {
	Event  string `json:"event"`
	Source string `json:"source,omitempty"`
	Text   string `json:"text"`
}

func (tc *TsConnector) TcEventEmit(ctx *gin.Context) {
	var req EventEmitReq
	if err := ctx.ShouldBindJSON(&req); err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}
	source := strings.TrimSpace(req.Source)
	if source == "" {
		if u, ok := tc.extractUserContext(ctx); ok {
			source = "operator:" + u
		} else {
			source = "api"
		}
	}
	if err := tc.teamserver.TsEventEmitFrom(req.Event, source, req.Text); err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}
	respondOK(ctx)
}

func (tc *TsConnector) TcEventMutesList(ctx *gin.Context) {
	jsonMutes, err := tc.teamserver.TsEventMutesList()
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}
	respondJSONItems(ctx, jsonMutes)
}
