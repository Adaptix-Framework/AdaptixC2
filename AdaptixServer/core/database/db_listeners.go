package database

import (
	"database/sql"
	"errors"
	"fmt"

	"github.com/Adaptix-Framework/axc2/v2"
)

func (dbms *DBMS) DbListenerExist(listenerName string) bool {
	var name string
	err := dbms.database.QueryRow("SELECT ListenerName FROM Listeners WHERE ListenerName = ? LIMIT 1;", listenerName).Scan(&name)
	return err == nil
}

func (dbms *DBMS) DbListenerInsert(listenerData adaptix.ListenerData, customData []byte) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	status := listenerData.Status
	if status == "" {
		status = "Listen"
	}

	insertQuery := `INSERT OR IGNORE INTO Listeners (ListenerName, ListenerRegName, ListenerConfig, CreateTime, Tags, Watermark, CustomData, ListenerStatus) values(?,?,?,?,?,?,?,?);`
	result, err := dbms.database.Exec(insertQuery, listenerData.Name, listenerData.RegName, listenerData.Data, listenerData.CreateTime, listenerData.Tags, listenerData.Watermark, customData, status)
	if err != nil {
		return err
	}
	rows, _ := result.RowsAffected()
	if rows == 0 {
		return fmt.Errorf("listener %s already exists", listenerData.Name)
	}
	return nil
}

func (dbms *DBMS) DbListenerDelete(listenerName string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	deleteQuery := `DELETE FROM Listeners WHERE ListenerName = ?;`
	result, err := dbms.database.Exec(deleteQuery, listenerName)
	if err != nil {
		return err
	}
	rows, _ := result.RowsAffected()
	if rows == 0 {
		return fmt.Errorf("listener %s does not exist", listenerName)
	}
	return nil
}

func (dbms *DBMS) DbListenerUpdate(listenerName string, listenerConfig string, customData []byte, tags string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	updateQuery := `UPDATE Listeners SET ListenerConfig = ?, CustomData = ?, Tags = ? WHERE ListenerName = ?;`
	result, err := dbms.database.Exec(updateQuery, listenerConfig, customData, tags, listenerName)
	if err != nil {
		return err
	}
	rows, _ := result.RowsAffected()
	if rows == 0 {
		return fmt.Errorf("listener %s does not exist", listenerName)
	}
	return nil
}

func (dbms *DBMS) DbListenerUpdateTags(listenerName string, tags string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	updateQuery := `UPDATE Listeners SET Tags = ? WHERE ListenerName = ?;`
	result, err := dbms.database.Exec(updateQuery, tags, listenerName)
	if err != nil {
		return err
	}
	rows, _ := result.RowsAffected()
	if rows == 0 {
		return fmt.Errorf("listener %s does not exist", listenerName)
	}
	return nil
}

func (dbms *DBMS) DbListenerUpdateStatus(listenerName string, status string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	updateQuery := `UPDATE Listeners SET ListenerStatus = ? WHERE ListenerName = ?;`
	result, err := dbms.database.Exec(updateQuery, status, listenerName)
	if err != nil {
		return err
	}
	rows, _ := result.RowsAffected()
	if rows == 0 {
		return fmt.Errorf("listener %s does not exist", listenerName)
	}
	return nil
}

type ListenerRow struct {
	ListenerName    string
	ListenerRegName string
	ListenerConfig  string
	ListenerStatus  string
	Watermark       string
	Tags            string
	CreateTime      int64
	CustomData      []byte
}

func (dbms *DBMS) DbListenerAll() []ListenerRow {
	var listeners []ListenerRow

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT ListenerName, ListenerRegName, ListenerConfig, ListenerStatus, CreateTime, Watermark, CustomData, COALESCE(Tags, '') FROM Listeners;`
		query, err := dbms.database.Query(selectQuery)
		if err != nil {
			dbms.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server", "database", "Failed to query listeners: %s", err.Error())
			return listeners
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)

		for query.Next() {
			listenerRow := ListenerRow{}
			err = query.Scan(&listenerRow.ListenerName, &listenerRow.ListenerRegName, &listenerRow.ListenerConfig, &listenerRow.ListenerStatus, &listenerRow.CreateTime, &listenerRow.Watermark, &listenerRow.CustomData, &listenerRow.Tags)
			if err != nil {
				continue
			}
			if listenerRow.ListenerStatus == "" {
				listenerRow.ListenerStatus = "Listen"
			}
			listeners = append(listeners, listenerRow)
		}
	}
	return listeners
}
