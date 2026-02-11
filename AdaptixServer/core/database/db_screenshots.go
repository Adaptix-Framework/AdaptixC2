package database

import (
	"AdaptixServer/core/utils/logs"
	"database/sql"
	"errors"
	"fmt"

	adaptix "github.com/Adaptix-Framework/axc2"
)

func (dbms *DBMS) DbScreenshotExist(screenId string) bool {
	var id string
	err := dbms.database.QueryRow("SELECT ScreenId FROM Screenshots WHERE ScreenId = ? LIMIT 1;", screenId).Scan(&id)
	return err == nil
}

func (dbms *DBMS) DbScreenshotInsert(screenData adaptix.ScreenData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	insertQuery := `INSERT OR IGNORE INTO Screenshots (ScreenId, User, Computer, LocalPath, Note, Date) values(?,?,?,?,?,?);`
	result, err := dbms.database.Exec(insertQuery, screenData.ScreenId, screenData.User, screenData.Computer, screenData.LocalPath, screenData.Note, screenData.Date)
	if err != nil {
		return err
	}
	rows, _ := result.RowsAffected()
	if rows == 0 {
		return fmt.Errorf("screen %s already exists", screenData.ScreenId)
	}
	return nil
}

func (dbms *DBMS) DbScreenshotUpdate(screenId string, note string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	updateQuery := `UPDATE Screenshots SET Note = ? WHERE ScreenId = ?;`
	result, err := dbms.database.Exec(updateQuery, note, screenId)
	if err != nil {
		return err
	}
	rows, _ := result.RowsAffected()
	if rows == 0 {
		return fmt.Errorf("screen %s does not exist", screenId)
	}
	return nil
}

func (dbms *DBMS) DbScreenshotDelete(screenId string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	deleteQuery := `DELETE FROM Screenshots WHERE ScreenId = ?;`
	result, err := dbms.database.Exec(deleteQuery, screenId)
	if err != nil {
		return err
	}
	rows, _ := result.RowsAffected()
	if rows == 0 {
		return fmt.Errorf("screen %s does not exist", screenId)
	}
	return nil
}

func (dbms *DBMS) DbScreenshotById(screenId string) (*adaptix.ScreenData, error) {
	ok := dbms.DatabaseExists()
	if !ok {
		return nil, errors.New("database does not exist")
	}

	selectQuery := `SELECT ScreenId, User, Computer, LocalPath, Note, Date FROM Screenshots WHERE ScreenId = ?;`
	row := dbms.database.QueryRow(selectQuery, screenId)

	screenData := &adaptix.ScreenData{}
	err := row.Scan(&screenData.ScreenId, &screenData.User, &screenData.Computer, &screenData.LocalPath, &screenData.Note, &screenData.Date)
	if err != nil {
		return nil, fmt.Errorf("screen %s not found", screenId)
	}
	return screenData, nil
}

func (dbms *DBMS) DbScreenshotAll() []adaptix.ScreenData {
	var screens []adaptix.ScreenData

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT ScreenId, User, Computer, LocalPath, Note, Date FROM Screenshots ORDER BY Date;`
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
			logs.Debug("", "Failed to query screenshots: "+err.Error())
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)
	}
	return screens
}
