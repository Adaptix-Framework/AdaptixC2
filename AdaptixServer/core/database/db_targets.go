package database

import (
	"AdaptixServer/core/utils/filter"
	"database/sql"
	"errors"
	"fmt"
	"strconv"
	"strings"

	"github.com/Adaptix-Framework/axc2/v2"
)

var sortableTargetColumns = map[string]string{
	"TargetId": "TargetId",
	"Computer": "Computer",
	"Domain":   "Domain",
	"Address":  "Address",
	"Tag":      "Tag",
	"Os":       "Os",
	"OsDesk":   "OsDesk",
	"Date":     "Date",
	"Info":     "Info",
	"Alive":    "Alive",
}

func joinInt64s(ids []int64) string {
	if len(ids) == 0 {
		return ""
	}
	parts := make([]string, len(ids))
	for i, id := range ids {
		parts[i] = strconv.FormatInt(id, 10)
	}
	return strings.Join(parts, ",")
}

func splitInt64s(s string) []int64 {
	if s == "" {
		return nil
	}
	parts := strings.Split(s, ",")
	out := make([]int64, 0, len(parts))
	for _, p := range parts {
		v, err := strconv.ParseInt(strings.TrimSpace(p), 10, 64)
		if err == nil {
			out = append(out, v)
		}
	}
	return out
}

func (dbms *DBMS) DbTargetExist(targetId int64) bool {
	var id string
	err := dbms.database.QueryRow("SELECT TargetId FROM Targets WHERE TargetId = ? LIMIT 1;", targetId).Scan(&id)
	return err == nil
}

func (dbms *DBMS) DbTargetsAdd(targetsData []*adaptix.TargetData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	tx, err := dbms.database.Begin()
	if err != nil {
		return err
	}

	insertQuery := `INSERT OR IGNORE INTO Targets (TargetId, Computer, Domain, Address, Os, OsDesk, Tag, Info, Date, Alive, Agents) values(?,?,?,?,?,?,?,?,?,?,?);`
	stmt, err := tx.Prepare(insertQuery)
	if err != nil {
		_ = tx.Rollback()
		return err
	}
	defer func(stmt *sql.Stmt) {
		_ = stmt.Close()
	}(stmt)

	for _, targetData := range targetsData {
		_, err = stmt.Exec(targetData.TargetId, targetData.Computer, targetData.Domain, targetData.Address, targetData.Os,
			targetData.OsDesk, targetData.Tag, targetData.Info, targetData.Date, targetData.Alive, joinInt64s(targetData.Agents))
		if err != nil {
			dbms.ts.TsLogAdd(adaptix.LogStatusError, 0, "server:database", "%s", err.Error())
			continue
		}
	}

	return tx.Commit()
}

func (dbms *DBMS) DbTargetUpdate(targetData *adaptix.TargetData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	updateQuery := `UPDATE Targets SET Computer = ?, Domain = ?, Address = ?, Os = ?, OsDesk = ?, Tag = ?, Info = ?, Alive = ?, Agents = ? WHERE TargetId = ?;`
	result, err := dbms.database.Exec(updateQuery, targetData.Computer, targetData.Domain, targetData.Address, targetData.Os,
		targetData.OsDesk, targetData.Tag, targetData.Info, targetData.Alive, joinInt64s(targetData.Agents), targetData.TargetId)
	if err != nil {
		return err
	}
	rows, _ := result.RowsAffected()
	if rows == 0 {
		return fmt.Errorf("target %d does not exist", targetData.TargetId)
	}
	return nil
}

func (dbms *DBMS) DbTargetDelete(targetId string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	deleteQuery := `DELETE FROM Targets WHERE TargetId = ?;`
	_, err := dbms.database.Exec(deleteQuery, targetId)
	return err
}

func (dbms *DBMS) DbTargetDeleteBatch(targetIds []int64) error {
	if len(targetIds) == 0 {
		return nil
	}

	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	placeholders := make([]string, len(targetIds))
	args := make([]interface{}, len(targetIds))
	for i, id := range targetIds {
		placeholders[i] = "?"
		args[i] = id
	}

	deleteQuery := fmt.Sprintf("DELETE FROM Targets WHERE TargetId IN (%s);",
		strings.Join(placeholders, ","))
	_, err := dbms.database.Exec(deleteQuery, args...)
	return err
}

func (dbms *DBMS) DbTargetUpdateBatch(targets []*adaptix.TargetData) error {
	if len(targets) == 0 {
		return nil
	}

	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	tx, err := dbms.database.Begin()
	if err != nil {
		return err
	}

	updateQuery := `UPDATE Targets SET Computer = ?, Domain = ?, Address = ?, Os = ?, OsDesk = ?, Tag = ?, Info = ?, Alive = ?, Agents = ? WHERE TargetId = ?;`
	stmt, err := tx.Prepare(updateQuery)
	if err != nil {
		_ = tx.Rollback()
		return err
	}
	defer func(stmt *sql.Stmt) {
		_ = stmt.Close()
	}(stmt)

	for _, t := range targets {
		_, err = stmt.Exec(t.Computer, t.Domain, t.Address, t.Os, t.OsDesk, t.Tag, t.Info, t.Alive, joinInt64s(t.Agents), t.TargetId)
		if err != nil {
			_ = tx.Rollback()
			return err
		}
	}

	return tx.Commit()
}

