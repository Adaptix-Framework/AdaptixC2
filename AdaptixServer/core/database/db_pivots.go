package database

import (
	"AdaptixServer/core/adaptix"
	"database/sql"
	"errors"
	"fmt"
)

func (dbms *DBMS) DbPivotExist(pivotId string) bool {
	rows, err := dbms.database.Query("SELECT PivotId FROM Pivots;")
	if err != nil {
		return false
	}
	defer func(rows *sql.Rows) {
		_ = rows.Close()
	}(rows)

	for rows.Next() {
		rowPivotId := ""
		_ = rows.Scan(&rowPivotId)
		if pivotId == rowPivotId {
			return true
		}
	}
	return false
}

func (dbms *DBMS) DbPivotInsert(pivotData adaptix.PivotData) error {
	var (
		ok          bool
		err         error
		insertQuery string
	)

	ok = dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.DbPivotExist(pivotData.PivotId)
	if ok {
		return fmt.Errorf("pivot %s alredy exists", pivotData.PivotId)
	}

	insertQuery = `INSERT INTO Pivots (PivotId, PivotName, ParentAgentId, ChildAgentId) values(?,?,?,?);`
	_, err = dbms.database.Exec(insertQuery, pivotData.PivotId, pivotData.PivotName, pivotData.ParentAgentId, pivotData.ChildAgentId)

	return err
}

func (dbms *DBMS) DbPivotDelete(pivotId string) error {
	var (
		ok          bool
		err         error
		deleteQuery string
	)

	ok = dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.DbPivotExist(pivotId)
	if !ok {
		return fmt.Errorf("pivot %s not exists", pivotId)
	}

	deleteQuery = `DELETE FROM Pivots WHERE PivotId = ?;`
	_, err = dbms.database.Exec(deleteQuery, pivotId)

	return err
}

func (dbms *DBMS) DbPivotAll() []*adaptix.PivotData {
	var (
		pivots      []*adaptix.PivotData
		ok          bool
		selectQuery string
	)

	ok = dbms.DatabaseExists()
	if ok {
		selectQuery = `SELECT PivotId, PivotName, ParentAgentId, ChildAgentId FROM Pivots;`
		query, err := dbms.database.Query(selectQuery)
		if err == nil {

			for query.Next() {
				pivotData := &adaptix.PivotData{}
				err = query.Scan(&pivotData.PivotId, &pivotData.PivotName, &pivotData.ParentAgentId, &pivotData.ChildAgentId)
				if err != nil {
					continue
				}
				pivots = append(pivots, pivotData)
			}
		} else {
			fmt.Println(err.Error() + " --- Clear database file!")
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)
	}
	return pivots
}
