package database

import (
	"AdaptixServer/core/utils/logs"
	"database/sql"
	"errors"
	"fmt"

	"github.com/Adaptix-Framework/axc2"
)

func (dbms *DBMS) DbPivotExist(pivotId string) bool {
	rows, err := dbms.database.Query("SELECT PivotId FROM Pivots WHERE PivotId = ?;", pivotId)
	if err != nil {
		return false
	}
	defer func(rows *sql.Rows) {
		_ = rows.Close()
	}(rows)

	return rows.Next()
}

func (dbms *DBMS) DbPivotInsert(pivotData adaptix.PivotData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	ok = dbms.DbPivotExist(pivotData.PivotId)
	if ok {
		return fmt.Errorf("pivot %s already exists", pivotData.PivotId)
	}

	insertQuery := `INSERT INTO Pivots (PivotId, PivotName, ParentAgentId, ChildAgentId) values(?,?,?,?);`
	_, err := dbms.database.Exec(insertQuery, pivotData.PivotId, pivotData.PivotName, pivotData.ParentAgentId, pivotData.ChildAgentId)

	return err
}

func (dbms *DBMS) DbPivotDelete(pivotId string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	ok = dbms.DbPivotExist(pivotId)
	if !ok {
		return fmt.Errorf("pivot %s does not exist", pivotId)
	}

	deleteQuery := `DELETE FROM Pivots WHERE PivotId = ?;`
	_, err := dbms.database.Exec(deleteQuery, pivotId)

	return err
}

func (dbms *DBMS) DbPivotAll() []*adaptix.PivotData {
	var pivots []*adaptix.PivotData

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT PivotId, PivotName, ParentAgentId, ChildAgentId FROM Pivots;`
		query, err := dbms.database.Query(selectQuery)
		if err != nil {
			logs.Debug("", "Failed to query pivots: "+err.Error())
			return pivots
		}
		defer query.Close()

		for query.Next() {
			pivotData := &adaptix.PivotData{}
			err = query.Scan(&pivotData.PivotId, &pivotData.PivotName, &pivotData.ParentAgentId, &pivotData.ChildAgentId)
			if err != nil {
				continue
			}
			pivots = append(pivots, pivotData)
		}
	}
	return pivots
}
