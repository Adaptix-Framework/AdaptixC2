package database

import (
	"AdaptixServer/core/utils/logs"
	"database/sql"
	"errors"
	"fmt"

	"github.com/Adaptix-Framework/axc2"
)

func (dbms *DBMS) DbAgentExist(agentId string) bool {
	rows, err := dbms.database.Query("SELECT Id FROM Agents WHERE Id = ?;", agentId)
	if err != nil {
		return false
	}
	defer func(rows *sql.Rows) {
		_ = rows.Close()
	}(rows)

	return rows.Next()
}

func (dbms *DBMS) DbAgentInsert(agentData adaptix.AgentData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	ok = dbms.DbAgentExist(agentData.Id)
	if ok {
		return fmt.Errorf("agent %s already exists", agentData.Id)
	}

	insertQuery := `INSERT INTO Agents (Id, Crc, Name, SessionKey, Listener, Async, ExternalIP, InternalIP, GmtOffset, 
                       Sleep, Jitter, Pid, Tid, Arch, Elevated, Process, Os, OsDesc, Domain, Computer, Username, Impersonated,
					   OemCP, ACP, CreateTime, LastTick, WorkingTime, KillDate, Tags, Mark, Color, TargetId
				   ) values(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?, ?);`
	_, err := dbms.database.Exec(insertQuery,
		agentData.Id, agentData.Crc, agentData.Name, agentData.SessionKey, agentData.Listener, agentData.Async, agentData.ExternalIP,
		agentData.InternalIP, agentData.GmtOffset, agentData.Sleep, agentData.Jitter, agentData.Pid, agentData.Tid, agentData.Arch,
		agentData.Elevated, agentData.Process, agentData.Os, agentData.OsDesc, agentData.Domain, agentData.Computer, agentData.Username,
		agentData.Impersonated, agentData.OemCP, agentData.ACP, agentData.CreateTime, agentData.LastTick, agentData.WorkingTime, agentData.KillDate, agentData.Tags, agentData.Mark,
		agentData.Color, agentData.TargetId,
	)
	return err
}

func (dbms *DBMS) DbAgentUpdate(agentData adaptix.AgentData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	ok = dbms.DbAgentExist(agentData.Id)
	if !ok {
		return fmt.Errorf("agent %s does not exist", agentData.Id)
	}

	updateQuery := `UPDATE Agents SET Sleep = ?, Jitter = ?, Impersonated = ?, WorkingTime = ?, KillDate = ?, Tags = ?, Mark = ?, Color = ? WHERE Id = ?;`
	_, err := dbms.database.Exec(updateQuery, agentData.Sleep, agentData.Jitter, agentData.Impersonated, agentData.WorkingTime, agentData.KillDate,
		agentData.Tags, agentData.Mark, agentData.Color, agentData.Id,
	)
	return err
}

func (dbms *DBMS) DbAgentDelete(agentId string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	ok = dbms.DbAgentExist(agentId)
	if !ok {
		return fmt.Errorf("agent %s does not exist", agentId)
	}

	deleteQuery := `DELETE FROM Agents WHERE Id = ?;`
	_, err := dbms.database.Exec(deleteQuery, agentId)

	return err
}

func (dbms *DBMS) DbAgentTick(agentData adaptix.AgentData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	ok = dbms.DbAgentExist(agentData.Id)
	if !ok {
		return fmt.Errorf("agent %s does not exist", agentData.Id)
	}

	updateQuery := `UPDATE Agents SET LastTick = ? WHERE Id = ?;`
	_, err := dbms.database.Exec(updateQuery, agentData.LastTick, agentData.Id)
	return err
}

func (dbms *DBMS) DbAgentAll() []adaptix.AgentData {
	var agents []adaptix.AgentData

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT Id, Crc, Name, SessionKey, Listener, Async, ExternalIP, InternalIP, GmtOffset, 
                       Sleep, Jitter, Pid, Tid, Arch, Elevated, Process, Os, OsDesc, Domain, Computer, Username, Impersonated,
					   OemCP, ACP, CreateTime, LastTick, WorkingTime, KillDate, Tags, Mark, Color, TargetId FROM Agents;`
		query, err := dbms.database.Query(selectQuery)
		if err == nil {

			for query.Next() {
				agentData := adaptix.AgentData{}
				err = query.Scan(&agentData.Id, &agentData.Crc, &agentData.Name, &agentData.SessionKey, &agentData.Listener,
					&agentData.Async, &agentData.ExternalIP, &agentData.InternalIP, &agentData.GmtOffset, &agentData.Sleep,
					&agentData.Jitter, &agentData.Pid, &agentData.Tid, &agentData.Arch, &agentData.Elevated, &agentData.Process,
					&agentData.Os, &agentData.OsDesc, &agentData.Domain, &agentData.Computer, &agentData.Username, &agentData.Impersonated,
					&agentData.OemCP, &agentData.ACP, &agentData.CreateTime, &agentData.LastTick, &agentData.WorkingTime, &agentData.KillDate,
					&agentData.Tags, &agentData.Mark, &agentData.Color, &agentData.TargetId,
				)
				if err != nil {
					continue
				}
				agents = append(agents, agentData)
			}
		} else {
			logs.Debug("", "Failed to query agents: "+err.Error())
		}

		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)
	}
	return agents
}