func (dbms *DBMS) DbTargetById(targetId int64) (*adaptix.TargetData, error) {
	if !dbms.DatabaseExists() {
		return nil, errors.New("database does not exist")
	}
	target := &adaptix.TargetData{}
	agentsStr := ""
	selectQuery := `SELECT TargetId, Computer, Domain, Address, Os, OsDesk, Tag, Info, Date, Alive, Agents FROM Targets WHERE TargetId = ?;`
	err := dbms.database.QueryRow(selectQuery, targetId).Scan(&target.TargetId, &target.Computer, &target.Domain, &target.Address, &target.Os,
		&target.OsDesk, &target.Tag, &target.Info, &target.Date, &target.Alive, &agentsStr)
	if err != nil {
		return nil, fmt.Errorf("target %d not found", targetId)
	}
	if agentsStr != "" {
		target.Agents = splitInt64s(agentsStr)
	}
	return target, nil
}

func (dbms *DBMS) DbTargetFindByMatch(address, computer, domain string) (*adaptix.TargetData, error) {
	if !dbms.DatabaseExists() {
		return nil, errors.New("database does not exist")
	}

	selectQuery := `SELECT TargetId, Computer, Domain, Address, Os, OsDesk, Tag, Info, Date, Alive, Agents FROM Targets WHERE (Address = ? AND Address != '') OR (LOWER(Computer) = LOWER(?) AND LOWER(Domain) = LOWER(?)) LIMIT 1;`
	target := &adaptix.TargetData{}
	agentsStr := ""
	err := dbms.database.QueryRow(selectQuery, address, computer, domain).Scan(&target.TargetId, &target.Computer, &target.Domain, &target.Address, &target.Os,
		&target.OsDesk, &target.Tag, &target.Info, &target.Date, &target.Alive, &agentsStr)
	if err != nil {
		return nil, fmt.Errorf("target not found")
	}
	if agentsStr != "" {
		target.Agents = splitInt64s(agentsStr)
	}
	return target, nil
}

func (dbms *DBMS) DbTargetSetTagBatch(targetIds []int64, tag string) error {
	if len(targetIds) == 0 {
		return nil
	}
	if !dbms.DatabaseExists() {
		return errors.New("database does not exist")
	}
	placeholders := make([]string, len(targetIds))
	args := make([]interface{}, 0, len(targetIds)+1)
	args = append(args, tag)
	for i, id := range targetIds {
		placeholders[i] = "?"
		args = append(args, id)
	}
	updateQuery := fmt.Sprintf("UPDATE Targets SET Tag = ? WHERE TargetId IN (%s);", strings.Join(placeholders, ","))
	_, err := dbms.database.Exec(updateQuery, args...)
	return err
}

func (dbms *DBMS) DbTargetsAll() []*adaptix.TargetData {
	var targets []*adaptix.TargetData

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT TargetId, Computer, Domain, Address, Os, OsDesk, Tag, Info, Date, Alive, Agents FROM Targets ORDER BY Date;`
		query, err := dbms.database.Query(selectQuery)
		if err != nil {
			dbms.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server:database", "Failed to query targets: %s", err.Error())
			return targets
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)

		for query.Next() {
			targetData := &adaptix.TargetData{}
			agentsStr := ""
			err = query.Scan(&targetData.TargetId, &targetData.Computer, &targetData.Domain, &targetData.Address, &targetData.Os,
				&targetData.OsDesk, &targetData.Tag, &targetData.Info, &targetData.Date, &targetData.Alive, &agentsStr)
			if err != nil {
				continue
			}
			if agentsStr != "" {
				targetData.Agents = splitInt64s(agentsStr)
			}

			targets = append(targets, targetData)
		}
	}
	return targets
}

func (dbms *DBMS) DbTargetsGetPage(offset, limit int, filterExpr, sortCol, sortOrder string) ([]adaptix.TargetData, int, error) {
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
			filterSQL, filterArgs := filter.ToSQL(node, []string{
				"TargetId", "Computer", "Domain", "Address",
				"Tag", "OsDesk", "Info",
			})
			if filterSQL != "" {
				where += " AND " + filterSQL
				args = append(args, filterArgs...)
			}
		}
	}

	orderClause := "Date DESC"
	if col, ok := sortableTargetColumns[sortCol]; ok {
		dir := "DESC"
		if sortOrder == "asc" || sortOrder == "ASC" {
			dir = "ASC"
		}
		orderClause = fmt.Sprintf(`"%s" %s`, col, dir)
	}

	var total int
	if err := dbms.database.QueryRow("SELECT COUNT(*) FROM Targets WHERE "+where, args...).Scan(&total); err != nil {
		return nil, 0, err
	}

	selectArgs := append(append([]interface{}(nil), args...), limit, offset)
	rows, err := dbms.database.Query(
		"SELECT TargetId, Computer, Domain, Address, Os, OsDesk, Tag, Info, Date, Alive, Agents"+
			" FROM Targets WHERE "+where+" ORDER BY "+orderClause+" LIMIT ? OFFSET ?",
		selectArgs...,
	)
	if err != nil {
		return nil, 0, err
	}
	defer func() { _ = rows.Close() }()

	out := make([]adaptix.TargetData, 0, limit)
	for rows.Next() {
		var t adaptix.TargetData
		agentsStr := ""
		err := rows.Scan(&t.TargetId, &t.Computer, &t.Domain, &t.Address, &t.Os, &t.OsDesk, &t.Tag, &t.Info, &t.Date, &t.Alive, &agentsStr)
		if err != nil {
			continue
		}
		if agentsStr != "" {
			t.Agents = splitInt64s(agentsStr)
		}
		out = append(out, t)
	}
	return out, total, nil
}
