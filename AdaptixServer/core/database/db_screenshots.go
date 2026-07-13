package database

import (
	"AdaptixServer/core/utils/filter"
	"database/sql"
	"errors"
	"fmt"

	"github.com/Adaptix-Framework/axc2/v2"
)

var sortableScreenColumns = map[string]string{
	"ScreenId": "ScreenId",
	"User":     "User",
	"Computer": "Computer",
	"Note":     "Note",
	"Date":     "Date",
}

func (dbms *DBMS) DbScreenshotExist(screenId int64) bool {
	var id int64
	err := dbms.database.QueryRow("SELECT ScreenId FROM Screenshots WHERE ScreenId = ? LIMIT 1;", screenId).Scan(&id)
	return err == nil
}

func (dbms *DBMS) DbScreenshotInsert(screenData adaptix.ScreenData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	insertQuery := `INSERT OR IGNORE INTO Screenshots (ScreenId, AgentId, User, Computer, LocalPath, Note, Date) values(?,?,?,?,?,?,?);`
	result, err := dbms.database.Exec(insertQuery, screenData.ScreenId, screenData.AgentId, screenData.User, screenData.Computer, screenData.LocalPath, screenData.Note, screenData.Date)
	if err != nil {
		return err
	}
	rows, _ := result.RowsAffected()
	if rows == 0 {
		return fmt.Errorf("screen %d already exists", screenData.ScreenId)
	}
	return nil
}

func (dbms *DBMS) DbScreenshotUpdate(screenId int64, note string) error {
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
		return fmt.Errorf("screen %d does not exist", screenId)
	}
	return nil
}

func (dbms *DBMS) DbScreenshotDelete(screenId int64) error {
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
		return fmt.Errorf("screen %d does not exist", screenId)
	}
	return nil
}

func (dbms *DBMS) DbScreenshotById(screenId int64) (*adaptix.ScreenData, error) {
	ok := dbms.DatabaseExists()
	if !ok {
		return nil, errors.New("database does not exist")
	}

	selectQuery := `SELECT ScreenId, AgentId, User, Computer, LocalPath, Note, Date FROM Screenshots WHERE ScreenId = ?;`
	row := dbms.database.QueryRow(selectQuery, screenId)

	screenData := &adaptix.ScreenData{}
	err := row.Scan(&screenData.ScreenId, &screenData.AgentId, &screenData.User, &screenData.Computer, &screenData.LocalPath, &screenData.Note, &screenData.Date)
	if err != nil {
		return nil, fmt.Errorf("screen %d not found", screenId)
	}
	return screenData, nil
}

func (dbms *DBMS) DbScreenshotAll() []adaptix.ScreenData {
	var screens []adaptix.ScreenData

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT ScreenId, AgentId, User, Computer, LocalPath, Note, Date FROM Screenshots ORDER BY Date;`
		query, err := dbms.database.Query(selectQuery)
		if err == nil {
			defer func(query *sql.Rows) { _ = query.Close() }(query)
			for query.Next() {
				screenData := adaptix.ScreenData{}
				err = query.Scan(&screenData.ScreenId, &screenData.AgentId, &screenData.User, &screenData.Computer, &screenData.LocalPath, &screenData.Note, &screenData.Date)
				if err != nil {
					continue
				}
				screens = append(screens, screenData)
			}
		} else {
			dbms.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server:database", "Failed to query screenshots: %s", err.Error())
		}
	}
	return screens
}

func (dbms *DBMS) DbScreenshotsLimited(limit int) []adaptix.ScreenData {
	var screens []adaptix.ScreenData

	if !dbms.DatabaseExists() {
		return screens
	}

	selectQuery := `SELECT ScreenId, AgentId, User, Computer, LocalPath, Note, Date FROM Screenshots ORDER BY Date DESC LIMIT ?;`
	rows, err := dbms.database.Query(selectQuery, limit)
	if err != nil {
		dbms.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server:database", "Failed to query screenshots: %s", err.Error())
		return screens
	}
	defer func() { _ = rows.Close() }()

	for rows.Next() {
		s := adaptix.ScreenData{}
		err := rows.Scan(&s.ScreenId, &s.AgentId, &s.User, &s.Computer, &s.LocalPath, &s.Note, &s.Date)
		if err != nil {
			continue
		}
		screens = append(screens, s)
	}
	return screens
}

func (dbms *DBMS) DbScreenshotsGetPage(offset, limit int, filterExpr, sortCol, sortOrder string) ([]adaptix.ScreenData, int, error) {
	if !dbms.DatabaseExists() {
		return nil, 0, errors.New("database does not exist")
	}

	where := "1=1"
	var args []interface{}

	if filterExpr != "" {
		node, err := filter.ParseFilterExpr(filterExpr)
		if err != nil {
			return nil, 0, fmt.Errorf("invalid filter: %w", err)
		}
		if node != nil {
			filterSQL, filterArgs := filter.ToSQL(node, []string{"ScreenId", "User", "Computer", "Note"})
			if filterSQL != "" {
				where += " AND " + filterSQL
				args = append(args, filterArgs...)
			}
		}
	}

	orderClause := "Date DESC"
	if col, ok := sortableScreenColumns[sortCol]; ok {
		dir := "DESC"
		if sortOrder == "asc" || sortOrder == "ASC" {
			dir = "ASC"
		}
		orderClause = fmt.Sprintf(`"%s" %s`, col, dir)
	}

	var total int
	err := dbms.database.QueryRow("SELECT COUNT(*) FROM Screenshots WHERE "+where, args...).Scan(&total)
	if err != nil {
		return nil, 0, err
	}

	selectArgs := append(append([]interface{}(nil), args...), limit, offset)
	rows, err := dbms.database.Query(
		"SELECT ScreenId, AgentId, User, Computer, LocalPath, Note, Date"+
			" FROM Screenshots WHERE "+where+" ORDER BY "+orderClause+" LIMIT ? OFFSET ?",
		selectArgs...,
	)
	if err != nil {
		return nil, 0, err
	}
	defer func() { _ = rows.Close() }()

	out := make([]adaptix.ScreenData, 0, limit)
	for rows.Next() {
		var s adaptix.ScreenData
		err := rows.Scan(&s.ScreenId, &s.AgentId, &s.User, &s.Computer, &s.LocalPath, &s.Note, &s.Date)
		if err != nil {
			continue
		}
		out = append(out, s)
	}
	return out, total, nil
}
