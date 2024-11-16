package database

import (
	"errors"
	"fmt"
)

func (dbms *DBMS) DbListenerExist(listenerName string) bool {
	rows, err := dbms.database.Query("SELECT ListenerName FROM Listeners;")
	if err != nil {
		return false
	}
	defer rows.Close()

	for rows.Next() {
		rowListenerName := ""
		_ = rows.Scan(&rowListenerName)
		if listenerName == rowListenerName {
			return true
		}
	}
	return false
}

func (dbms *DBMS) DbListenerInsert(listenerName string, listenerType string, listenerConfig string) error {
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

	insertQuery = `INSERT INTO Listeners (ListenerName, ListenerType, ListenerConfig) values(?,?,?);`
	_, err = dbms.database.Exec(insertQuery, listenerName, listenerType, listenerConfig)

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

func (dbms *DBMS) DbListenerUpdate(listenerName string, listenerConfig string) error {
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

	updateQuery = `UPDATE Listeners SET ListenerConfig = ? WHERE ListenerName = ?;`
	_, err = dbms.database.Exec(updateQuery, listenerName, listenerConfig)

	return err
}

type ListenerRow struct {
	ListenerName   string
	ListenerType   string
	ListenerConfig string
}

func (dbms *DBMS) DbListenerAll() []ListenerRow {
	var (
		listeners   []ListenerRow
		ok          bool
		selectQuery string
	)

	ok = dbms.DatabaseExists()
	if ok {
		selectQuery = `SELECT ListenerName, ListenerType, ListenerConfig FROM Listeners;`
		query, err := dbms.database.Query(selectQuery)
		if err == nil {

			for query.Next() {
				listenerRow := ListenerRow{}
				err = query.Scan(&listenerRow.ListenerName, &listenerRow.ListenerType, &listenerRow.ListenerConfig)
				if err != nil {
					continue
				}
				listeners = append(listeners, listenerRow)
			}
		}
		defer query.Close()
	}
	return listeners
}
