package database

import (
	"database/sql"
	"errors"
	"time"
)

type ChatDataEx struct {
	Id          int64
	Username    string
	Message     string
	Date        int64
	Edited      bool
	Deleted     bool
	DeletedDate int64
	Reactions   string
	ReplyToId   int64
	ReplyToName string
}

func (dbms *DBMS) DbChatInsertEx(username string, message string, date int64, replyToId int64, replyToName string) (int64, error) {
	if !dbms.DatabaseExists() {
		return 0, errors.New("database not exists")
	}
	result, err := dbms.database.Exec(
		`INSERT INTO Chat (Username, Message, Date, ReplyToId, ReplyToName) VALUES(?,?,?,?,?);`,
		username, message, date, replyToId, replyToName,
	)
	if err != nil {
		return 0, err
	}
	return result.LastInsertId()
}

func (dbms *DBMS) DbChatRecent(limit int, beforeId int64) []ChatDataEx {
	var messages []ChatDataEx
	if !dbms.DatabaseExists() {
		return messages
	}

	var rows *sql.Rows
	var err error
	if beforeId > 0 {
		rows, err = dbms.database.Query(
			`SELECT Id, Username, Message, Date, Edited, Deleted, DeletedDate, Reactions, ReplyToId, ReplyToName
			 FROM Chat WHERE Id < ? ORDER BY Id DESC LIMIT ?`,
			beforeId, limit,
		)
	} else {
		rows, err = dbms.database.Query(
			`SELECT Id, Username, Message, Date, Edited, Deleted, DeletedDate, Reactions, ReplyToId, ReplyToName
			 FROM Chat ORDER BY Id DESC LIMIT ?`,
			limit,
		)
	}
	if err != nil {
		dbms.ts.TsLogAdd(0, 0, "server", "database", "%s", err.Error())
		return messages
	}
	defer func() { _ = rows.Close() }()

	for rows.Next() {
		var m ChatDataEx
		if err := rows.Scan(&m.Id, &m.Username, &m.Message, &m.Date, &m.Edited, &m.Deleted, &m.DeletedDate, &m.Reactions, &m.ReplyToId, &m.ReplyToName); err != nil {
			continue
		}
		messages = append(messages, m)
	}
	for i, j := 0, len(messages)-1; i < j; i, j = i+1, j-1 {
		messages[i], messages[j] = messages[j], messages[i]
	}
	return messages
}

func (dbms *DBMS) DbChatSearch(query string, limit int, beforeId int64) []ChatDataEx {
	var messages []ChatDataEx
	if !dbms.DatabaseExists() {
		return messages
	}

	pattern := "%" + query + "%"
	var rows *sql.Rows
	var err error
	if beforeId > 0 {
		rows, err = dbms.database.Query(
			`SELECT Id, Username, Message, Date, Edited, Deleted, DeletedDate, Reactions, ReplyToId, ReplyToName
			 FROM Chat WHERE Message LIKE ? AND Id < ? ORDER BY Id DESC LIMIT ?`,
			pattern, beforeId, limit,
		)
	} else {
		rows, err = dbms.database.Query(
			`SELECT Id, Username, Message, Date, Edited, Deleted, DeletedDate, Reactions, ReplyToId, ReplyToName
			 FROM Chat WHERE Message LIKE ? ORDER BY Id DESC LIMIT ?`,
			pattern, limit,
		)
	}
	if err != nil {
		dbms.ts.TsLogAdd(0, 0, "server", "database", "%s", err.Error())
		return messages
	}
	defer func() { _ = rows.Close() }()

	for rows.Next() {
		var m ChatDataEx
		if err := rows.Scan(&m.Id, &m.Username, &m.Message, &m.Date, &m.Edited, &m.Deleted, &m.DeletedDate, &m.Reactions, &m.ReplyToId, &m.ReplyToName); err != nil {
			continue
		}
		messages = append(messages, m)
	}
	for i, j := 0, len(messages)-1; i < j; i, j = i+1, j-1 {
		messages[i], messages[j] = messages[j], messages[i]
	}
	return messages
}

func (dbms *DBMS) DbChatEdit(id int64, newMessage string) error {
	if !dbms.DatabaseExists() {
		return errors.New("database not exists")
	}
	_, err := dbms.database.Exec(`UPDATE Chat SET Message = ?, Edited = TRUE WHERE Id = ?`, newMessage, id)
	return err
}

func (dbms *DBMS) DbChatDelete(id int64) error {
	if !dbms.DatabaseExists() {
		return errors.New("database not exists")
	}
	now := time.Now().UTC().Unix()
	_, err := dbms.database.Exec(`UPDATE Chat SET Deleted = TRUE, DeletedDate = ? WHERE Id = ?`, now, id)
	return err
}

func (dbms *DBMS) DbChatGetOwner(id int64) (string, bool) {
	if !dbms.DatabaseExists() {
		return "", false
	}
	var username string
	var deleted bool
	err := dbms.database.QueryRow(`SELECT Username, Deleted FROM Chat WHERE Id = ?`, id).Scan(&username, &deleted)
	if err != nil {
		return "", false
	}
	return username, !deleted
}

func (dbms *DBMS) DbChatGetReactions(id int64) string {
	if !dbms.DatabaseExists() {
		return "{}"
	}
	var reactions string
	err := dbms.database.QueryRow(`SELECT Reactions FROM Chat WHERE Id = ?`, id).Scan(&reactions)
	if err != nil {
		return "{}"
	}
	return reactions
}

func (dbms *DBMS) DbChatSetReactions(id int64, reactions string) error {
	if !dbms.DatabaseExists() {
		return errors.New("database not exists")
	}
	_, err := dbms.database.Exec(`UPDATE Chat SET Reactions = ? WHERE Id = ?`, reactions, id)
	return err
}

func (dbms *DBMS) DbChatGetTodo() (string, string, int64) {
	if !dbms.DatabaseExists() {
		return "", "", 0
	}
	var content, updatedBy string
	var updatedAt int64
	err := dbms.database.QueryRow(`SELECT Content, UpdatedBy, UpdatedAt FROM ChatTodo WHERE Id = 1`).Scan(&content, &updatedBy, &updatedAt)
	if err != nil {
		return "", "", 0
	}
	return content, updatedBy, updatedAt
}

func (dbms *DBMS) DbChatSetTodo(content string, updatedBy string, updatedAt int64) error {
	if !dbms.DatabaseExists() {
		return errors.New("database not exists")
	}
	_, err := dbms.database.Exec(`UPDATE ChatTodo SET Content = ?, UpdatedBy = ?, UpdatedAt = ? WHERE Id = 1`, content, updatedBy, updatedAt)
	return err
}

func (dbms *DBMS) DbChatClear() error {
	if !dbms.DatabaseExists() {
		return errors.New("database not exists")
	}
	_, err := dbms.database.Exec(`DELETE FROM Chat`)
	return err
}

func (dbms *DBMS) DbChatCount() int {
	if !dbms.DatabaseExists() {
		return 0
	}
	var count int
	_ = dbms.database.QueryRow(`SELECT COUNT(*) FROM Chat`).Scan(&count)
	return count
}
