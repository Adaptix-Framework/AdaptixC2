package database

import (
	"database/sql"
	"errors"
	"fmt"
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

func (dbms *DBMS) DbListenerInsert(listenerName string, listenerType string, listenerConfig string, customData []byte) error {
	var (
		ok          bool
		err         error
		insertQuery string
	)

	ok = dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.DbListenerExist(listenerName)
	if ok {
		return fmt.Errorf("listener %s alredy exists", listenerName)
	}

	insertQuery = `INSERT INTO Listeners (ListenerName, ListenerType, ListenerConfig, CustomData) values(?,?,?,?);`
	_, err = dbms.database.Exec(insertQuery, listenerName, listenerType, listenerConfig, customData)

	return err
}

func (dbms *DBMS) DbListenerDelete(listenerName string) error {
	var (
		ok          bool
		err         error
		deleteQuery string
	)

	ok = dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.DbListenerExist(listenerName)
	if !ok {
		return fmt.Errorf("listener %s not exists", listenerName)
	}

	deleteQuery = `DELETE FROM Listeners WHERE ListenerName = ?;`
	_, err = dbms.database.Exec(deleteQuery, listenerName)

	return err
}

func (dbms *DBMS) DbListenerUpdate(listenerName string, listenerConfig string, customData []byte) error {
	var (
		ok          bool
		err         error
		updateQuery string
	)

	ok = dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.DbListenerExist(listenerName)
	if !ok {
		return fmt.Errorf("listener %s not exists", listenerName)
	}

	updateQuery = `UPDATE Listeners SET ListenerConfig = ?, CustomData = ? WHERE ListenerName = ?;`
	_, err = dbms.database.Exec(updateQuery, listenerConfig, customData, listenerName)

	return err
}

type ListenerRow struct {
	ListenerName   string
	ListenerType   string
	ListenerConfig string
	CustomData     []byte
}

func (dbms *DBMS) DbListenerAll() []ListenerRow {
	var (
		listeners   []ListenerRow
		ok          bool
		selectQuery string
	)

	ok = dbms.DatabaseExists()
	if ok {
		selectQuery = `SELECT ListenerName, ListenerType, ListenerConfig, CustomData FROM Listeners;`
		query, err := dbms.database.Query(selectQuery)
		if err == nil {

			for query.Next() {
				listenerRow := ListenerRow{}
				err = query.Scan(&listenerRow.ListenerName, &listenerRow.ListenerType, &listenerRow.ListenerConfig, &listenerRow.CustomData)
				if err != nil {
					continue
				}
				listeners = append(listeners, listenerRow)
			}
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)
	}
	return listeners
}
