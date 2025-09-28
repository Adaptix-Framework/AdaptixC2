package connector

import (
	"errors"
	"net/http"

	"github.com/gin-gonic/gin"
)

type ChatMessage struct {
	Message string `json:"message"`
}

func (tc *TsConnector) TcChatSendMessage(ctx *gin.Context) {
	var chat_message ChatMessage

	err := ctx.ShouldBindJSON(&chat_message)
	if err != nil {
		_ = ctx.Error(errors.New("invalid message"))
		return
	}

	username := ctx.GetString("username")

	tc.teamserver.TsChatSendMessage(username, chat_message.Message)

	answer := gin.H{"ok": true, "message": ""}
	ctx.JSON(http.StatusOK, answer)
}
