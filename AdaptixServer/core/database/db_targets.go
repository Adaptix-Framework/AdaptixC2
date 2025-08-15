package database

import (
	"AdaptixServer/core/utils/logs"
	"database/sql"
	"errors"
	"fmt"
	adaptix "github.com/Adaptix-Framework/axc2"
)

func (dbms *DBMS) DbTargetExist(targetId string) bool {
	rows, err := dbms.database.Query("SELECT TargetId FROM Targets;")
	if err != nil {
		return false
	}
	defer func(rows *sql.Rows) {
		_ = rows.Close()
	}(rows)

	for rows.Next() {
		rowTargetId := ""
		_ = rows.Scan(&rowTargetId)
		if targetId == rowTargetId {
			return true
		}
	}
	return false
}

func (dbms *DBMS) DbTargetsAdd(targetsData []*adaptix.TargetData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	for _, targetData := range targetsData {
		ok = dbms.DbTargetExist(targetData.TargetId)
		if ok {
			logs.Error("", "target %s alredy exists", targetData.TargetId)
			continue
		}

		insertQuery := `INSERT INTO Targets (TargetId, Computer, Domain, Address, Os, OsDesk, Tag, Info, Date, Alive, Owned) values(?,?,?,?,?,?,?,?,?,?,?);`
		_, err := dbms.database.Exec(insertQuery, targetData.TargetId, targetData.Computer, targetData.Domain, targetData.Address, targetData.Os,
			targetData.OsDesk, targetData.Tag, targetData.Info, targetData.Date, targetData.Alive, targetData.Owned)
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
		return errors.New("database not exists")
	}

	ok = dbms.DbTargetExist(targetData.TargetId)
	if !ok {
		return fmt.Errorf("target %s not exists", targetData.TargetId)
	}

	updateQuery := `UPDATE Targets SET Computer = ?, Domain = ?, Address = ?, Os = ?, OsDesk = ?, Tag = ?, Info = ?, Alive = ?, Owned = ? WHERE TargetId = ?;`
	_, err := dbms.database.Exec(updateQuery, targetData.Computer, targetData.Domain, targetData.Address, targetData.Os,
		targetData.OsDesk, targetData.Tag, targetData.Info, targetData.Alive, targetData.Owned, targetData.TargetId)
	return err
}

func (dbms *DBMS) DbTargetDelete(targetId string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.DbTargetExist(targetId)
	if !ok {
		return fmt.Errorf("target %s not exists", targetId)
	}

	deleteQuery := `DELETE FROM Targets WHERE TargetId = ?;`
	_, err := dbms.database.Exec(deleteQuery, targetId)
	return err
}

func (dbms *DBMS) DbTargetsAll() []*adaptix.TargetData {
	var targets []*adaptix.TargetData

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT TargetId, Computer, Domain, Address, Os, OsDesk, Tag, Info, Date, Alive, Owned FROM Targets;`
		query, err := dbms.database.Query(selectQuery)
		if err == nil {
			for query.Next() {
				targetData := &adaptix.TargetData{}
				err = query.Scan(&targetData.TargetId, &targetData.Computer, &targetData.Domain, &targetData.Address, &targetData.Os,
					&targetData.OsDesk, &targetData.Tag, &targetData.Info, &targetData.Date, &targetData.Alive, &targetData.Owned)
				if err != nil {
					continue
				}
				targets = append(targets, targetData)
			}
		} else {
			logs.Debug("", err.Error()+" --- Clear database file!")
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)
	}
	return targets
}
