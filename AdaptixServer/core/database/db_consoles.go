package database

import (
	"AdaptixServer/core/utils/logs"
	"database/sql"
	"encoding/json"
	"errors"
)

func (dbms *DBMS) DbConsoleInsert(agentId string, packet interface{}) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	data, err := json.Marshal(packet)
	if err != nil {
		return err
	}

	if dbms.stmtConsoleInsert != nil {
		_, err = dbms.stmtConsoleInsert.Exec(agentId, data)
		return err
	}
	_, err = dbms.database.Exec(`INSERT INTO Consoles (AgentId, Packet) values(?,?);`, agentId, data)
	return err
}

func (dbms *DBMS) DbConsoleDelete(agentId string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	deleteQuery := `DELETE FROM Consoles WHERE AgentId = ?;`
	_, err := dbms.database.Exec(deleteQuery, agentId)

	return err
}

func (dbms *DBMS) DbConsoleAll(agentId string) [][]byte {
	var consoles [][]byte

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT Packet FROM Consoles WHERE AgentId = ? ORDER BY Id;`
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
			logs.Debug("", "Failed to query consoles: "+err.Error())
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)
	}
	return consoles
}

func (dbms *DBMS) DbConsoleLimited(agentId string, limit int) [][]byte {
	var consoles [][]byte

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT Packet FROM Consoles WHERE AgentId = ? ORDER BY Id DESC LIMIT ?;`
		query, err := dbms.database.Query(selectQuery, agentId, limit)
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
			logs.Debug("", "Failed to query consoles: "+err.Error())
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)
	}
	for i, j := 0, len(consoles)-1; i < j; i, j = i+1, j-1 {
		consoles[i], consoles[j] = consoles[j], consoles[i]
	}
	return consoles
}

func (dbms *DBMS) DbConsoleCount(agentId string) int {
	ok := dbms.DatabaseExists()
	if !ok {
		return 0
	}

	var count int
	selectQuery := `SELECT COUNT(*) FROM Consoles WHERE AgentId = ?;`
	err := dbms.database.QueryRow(selectQuery, agentId).Scan(&count)
	if err != nil {
		return 0
	}
	return count
}
