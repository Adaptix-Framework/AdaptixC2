package database

import (
	"database/sql"
	"encoding/json"
	"errors"

	"github.com/Adaptix-Framework/axc2/v2"
)

func (dbms *DBMS) DbGroupCreate(parentId int64, name string, scope string) (int64, error) {
	ok := dbms.DatabaseExists()
	if !ok {
		return 0, errors.New("database does not exist")
	}

	result, err := dbms.database.Exec(`INSERT INTO Groups (ParentGroupId, GroupName, Scope, Members) VALUES (?, ?, ?, '[]');`, parentId, name, scope)
	if err != nil {
		return 0, err
	}

	groupId, err2 := result.LastInsertId()
	if err2 != nil {
		return 0, err2
	}
	return groupId, nil
}

func (dbms *DBMS) DbGroupRename(groupId int64, name string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	_, err := dbms.database.Exec(`UPDATE Groups SET GroupName = ? WHERE GroupId = ?;`, name, groupId)
	return err
}

func (dbms *DBMS) DbGroupDelete(groupId int64) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	_, err := dbms.database.Exec(`DELETE FROM Groups WHERE GroupId = ?;`, groupId)
	return err
}

func (dbms *DBMS) DbGroupSetMembers(groupId int64, members []int64) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	membersJSON, err := json.Marshal(members)
	if err != nil {
		return err
	}

	_, err = dbms.database.Exec(`UPDATE Groups SET Members = ? WHERE GroupId = ?;`, string(membersJSON), groupId)
	return err
}

func (dbms *DBMS) DbGroupReparent(groupId int64, newParentId int64) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}
	_, err := dbms.database.Exec(`UPDATE Groups SET ParentGroupId = ? WHERE GroupId = ?;`, newParentId, groupId)
	return err
}

func (dbms *DBMS) DbGroupGetAll(scope string) []adaptix.GroupData {
	var groups []adaptix.GroupData

	ok := dbms.DatabaseExists()
	if !ok {
		return groups
	}

	var query *sql.Rows
	var err error

	if scope == "" {
		query, err = dbms.database.Query(`SELECT GroupId, ParentGroupId, GroupName, Scope, Members FROM Groups;`)
	} else {
		query, err = dbms.database.Query(`SELECT GroupId, ParentGroupId, GroupName, Scope, Members FROM Groups WHERE Scope = ?;`, scope)
	}
	if err != nil {
		return groups
	}
	defer func(query *sql.Rows) {
		_ = query.Close()
	}(query)

	for query.Next() {
		var g adaptix.GroupData
		var membersStr string
		err = query.Scan(&g.GroupId, &g.ParentGroupId, &g.GroupName, &g.Scope, &membersStr)
		if err != nil {
			continue
		}
		if membersStr != "" {
			_ = json.Unmarshal([]byte(membersStr), &g.Members)
		}
		if g.Members == nil {
			g.Members = []int64{}
		}
		groups = append(groups, g)
	}
	return groups
}

func (dbms *DBMS) DbGroupGetById(groupId int64) (adaptix.GroupData, error) {
	ok := dbms.DatabaseExists()
	if !ok {
		return adaptix.GroupData{}, errors.New("database does not exist")
	}

	var g adaptix.GroupData
	var membersStr string
	err := dbms.database.QueryRow(`SELECT GroupId, ParentGroupId, GroupName, Scope, Members FROM Groups WHERE GroupId = ?;`, groupId).Scan(&g.GroupId, &g.ParentGroupId, &g.GroupName, &g.Scope, &membersStr)
	if err != nil {
		return adaptix.GroupData{}, err
	}
	if membersStr != "" {
		_ = json.Unmarshal([]byte(membersStr), &g.Members)
	}
	if g.Members == nil {
		g.Members = []int64{}
	}
	return g, nil
}
