package server

import (
	"encoding/json"
	"fmt"
	"time"

	"AdaptixServer/core/database"

	"github.com/Adaptix-Framework/axc2/v2"
)

func (ts *Teamserver) TsChatSendMessage(username string, message string, replyToId int64, replyToName string) {
	date := time.Now().UTC().Unix()
	id, err := ts.DBMS.DbChatInsertEx(username, message, date, replyToId, replyToName)
	if err != nil {
		ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "chat", "%s", err.Error())
		return
	}
	packet := CreateSpChatMessageEx(database.ChatDataEx{
		Id:          id,
		Username:    username,
		Message:     message,
		Date:        date,
		Reactions:   "{}",
		ReplyToId:   replyToId,
		ReplyToName: replyToName,
	})
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryChatRealtime)
}

func (ts *Teamserver) TsChatEditMessage(username string, id int64, newMessage string) error {
	owner, active := ts.DBMS.DbChatGetOwner(id)
	if !active {
		return fmt.Errorf("message not found or deleted")
	}
	if owner != username {
		return fmt.Errorf("access denied")
	}
	if err := ts.DBMS.DbChatEdit(id, newMessage); err != nil {
		return err
	}
	packet := CreateSpChatEdit(id, newMessage)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryChatRealtime)
	return nil
}

func (ts *Teamserver) TsChatDeleteMessage(username string, id int64) error {
	owner, active := ts.DBMS.DbChatGetOwner(id)
	if !active {
		return fmt.Errorf("message not found or already deleted")
	}
	if owner != username {
		return fmt.Errorf("access denied")
	}
	if err := ts.DBMS.DbChatDelete(id); err != nil {
		return err
	}
	packet := CreateSpChatDelete(id)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryChatRealtime)
	return nil
}

func (ts *Teamserver) TsChatToggleReaction(username string, id int64, emoji string) error {
	raw := ts.DBMS.DbChatGetReactions(id)
	reactions := make(map[string][]string)
	if err := json.Unmarshal([]byte(raw), &reactions); err != nil {
		reactions = make(map[string][]string)
	}
	users := reactions[emoji]
	found := -1
	for i, u := range users {
		if u == username {
			found = i
			break
		}
	}
	if found >= 0 {
		reactions[emoji] = append(users[:found], users[found+1:]...)
		if len(reactions[emoji]) == 0 {
			delete(reactions, emoji)
		}
	} else {
		reactions[emoji] = append(users, username)
	}
	newJSON, _ := json.Marshal(reactions)
	if err := ts.DBMS.DbChatSetReactions(id, string(newJSON)); err != nil {
		return err
	}
	packet := CreateSpChatReaction(id, string(newJSON))
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryChatRealtime)
	return nil
}

func (ts *Teamserver) TsChatGetTodo() (string, string, int64) {
	return ts.DBMS.DbChatGetTodo()
}

func (ts *Teamserver) TsChatUpdateTodo(username string, content string) error {
	now := time.Now().UTC().Unix()
	if err := ts.DBMS.DbChatSetTodo(content, username, now); err != nil {
		return err
	}
	packet := CreateSpChatTodo(content, username, now)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryChatTodo)
	return nil
}

func (ts *Teamserver) TsChatHistory(limit int, beforeId int64) []byte {
	messages := ts.DBMS.DbChatRecent(limit, beforeId)
	data, _ := json.Marshal(messages)
	return data
}

func (ts *Teamserver) TsChatSearch(query string, limit int, beforeId int64) []byte {
	messages := ts.DBMS.DbChatSearch(query, limit, beforeId)
	data, _ := json.Marshal(messages)
	return data
}

func (ts *Teamserver) TsChatClear() error {
	return ts.DBMS.DbChatClear()
}

func (ts *Teamserver) TsChatCount() int {
	return ts.DBMS.DbChatCount()
}
