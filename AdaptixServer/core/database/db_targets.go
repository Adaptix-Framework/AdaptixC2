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

func parseTargetId(v interface{}) (int64, bool) {
	switch t := v.(type) {
	case int64:
		return t, true
	case int32:
		return int64(t), true
	case int:
		return int64(t), true
	case float64:
		return int64(t), true
	case []byte:
		return parseTargetId(string(t))
	case string:
		s := strings.TrimSpace(t)
		if s == "" {
			return 0, false
		}
		if id, err := strconv.ParseInt(s, 10, 64); err == nil {
			return id, true
		}
		return 0, false
	default:
		return 0, false
	}
}

func scanTargetRow(scanner interface {
	Scan(dest ...interface{}) error
}, target *adaptix.TargetData) error {
	var (
		rawId     interface{}
		agentsStr string
		alive     bool
	)
	err := scanner.Scan(&rawId, &target.Computer, &target.Domain, &target.Address, &target.Os, &target.OsDesk, &target.Tag, &target.Info, &target.Date, &alive, &agentsStr)
	if err != nil {
		return err
	}
	id, ok := parseTargetId(rawId)
	if !ok {
		return fmt.Errorf("unusable target id %v", rawId)
	}
	target.TargetId = id
	target.Alive = alive
	if agentsStr != "" {
		target.Agents = splitInt64s(agentsStr)
	} else {
		target.Agents = nil
	}
	return nil
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
			dbms.ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "database", "%s", err.Error())
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

	idStr := strconv.FormatInt(targetData.TargetId, 10)
	updateQuery := `UPDATE Targets SET Computer = ?, Domain = ?, Address = ?, Os = ?, OsDesk = ?, Tag = ?, Info = ?, Alive = ?, Agents = ? WHERE TargetId = ? OR CAST(TargetId AS TEXT) = ?;`
	result, err := dbms.database.Exec(updateQuery, targetData.Computer, targetData.Domain, targetData.Address, targetData.Os, targetData.OsDesk, targetData.Tag, targetData.Info, targetData.Alive, joinInt64s(targetData.Agents), targetData.TargetId, idStr)
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

	updateQuery := `UPDATE Targets SET Computer = ?, Domain = ?, Address = ?, Os = ?, OsDesk = ?, Tag = ?, Info = ?, Alive = ?, Agents = ? WHERE TargetId = ? OR CAST(TargetId AS TEXT) = ?;`
	stmt, err := tx.Prepare(updateQuery)
	if err != nil {
		_ = tx.Rollback()
		return err
	}
	defer func(stmt *sql.Stmt) {
		_ = stmt.Close()
	}(stmt)

	for _, t := range targets {
		idStr := strconv.FormatInt(t.TargetId, 10)
		_, err = stmt.Exec(t.Computer, t.Domain, t.Address, t.Os, t.OsDesk, t.Tag, t.Info, t.Alive, joinInt64s(t.Agents), t.TargetId, idStr)
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
	selectQuery := `SELECT TargetId, Computer, Domain, Address, Os, OsDesk, Tag, Info, Date, Alive, Agents FROM Targets WHERE TargetId = ? OR CAST(TargetId AS TEXT) = ? LIMIT 1;`
	idStr := strconv.FormatInt(targetId, 10)
	row := dbms.database.QueryRow(selectQuery, targetId, idStr)
	target := &adaptix.TargetData{}
	if err := scanTargetRow(row, target); err != nil {
		return nil, fmt.Errorf("target %d not found", targetId)
	}
	return target, nil
}

func (dbms *DBMS) DbTargetFindByMatch(address, computer, domain string) (*adaptix.TargetData, error) {
	if !dbms.DatabaseExists() {
		return nil, errors.New("database does not exist")
	}

	address = strings.TrimSpace(address)
	computer = strings.TrimSpace(computer)
	domain = strings.TrimSpace(domain)

	const cols = `TargetId, Computer, Domain, Address, Os, OsDesk, Tag, Info, Date, Alive, Agents`
	const order = ` ORDER BY CASE WHEN CAST(TargetId AS TEXT) GLOB '[0-9]*' THEN 0 ELSE 1 END, Date DESC`

	tryQuery := func(q string, args ...interface{}) *adaptix.TargetData {
		rows, err := dbms.database.Query(q, args...)
		if err != nil {
			return nil
		}
		defer func() { _ = rows.Close() }()
		for rows.Next() {
			t := &adaptix.TargetData{}
			if err := scanTargetRow(rows, t); err != nil {
				continue
			}
			return t
		}
		return nil
	}

	if address != "" {
		if t := tryQuery(
			`SELECT `+cols+` FROM Targets WHERE Address != '' AND Address = ?`+order,
			address,
		); t != nil {
			return t, nil
		}
	}

	if computer != "" {
		if t := tryQuery(`SELECT `+cols+` FROM Targets WHERE Computer != '' AND LOWER(Computer) = LOWER(?) AND LOWER(IFNULL(Domain, '')) = LOWER(?)`+order, computer, domain); t != nil {
			return t, nil
		}
	}

	return nil, fmt.Errorf("target not found")
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
			dbms.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server", "database", "Failed to query targets: %s", err.Error())
			return targets
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)

		for query.Next() {
			targetData := &adaptix.TargetData{}
			if err = scanTargetRow(query, targetData); err != nil {
				continue
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
		if err := scanTargetRow(rows, &t); err != nil {
			continue
		}
		out = append(out, t)
	}
	return out, total, nil
}
