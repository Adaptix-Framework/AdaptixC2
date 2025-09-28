package database

import (
	"AdaptixServer/core/utils/logs"
	"database/sql"
	"errors"

	adaptix "github.com/Adaptix-Framework/axc2"
)

func (dbms *DBMS) DbChatInsert(chatData adaptix.ChatData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	insertQuery := `INSERT INTO Chat (Username, Message, Date) values(?,?,?);`
	_, err := dbms.database.Exec(insertQuery, chatData.Username, chatData.Message, chatData.Date)
	return err
}

func (dbms *DBMS) DbChatAll() []adaptix.ChatData {
	var messages []adaptix.ChatData

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT Username, Message, Date FROM Chat ORDER BY Id;`
		query, err := dbms.database.Query(selectQuery)
		if err == nil {

			for query.Next() {
				chatData := adaptix.ChatData{}
				err = query.Scan(&chatData.Username, &chatData.Message, &chatData.Date)
				if err != nil {
					continue
				}
				messages = append(messages, chatData)
			}
		} else {
			logs.Debug("", err.Error()+" --- Clear database file!")
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)
	}
	return messages
}
