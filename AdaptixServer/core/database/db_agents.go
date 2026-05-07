package database

import (
	"AdaptixServer/core/utils/logs"
	"database/sql"
	"errors"
	"fmt"

	"github.com/Adaptix-Framework/axc2"
)

func (dbms *DBMS) DbAgentExist(agentId string) bool {
	var id string
	err := dbms.database.QueryRow("SELECT Id FROM Agents WHERE Id = ? LIMIT 1;", agentId).Scan(&id)
	return err == nil
}

func (dbms *DBMS) DbAgentInsert(agentData adaptix.AgentData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	insertQuery := `INSERT OR IGNORE INTO Agents (Id, Crc, Name, SessionKey, Listener, Async, ExternalIP, InternalIP, GmtOffset, 
                       Sleep, Jitter, Pid, Tid, Arch, Elevated, Process, Os, OsDesc, Domain, Computer, Username, Impersonated,
					   OemCP, ACP, CreateTime, LastTick, WorkingTime, KillDate, Tags, Mark, Color, TargetId, CustomData
				   ) values(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);`
	result, err := dbms.database.Exec(insertQuery,
		agentData.Id, agentData.Crc, agentData.Name, agentData.SessionKey, agentData.Listener, agentData.Async, agentData.ExternalIP,
		agentData.InternalIP, agentData.GmtOffset, agentData.Sleep, agentData.Jitter, agentData.Pid, agentData.Tid, agentData.Arch,
		agentData.Elevated, agentData.Process, agentData.Os, agentData.OsDesc, agentData.Domain, agentData.Computer, agentData.Username,
		agentData.Impersonated, agentData.OemCP, agentData.ACP, agentData.CreateTime, agentData.LastTick, agentData.WorkingTime, agentData.KillDate, agentData.Tags, agentData.Mark,
		agentData.Color, agentData.TargetId, agentData.CustomData,
	)
	if err != nil {
		return err
	}
	rows, _ := result.RowsAffected()
	if rows == 0 {
		return fmt.Errorf("agent %s already exists", agentData.Id)
	}
	return nil
}

func (dbms *DBMS) DbAgentUpdate(agentData adaptix.AgentData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	updateQuery := `UPDATE Agents SET Sleep = ?, Jitter = ?, Impersonated = ?, WorkingTime = ?, KillDate = ?, Tags = ?, Mark = ?, Color = ?, CustomData = ? WHERE Id = ?;`
	result, err := dbms.database.Exec(updateQuery, agentData.Sleep, agentData.Jitter, agentData.Impersonated, agentData.WorkingTime, agentData.KillDate,
		agentData.Tags, agentData.Mark, agentData.Color, agentData.CustomData, agentData.Id,
	)
	if err != nil {
		return err
	}
	rows, _ := result.RowsAffected()
	if rows == 0 {
		return fmt.Errorf("agent %s does not exist", agentData.Id)
	}
	return nil
}

func (dbms *DBMS) DbAgentDelete(agentId string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	deleteQuery := `DELETE FROM Agents WHERE Id = ?;`
	result, err := dbms.database.Exec(deleteQuery, agentId)
	if err != nil {
		return err
	}
	rows, _ := result.RowsAffected()
	if rows == 0 {
		return fmt.Errorf("agent %s does not exist", agentId)
	}
	return nil
}

func (dbms *DBMS) DbAgentTick(agentData adaptix.AgentData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	if dbms.stmtAgentTick != nil {
		_, err := dbms.stmtAgentTick.Exec(agentData.LastTick, agentData.Id)
		return err
	}
	_, err := dbms.database.Exec(`UPDATE Agents SET LastTick = ? WHERE Id = ?;`, agentData.LastTick, agentData.Id)
	return err
}

func (dbms *DBMS) DbAgentAll() []adaptix.AgentData {
	var agents []adaptix.AgentData

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT Id, Crc, Name, SessionKey, Listener, Async, ExternalIP, InternalIP, GmtOffset, 
                       Sleep, Jitter, Pid, Tid, Arch, Elevated, Process, Os, OsDesc, Domain, Computer, Username, Impersonated,
					   OemCP, ACP, CreateTime, LastTick, WorkingTime, KillDate, Tags, Mark, Color, TargetId, CustomData FROM Agents;`
		query, err := dbms.database.Query(selectQuery)
		if err != nil {
			logs.Debug("", "Failed to query agents: "+err.Error())
			return agents
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)

		for query.Next() {
			agentData := adaptix.AgentData{}
			err = query.Scan(&agentData.Id, &agentData.Crc, &agentData.Name, &agentData.SessionKey, &agentData.Listener,
				&agentData.Async, &agentData.ExternalIP, &agentData.InternalIP, &agentData.GmtOffset, &agentData.Sleep,
				&agentData.Jitter, &agentData.Pid, &agentData.Tid, &agentData.Arch, &agentData.Elevated, &agentData.Process,
				&agentData.Os, &agentData.OsDesc, &agentData.Domain, &agentData.Computer, &agentData.Username, &agentData.Impersonated,
				&agentData.OemCP, &agentData.ACP, &agentData.CreateTime, &agentData.LastTick, &agentData.WorkingTime, &agentData.KillDate,
				&agentData.Tags, &agentData.Mark, &agentData.Color, &agentData.TargetId, &agentData.CustomData,
			)
			if err != nil {
				continue
			}
			agents = append(agents, agentData)
		}
	}
	return agents
}
