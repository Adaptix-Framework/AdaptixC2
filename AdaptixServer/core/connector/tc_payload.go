package connector

import (
	"encoding/base64"
	"net/http"
	"strconv"

	"github.com/gin-gonic/gin"
)

func (tc *TsConnector) TcPayloadList(ctx *gin.Context) {
	showHidden := false
	if raw := ctx.Query("show_hidden"); raw != "" {
		if v, err := strconv.ParseBool(raw); err == nil {
			showHidden = v
		} else if raw == "1" {
			showHidden = true
		}
	}

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

	jsonData, err := tc.teamserver.TsPayloadGetPage(offset, limit, showHidden, filterExpr, sortCol, sortOrder)
	if err != nil {
		respondError(ctx, http.StatusBadRequest, err.Error())
		return
	}
	ctx.Data(http.StatusOK, "application/json; charset=utf-8", jsonData)
}

type PayloadIdReq struct {
	PayloadId int64 `json:"payload_id"`
}

func (tc *TsConnector) TcPayloadGet(ctx *gin.Context) {
	var req PayloadIdReq
	if err := ctx.ShouldBindJSON(&req); err != nil || req.PayloadId == 0 {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	p, err := tc.teamserver.TsPayloadGet(req.PayloadId)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}
	p.LocalPath = ""
	ctx.JSON(http.StatusOK, gin.H{"ok": true, "payload": p})
}

func (tc *TsConnector) TcPayloadDownload(ctx *gin.Context) {
	var req PayloadIdReq
	if err := ctx.ShouldBindJSON(&req); err != nil || req.PayloadId == 0 {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}

	filename, content, err := tc.teamserver.TsPayloadDownload(req.PayloadId)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}
	ctx.JSON(http.StatusOK, gin.H{
		"ok":       true,
		"filename": filename,
		"content":  base64.StdEncoding.EncodeToString(content),
	})
}

type PayloadHideReq struct {
	Ids    []int64 `json:"id_array"`
	Hidden bool    `json:"hidden"`
}

func (tc *TsConnector) TcPayloadHide(ctx *gin.Context) {
	var req PayloadHideReq
	if err := ctx.ShouldBindJSON(&req); err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}
	if err := tc.teamserver.TsPayloadHide(req.Ids, req.Hidden); err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}
	respondOK(ctx)
}

type PayloadUpdateReq struct {
	PayloadId int64  `json:"payload_id"`
	Name      string `json:"name"`
	Notes     string `json:"notes"` // description
	Artifact  string `json:"artifact"`
	Arch      string `json:"arch"`
	Hidden    bool   `json:"hidden"`
}

func (tc *TsConnector) TcPayloadUpdate(ctx *gin.Context) {
	var req PayloadUpdateReq
	if err := ctx.ShouldBindJSON(&req); err != nil || req.PayloadId == 0 {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}
	p, err := tc.teamserver.TsPayloadUpdateMeta(req.PayloadId, req.Name, req.Notes, req.Artifact, req.Arch, req.Hidden)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}
	p.LocalPath = ""
	p.ConfigJson = ""
	ctx.JSON(http.StatusOK, gin.H{"ok": true, "payload": p})
}

type PayloadColorReq struct {
	Ids        []int64 `json:"id_array"`
	Background string  `json:"background"`
	Foreground string  `json:"foreground"`
	Reset      bool    `json:"reset"`
}

func (tc *TsConnector) TcPayloadSetColor(ctx *gin.Context) {
	var req PayloadColorReq
	if err := ctx.ShouldBindJSON(&req); err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}
	if err := tc.teamserver.TsPayloadSetColor(req.Ids, req.Background, req.Foreground, req.Reset); err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}
	respondOK(ctx)
}

type PayloadRemoveReq struct {
	Ids  []int64 `json:"id_array"`
	Hard bool    `json:"hard"`
}

func (tc *TsConnector) TcPayloadRemove(ctx *gin.Context) {
	var req PayloadRemoveReq
	if err := ctx.ShouldBindJSON(&req); err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}
	if err := tc.teamserver.TsPayloadRemove(req.Ids, req.Hard); err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}
	respondOK(ctx)
}

func (tc *TsConnector) TcPayloadSync(ctx *gin.Context) {
	jsonData, err := tc.teamserver.TsPayloadGetPage(0, 100000, true, "", "Created", "desc")
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}
	ctx.Data(http.StatusOK, "application/json; charset=utf-8", jsonData)
}

type PayloadImportReq struct {
	Name      string   `json:"name"`
	AgentType string   `json:"agent_type"`
	Artifact  string   `json:"artifact"`
	Arch      string   `json:"arch"`
	Listeners []string `json:"listeners"`
	Content   string   `json:"content"` // base64
	Config    string   `json:"config"`
}

func (tc *TsConnector) TcPayloadImport(ctx *gin.Context) {
	var req PayloadImportReq
	if err := ctx.ShouldBindJSON(&req); err != nil {
		respondError(ctx, http.StatusOK, "invalid JSON data")
		return
	}
	if req.Content == "" {
		respondError(ctx, http.StatusOK, "content required")
		return
	}
	raw, err := base64.StdEncoding.DecodeString(req.Content)
	if err != nil {
		raw, err = base64.RawStdEncoding.DecodeString(req.Content)
		if err != nil {
			respondError(ctx, http.StatusOK, "invalid base64 content")
			return
		}
	}
	if req.AgentType == "" {
		req.AgentType = "imported"
	}
	if req.Name == "" {
		req.Name = "imported"
	}
	creator, ok := tc.extractUserContext(ctx)
	if !ok {
		return
	}
	p, err := tc.teamserver.TsPayloadImport(req.Name, req.AgentType, req.Artifact, req.Arch, creator, req.Listeners, raw, req.Config)
	if err != nil {
		respondError(ctx, http.StatusOK, err.Error())
		return
	}
	p.LocalPath = ""
	ctx.JSON(http.StatusOK, gin.H{"ok": true, "payload": p})
}
