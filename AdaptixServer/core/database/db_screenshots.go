package database

import (
	"AdaptixServer/core/utils/logs"
	"database/sql"
	"errors"
	"fmt"

	adaptix "github.com/Adaptix-Framework/axc2"
)

func (dbms *DBMS) DbScreenshotExist(screenId string) bool {
	rows, err := dbms.database.Query("SELECT ScreenId FROM Screenshots;")
	if err != nil {
		return false
	}
	defer func(rows *sql.Rows) {
		_ = rows.Close()
	}(rows)

	for rows.Next() {
		rowScreenId := ""
		_ = rows.Scan(&rowScreenId)
		if screenId == rowScreenId {
			return true
		}
	}
	return false
}

func (dbms *DBMS) DbScreenshotInsert(screenData adaptix.ScreenData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.DbScreenshotExist(screenData.ScreenId)
	if ok {
		return fmt.Errorf("screen %s alredy exists", screenData.ScreenId)
	}

	insertQuery := `INSERT INTO Screenshots (ScreenId, User, Computer, LocalPath, Note, Date) values(?,?,?,?,?,?);`
	_, err := dbms.database.Exec(insertQuery, screenData.ScreenId, screenData.User, screenData.Computer, screenData.LocalPath, screenData.Note, screenData.Date)
	return err
}

func (dbms *DBMS) DbScreenshotUpdate(screenId string, note string) error {

	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.DbScreenshotExist(screenId)
	if !ok {
		return fmt.Errorf("screen %s not exists", screenId)
	}

	updateQuery := `UPDATE Screenshots SET Note = ? WHERE ScreenId = ?;`
	_, err := dbms.database.Exec(updateQuery, screenId, note)
	return err
}

func (dbms *DBMS) DbScreenshotDelete(screenId string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.DbScreenshotExist(screenId)
	if !ok {
		return fmt.Errorf("screen %s not exists", screenId)
	}

	deleteQuery := `DELETE FROM Screenshots WHERE ScreenId = ?;`
	_, err := dbms.database.Exec(deleteQuery, screenId)
	return err
}

func (dbms *DBMS) DbScreenshotAll() []adaptix.ScreenData {
	var screens []adaptix.ScreenData

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT ScreenId, User, Computer, LocalPath, Note, Date FROM Screenshots;`
		query, err := dbms.database.Query(selectQuery)
		if err == nil {
			for query.Next() {
				screenData := adaptix.ScreenData{}
				err = query.Scan(&screenData.ScreenId, &screenData.User, &screenData.Computer, &screenData.LocalPath, &screenData.Note, &screenData.Date)
				if err != nil {
					continue
				}
				screens = append(screens, screenData)
			}
		} else {
			logs.Debug("", err.Error()+" --- Clear database file!")
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)
	}
	return screens
}
