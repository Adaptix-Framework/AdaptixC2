package server

import (
	"AdaptixServer/core/utils/logs"
	"time"

	adaptix "github.com/Adaptix-Framework/axc2"
)

func (ts *Teamserver) TsChatSendMessage(username string, message string) {
	chatData := adaptix.ChatData{
		Username: username,
		Message:  message,
		Date:     time.Now().UTC().Unix(),
	}
	ts.messages.Put(chatData)
	err := ts.DBMS.DbChatInsert(chatData)
	if err != nil {
		logs.Error("", err.Error())
	}
	packet := CreateSpChatMessage(chatData)
	ts.TsSyncAllClients(packet)
}
