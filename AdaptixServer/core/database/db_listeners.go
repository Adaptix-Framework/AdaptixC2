package database

import (
	"AdaptixServer/core/utils/logs"
	"database/sql"
	"errors"
	"fmt"

	"github.com/Adaptix-Framework/axc2"
)

func (dbms *DBMS) DbListenerExist(listenerName string) bool {
	rows, err := dbms.database.Query("SELECT ListenerName FROM Listeners WHERE ListenerName = ?;", listenerName)
	if err != nil {
		return false
	}
	defer func(rows *sql.Rows) {
		_ = rows.Close()
	}(rows)

	return rows.Next()
}

func (dbms *DBMS) DbListenerInsert(listenerData adaptix.ListenerData, customData []byte) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	ok = dbms.DbListenerExist(listenerData.Name)
	if ok {
		return fmt.Errorf("listener %s already exists", listenerData.Name)
	}

	insertQuery := `INSERT INTO Listeners (ListenerName, ListenerRegName, ListenerConfig, CreateTime, Watermark, CustomData) values(?,?,?,?,?,?);`
	_, err := dbms.database.Exec(insertQuery, listenerData.Name, listenerData.RegName, listenerData.Data, listenerData.CreateTime, listenerData.Watermark, customData)

	return err
}

func (dbms *DBMS) DbListenerDelete(listenerName string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	ok = dbms.DbListenerExist(listenerName)
	if !ok {
		return fmt.Errorf("listener %s does not exist", listenerName)
	}

	deleteQuery := `DELETE FROM Listeners WHERE ListenerName = ?;`
	_, err := dbms.database.Exec(deleteQuery, listenerName)

	return err
}

func (dbms *DBMS) DbListenerUpdate(listenerName string, listenerConfig string, customData []byte) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	ok = dbms.DbListenerExist(listenerName)
	if !ok {
		return fmt.Errorf("listener %s does not exist", listenerName)
	}

	updateQuery := `UPDATE Listeners SET ListenerConfig = ?, CustomData = ? WHERE ListenerName = ?;`
	_, err := dbms.database.Exec(updateQuery, listenerConfig, customData, listenerName)

	return err
}

type ListenerRow struct {
	ListenerName    string
	ListenerRegName string
	ListenerConfig  string
	Watermark       string
	CreateTime      int64
	CustomData      []byte
}

func (dbms *DBMS) DbListenerAll() []ListenerRow {
	var listeners []ListenerRow

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT ListenerName, ListenerRegName, ListenerConfig, CreateTime, Watermark, CustomData FROM Listeners;`
		query, err := dbms.database.Query(selectQuery)
		if err != nil {
			logs.Debug("", "Failed to query listeners: "+err.Error())
			return listeners
		}
		defer query.Close()

		for query.Next() {
			listenerRow := ListenerRow{}
			err = query.Scan(&listenerRow.ListenerName, &listenerRow.ListenerRegName, &listenerRow.ListenerConfig, &listenerRow.CreateTime, &listenerRow.Watermark, &listenerRow.CustomData)
			if err != nil {
				continue
			}
			listeners = append(listeners, listenerRow)
		}
	}
	return listeners
}
