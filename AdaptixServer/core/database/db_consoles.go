package database

import (
	"bytes"
	"database/sql"
	"encoding/json"
	"errors"
	"fmt"
)

func (dbms *DBMS) DbConsoleInsert(agentId string, packet interface{}) error {
	var (
		err         error
		ok          bool
		insertQuery string
		buffer      bytes.Buffer
	)

	_ = json.NewEncoder(&buffer).Encode(packet)

	ok = dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	insertQuery = `INSERT INTO Consoles (AgentId, Packet) values(?,?);`
	_, err = dbms.database.Exec(insertQuery, agentId, buffer.Bytes())
	return err
}

func (dbms *DBMS) DbConsoleDelete(agentId string) error {
	var (
		ok          bool
		err         error
		deleteQuery string
	)

	ok = dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	deleteQuery = `DELETE FROM Consoles WHERE AgentId = ?;`
	_, err = dbms.database.Exec(deleteQuery, agentId)

	return err
}

func (dbms *DBMS) DbConsoleAll(agentId string) [][]byte {
	var (
		consoles    [][]byte
		ok          bool
		selectQuery string
	)

	ok = dbms.DatabaseExists()
	if ok {
		selectQuery = `SELECT Packet FROM Consoles WHERE AgentId = ? ORDER BY Id;`
		query, err := dbms.database.Query(selectQuery, agentId)
		if err == nil {

			for query.Next() {
				var message []byte
				err = query.Scan(&message)
				if err != nil {
					continue
				}
				consoles = append(consoles, message)
			}
		} else {
			fmt.Println(err.Error() + " --- Clear database file!")
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)
	}
	return consoles
}
