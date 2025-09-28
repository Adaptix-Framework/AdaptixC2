package database

import (
	"AdaptixServer/core/utils/logs"
	"database/sql"
	"errors"
	"fmt"
	"strings"

	adaptix "github.com/Adaptix-Framework/axc2"
)

func (dbms *DBMS) DbTargetExist(targetId string) bool {
	rows, err := dbms.database.Query("SELECT TargetId FROM Targets WHERE TargetId = ?;", targetId)
	if err != nil {
		return false
	}
	defer func(rows *sql.Rows) {
		_ = rows.Close()
	}(rows)

	return rows.Next()
}

func (dbms *DBMS) DbTargetsAdd(targetsData []*adaptix.TargetData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	for _, targetData := range targetsData {
		ok = dbms.DbTargetExist(targetData.TargetId)
		if ok {
			logs.Error("", "target %s already exists", targetData.TargetId)
			continue
		}

		insertQuery := `INSERT INTO Targets (TargetId, Computer, Domain, Address, Os, OsDesk, Tag, Info, Date, Alive, Agents) values(?,?,?,?,?,?,?,?,?,?,?);`
		_, err := dbms.database.Exec(insertQuery, targetData.TargetId, targetData.Computer, targetData.Domain, targetData.Address, targetData.Os,
			targetData.OsDesk, targetData.Tag, targetData.Info, targetData.Date, targetData.Alive, strings.Join(targetData.Agents, ","))
		if err != nil {
			logs.Error("", err.Error())
			continue
		}
	}

	return nil
}

func (dbms *DBMS) DbTargetUpdate(targetData *adaptix.TargetData) error {

	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	ok = dbms.DbTargetExist(targetData.TargetId)
	if !ok {
		return fmt.Errorf("target %s does not exist", targetData.TargetId)
	}

	updateQuery := `UPDATE Targets SET Computer = ?, Domain = ?, Address = ?, Os = ?, OsDesk = ?, Tag = ?, Info = ?, Alive = ?, Agents = ? WHERE TargetId = ?;`
	_, err := dbms.database.Exec(updateQuery, targetData.Computer, targetData.Domain, targetData.Address, targetData.Os,
		targetData.OsDesk, targetData.Tag, targetData.Info, targetData.Alive, strings.Join(targetData.Agents, ","), targetData.TargetId)
	return err
}

func (dbms *DBMS) DbTargetDelete(targetId string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	ok = dbms.DbTargetExist(targetId)
	if !ok {
		return fmt.Errorf("target %s does not exist", targetId)
	}

	deleteQuery := `DELETE FROM Targets WHERE TargetId = ?;`
	_, err := dbms.database.Exec(deleteQuery, targetId)
	return err
}

func (dbms *DBMS) DbTargetsAll() []*adaptix.TargetData {
	var targets []*adaptix.TargetData

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT TargetId, Computer, Domain, Address, Os, OsDesk, Tag, Info, Date, Alive, Agents FROM Targets;`
		query, err := dbms.database.Query(selectQuery)
		if err == nil {
			for query.Next() {
				targetData := &adaptix.TargetData{}
				agetns_str := ""
				err = query.Scan(&targetData.TargetId, &targetData.Computer, &targetData.Domain, &targetData.Address, &targetData.Os,
					&targetData.OsDesk, &targetData.Tag, &targetData.Info, &targetData.Date, &targetData.Alive, &agetns_str)
				if err != nil {
					continue
				}
				if agetns_str != "" {
					targetData.Agents = strings.Split(agetns_str, ",")
				}

				targets = append(targets, targetData)
			}
		} else {
			logs.Debug("", "Failed to query targets: "+err.Error())
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)
	}
	return targets
}
