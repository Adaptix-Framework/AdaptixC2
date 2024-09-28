package database

import (
	"errors"
	"fmt"
)

func (dbms *DBMS) ListenerExist(listenerName string) bool {
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

func (dbms *DBMS) ListenerInsert(listenerName string, listenerType string, listenerConfig string) error {
	var (
		ok          bool
		err         error
		insertQuery string
	)

	ok = dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.ListenerExist(listenerName)
	if ok {
		return fmt.Errorf("listener %s alredy exists", listenerName)
	}

	insertQuery = `INSERT INTO Listeners (ListenerName, ListenerType, ListenerConfig) values(?,?,?);`
	_, err = dbms.database.Exec(insertQuery, listenerName, listenerType, listenerConfig)
	if err != nil {
		return err
	}
	return nil
}

func (dbms *DBMS) ListenerDelete(listenerName string) error {
	var (
		ok          bool
		err         error
		deleteQuery string
	)

	ok = dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.ListenerExist(listenerName)
	if !ok {
		return fmt.Errorf("listener %s not exists", listenerName)
	}

	deleteQuery = `DELETE FROM Listeners WHERE ListenerName = ?;`
	_, err = dbms.database.Exec(deleteQuery, listenerName)
	if err != nil {
		return err
	}
	return nil

}

func (dbms *DBMS) ListenerUpdate(listenerName string, listenerConfig string) error {
	var (
		ok          bool
		err         error
		updateQuery string
	)

	ok = dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.ListenerExist(listenerName)
	if !ok {
		return fmt.Errorf("listener %s not exists", listenerName)
	}

	updateQuery = `UPDATE Listeners SET ListenerConfig = ? WHERE ListenerName = ?;`
	_, err = dbms.database.Exec(updateQuery, listenerName, listenerConfig)
	if err != nil {
		return err
	}
	return nil
}
