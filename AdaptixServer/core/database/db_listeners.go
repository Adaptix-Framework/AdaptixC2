package database

import (
	"AdaptixServer/core/utils/logs"
	"database/sql"
	"errors"
	"fmt"
	"github.com/Adaptix-Framework/axc2"
)

func (dbms *DBMS) DbListenerExist(listenerName string) bool {
	rows, err := dbms.database.Query("SELECT ListenerName FROM Listeners;")
	if err != nil {
		return false
	}
	defer func(rows *sql.Rows) {
		_ = rows.Close()
	}(rows)

	for rows.Next() {
		rowListenerName := ""
		_ = rows.Scan(&rowListenerName)
		if listenerName == rowListenerName {
			return true
		}
	}
	return false
}

func (dbms *DBMS) DbListenerInsert(listenerData adaptix.ListenerData, customData []byte) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.DbListenerExist(listenerData.Name)
	if ok {
		return fmt.Errorf("listener %s alredy exists", listenerData.Name)
	}

	insertQuery := `INSERT INTO Listeners (ListenerName, ListenerRegName, ListenerConfig, Watermark, CustomData) values(?,?,?,?,?);`
	_, err := dbms.database.Exec(insertQuery, listenerData.Name, listenerData.RegName, listenerData.Data, listenerData.Watermark, customData)

	return err
}

func (dbms *DBMS) DbListenerDelete(listenerName string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.DbListenerExist(listenerName)
	if !ok {
		return fmt.Errorf("listener %s not exists", listenerName)
	}

	deleteQuery := `DELETE FROM Listeners WHERE ListenerName = ?;`
	_, err := dbms.database.Exec(deleteQuery, listenerName)

	return err
}

func (dbms *DBMS) DbListenerUpdate(listenerName string, listenerConfig string, customData []byte) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.DbListenerExist(listenerName)
	if !ok {
		return fmt.Errorf("listener %s not exists", listenerName)
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
	CustomData      []byte
}

func (dbms *DBMS) DbListenerAll() []ListenerRow {
	var listeners []ListenerRow

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT ListenerName, ListenerRegName, ListenerConfig, Watermark, CustomData FROM Listeners;`
		query, err := dbms.database.Query(selectQuery)
		if err == nil {

			for query.Next() {
				listenerRow := ListenerRow{}
				err = query.Scan(&listenerRow.ListenerName, &listenerRow.ListenerRegName, &listenerRow.ListenerConfig, &listenerRow.Watermark, &listenerRow.CustomData)
				if err != nil {
					continue
				}
				listeners = append(listeners, listenerRow)
			}
		} else {
			logs.Debug("", err.Error()+" --- Clear database file!")
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)
	}
	return listeners
}
