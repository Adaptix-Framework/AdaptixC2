package database

import (
	"AdaptixServer/core/adaptix"
	"errors"
	"fmt"
)

func (dbms *DBMS) DbAgentExist(agentId string) bool {
	rows, err := dbms.database.Query("SELECT Id FROM Agents;")
	if err != nil {
		return false
	}
	defer rows.Close()

	for rows.Next() {
		rowAgentId := ""
		_ = rows.Scan(&rowAgentId)
		if agentId == rowAgentId {
			return true
		}
	}
	return false
}

func (dbms *DBMS) DbAgentInsert(agentData adaptix.AgentData) error {
	var (
		err         error
		ok          bool
		insertQuery string
	)

	ok = dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.DbAgentExist(agentData.Id)
	if ok {
		return fmt.Errorf("agent %s alredy exists", agentData.Id)
	}

	insertQuery = `INSERT INTO Agents (Id, Crc, Name, SessionKey, Listener, Async, ExternalIP, InternalIP, GmtOffset, 
                       Sleep, Jitter, Pid, Tid, Arch, Elevated, Process, Os, OsDesc, Domain, Computer, Username, OemCP, 
                       ACP, CreateTime, LastTick, Tags) values(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);`
	_, err = dbms.database.Exec(insertQuery,
		agentData.Id, agentData.Crc, agentData.Name, agentData.SessionKey, agentData.Listener, agentData.Async, agentData.ExternalIP,
		agentData.InternalIP, agentData.GmtOffset, agentData.Sleep, agentData.Jitter, agentData.Pid, agentData.Tid, agentData.Arch,
		agentData.Elevated, agentData.Process, agentData.Os, agentData.OsDesc, agentData.Domain, agentData.Computer, agentData.Username,
		agentData.OemCP, agentData.ACP, agentData.CreateTime, agentData.LastTick, agentData.Tags,
	)
	return err
}

func (dbms *DBMS) DbAgentUpdate(agentData adaptix.AgentData) error {
	var (
		err         error
		ok          bool
		updateQuery string
	)

	ok = dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.DbAgentExist(agentData.Id)
	if !ok {
		return fmt.Errorf("agent %s does not exists", agentData.Id)
	}

	updateQuery = `UPDATE Agents SET Sleep = ?, Jitter = ?, Elevated = ?, Username = ?, LastTick = ?, Tags = ? WHERE Id = ?;`
	_, err = dbms.database.Exec(updateQuery, agentData.Sleep, agentData.Jitter, agentData.Elevated, agentData.Username, agentData.LastTick, agentData.Tags, agentData.Id)
	return err
}

func (dbms *DBMS) DbAgentDelete(agentId string) error {
	var (
		ok          bool
		err         error
		deleteQuery string
	)

	ok = dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.DbAgentExist(agentId)
	if !ok {
		return fmt.Errorf("agent %s does not exists", agentId)
	}

	deleteQuery = `DELETE FROM Agents WHERE Id = ?;`
	_, err = dbms.database.Exec(deleteQuery, agentId)

	return err
}

func (dbms *DBMS) DbAgentAll() []adaptix.AgentData {
	var (
		agents      []adaptix.AgentData
		ok          bool
		selectQuery string
	)

	ok = dbms.DatabaseExists()
	if ok {
		selectQuery = `SELECT Id, Crc, Name, SessionKey, Listener, Async, ExternalIP, InternalIP, GmtOffset, 
                       Sleep, Jitter, Pid, Tid, Arch, Elevated, Process, Os, OsDesc, Domain, Computer, Username, OemCP, 
                       ACP, CreateTime, LastTick, Tags FROM Agents;`
		query, err := dbms.database.Query(selectQuery)
		if err == nil {

			for query.Next() {
				agentData := adaptix.AgentData{}
				err = query.Scan(&agentData.Id, &agentData.Crc, &agentData.Name, &agentData.SessionKey, &agentData.Listener,
					&agentData.Async, &agentData.ExternalIP, &agentData.InternalIP, &agentData.GmtOffset, &agentData.Sleep,
					&agentData.Jitter, &agentData.Pid, &agentData.Tid, &agentData.Arch, &agentData.Elevated, &agentData.Process,
					&agentData.Os, &agentData.OsDesc, &agentData.Domain, &agentData.Computer, &agentData.Username, &agentData.OemCP,
					&agentData.ACP, &agentData.CreateTime, &agentData.LastTick, &agentData.Tags,
				)
				if err != nil {
					continue
				}
				agents = append(agents, agentData)
			}
		}
		defer query.Close()
	}
	return agents
}
