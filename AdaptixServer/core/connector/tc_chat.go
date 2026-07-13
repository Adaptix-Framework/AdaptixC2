package connector

import (
	"encoding/json"
	"errors"
	"net/http"
	"strconv"

	"github.com/gin-gonic/gin"
)

type ChatMessage struct {
	Message     string `json:"message"`
	ReplyToId   int64  `json:"reply_to_id"`
	ReplyToName string `json:"reply_to_name"`
}

type ChatEditRequest struct {
	Message string `json:"message"`
}

type ChatReactionRequest struct {
	Emoji string `json:"emoji"`
}

type ChatTodoRequest struct {
	Content string `json:"content"`
}

func (tc *TsConnector) TcChatSendMessage(ctx *gin.Context) {
	var req ChatMessage
	if err := ctx.ShouldBindJSON(&req); err != nil {
		_ = ctx.Error(errors.New("invalid message"))
		return
	}
	username := ctx.GetString("username")
	tc.teamserver.TsChatSendMessage(username, req.Message, req.ReplyToId, req.ReplyToName)
	ctx.JSON(http.StatusOK, gin.H{"ok": true, "message": ""})
}

func (tc *TsConnector) TcChatEditMessage(ctx *gin.Context) {
	idStr := ctx.Param("id")
	id, err := strconv.ParseInt(idStr, 10, 64)
	if err != nil {
		_ = ctx.Error(errors.New("invalid id"))
		return
	}
	var req ChatEditRequest
	if err := ctx.ShouldBindJSON(&req); err != nil {
		_ = ctx.Error(errors.New("invalid request"))
		return
	}
	username := ctx.GetString("username")
	if err := tc.teamserver.TsChatEditMessage(username, id, req.Message); err != nil {
		_ = ctx.Error(err)
		return
	}
	ctx.JSON(http.StatusOK, gin.H{"ok": true, "message": ""})
}

func (tc *TsConnector) TcChatDeleteMessage(ctx *gin.Context) {
	idStr := ctx.Param("id")
	id, err := strconv.ParseInt(idStr, 10, 64)
	if err != nil {
		_ = ctx.Error(errors.New("invalid id"))
		return
	}
	username := ctx.GetString("username")
	if err := tc.teamserver.TsChatDeleteMessage(username, id); err != nil {
		_ = ctx.Error(err)
		return
	}
	ctx.JSON(http.StatusOK, gin.H{"ok": true, "message": ""})
}

func (tc *TsConnector) TcChatReaction(ctx *gin.Context) {
	idStr := ctx.Param("id")
	id, err := strconv.ParseInt(idStr, 10, 64)
	if err != nil {
		_ = ctx.Error(errors.New("invalid id"))
		return
	}
	var req ChatReactionRequest
	if err := ctx.ShouldBindJSON(&req); err != nil {
		_ = ctx.Error(errors.New("invalid request"))
		return
	}
	username := ctx.GetString("username")
	if err := tc.teamserver.TsChatToggleReaction(username, id, req.Emoji); err != nil {
		_ = ctx.Error(err)
		return
	}
	ctx.JSON(http.StatusOK, gin.H{"ok": true, "message": ""})
}

func (tc *TsConnector) TcChatGetTodo(ctx *gin.Context) {
	content, updatedBy, updatedAt := tc.teamserver.TsChatGetTodo()
	ctx.JSON(http.StatusOK, gin.H{
		"ok":         true,
		"content":    content,
		"updated_by": updatedBy,
		"updated_at": updatedAt,
	})
}

func (tc *TsConnector) TcChatUpdateTodo(ctx *gin.Context) {
	var req ChatTodoRequest
	if err := ctx.ShouldBindJSON(&req); err != nil {
		_ = ctx.Error(errors.New("invalid request"))
		return
	}
	username := ctx.GetString("username")
	if err := tc.teamserver.TsChatUpdateTodo(username, req.Content); err != nil {
		_ = ctx.Error(err)
		return
	}
	ctx.JSON(http.StatusOK, gin.H{"ok": true, "message": ""})
}

func (tc *TsConnector) TcChatHistory(ctx *gin.Context) {
	beforeIdStr := ctx.DefaultQuery("before_id", "0")
	limitStr := ctx.DefaultQuery("limit", "50")

	beforeId, _ := strconv.ParseInt(beforeIdStr, 10, 64)
	limit, _ := strconv.Atoi(limitStr)
	if limit <= 0 || limit > 200 {
		limit = 50
	}

	data := tc.teamserver.TsChatHistory(int(limit), beforeId)
	var messages []interface{}
	_ = json.Unmarshal(data, &messages)
	total := tc.teamserver.TsChatCount()
	ctx.JSON(http.StatusOK, gin.H{"ok": true, "messages": messages, "total": total})
}

func (tc *TsConnector) TcChatSearch(ctx *gin.Context) {
	query := ctx.DefaultQuery("q", "")
	if query == "" {
		ctx.JSON(http.StatusOK, gin.H{"ok": true, "messages": []interface{}{}})
		return
	}
	beforeIdStr := ctx.DefaultQuery("before_id", "0")
	limitStr := ctx.DefaultQuery("limit", "50")

	beforeId, _ := strconv.ParseInt(beforeIdStr, 10, 64)
	limit, _ := strconv.Atoi(limitStr)
	if limit <= 0 || limit > 200 {
		limit = 50
	}

	data := tc.teamserver.TsChatSearch(query, int(limit), beforeId)
	var messages []interface{}
	_ = json.Unmarshal(data, &messages)
	ctx.JSON(http.StatusOK, gin.H{"ok": true, "messages": messages})
}

func (tc *TsConnector) TcChatClear(ctx *gin.Context) {
	if err := tc.teamserver.TsChatClear(); err != nil {
		_ = ctx.Error(err)
		return
	}
	ctx.JSON(http.StatusOK, gin.H{"ok": true, "message": ""})
}
