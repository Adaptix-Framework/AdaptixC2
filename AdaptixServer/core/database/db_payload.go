package database

import (
	"database/sql"
	"encoding/json"
	"errors"
	"fmt"
	"strings"

	"AdaptixServer/core/utils/filter"

	"github.com/Adaptix-Framework/axc2/v2"
)

func (dbms *DBMS) DbPayloadInsert(p adaptix.PayloadData) error {
	if !dbms.DatabaseExists() {
		return errors.New("database does not exist")
	}
	listenersJSON, err := json.Marshal(p.Listeners)
	if err != nil {
		return err
	}
	hidden := 0
	if p.Hidden {
		hidden = 1
	}
	q := `INSERT INTO Payloads ( PayloadId, Name, AgentType, Artifact, Arch, Listeners, Size, Sha1, Sha256, Md5, Creator, Created, Hidden, LocalPath, ConfigJson, BuildId, Watermark, Filename, Notes, Uid, Color) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);`
	_, err = dbms.database.Exec(q, p.PayloadId, p.Name, p.AgentType, p.Artifact, p.Arch, string(listenersJSON), p.Size, p.Sha1, p.Sha256, p.Md5, p.Creator, p.Created, hidden, p.LocalPath, p.ConfigJson, p.BuildId, p.Watermark, p.Filename, p.Notes, p.Uid, p.Color)
	return err
}

func scanPayload(row interface {
	Scan(dest ...any) error
}) (adaptix.PayloadData, error) {
	var p adaptix.PayloadData
	var listenersJSON string
	var hidden int
	err := row.Scan(&p.PayloadId, &p.Name, &p.AgentType, &p.Artifact, &p.Arch, &listenersJSON, &p.Size, &p.Sha1, &p.Sha256, &p.Md5, &p.Creator, &p.Created, &hidden, &p.LocalPath, &p.ConfigJson, &p.BuildId, &p.Watermark, &p.Filename, &p.Notes, &p.Uid, &p.Color)
	if err != nil {
		return p, err
	}
	p.Hidden = hidden != 0
	if listenersJSON != "" {
		_ = json.Unmarshal([]byte(listenersJSON), &p.Listeners)
	}
	if p.Listeners == nil {
		p.Listeners = []string{}
	}
	return p, nil
}

const payloadSelectCols = `PayloadId, Name, AgentType, Artifact, Arch, Listeners, Size, Sha1, Sha256, Md5, Creator, Created, Hidden, LocalPath, ConfigJson, BuildId, Watermark, Filename, Notes, Uid, Color`

func (dbms *DBMS) DbPayloadGet(id int64) (adaptix.PayloadData, error) {
	var p adaptix.PayloadData
	if !dbms.DatabaseExists() {
		return p, errors.New("database does not exist")
	}
	q := `SELECT ` + payloadSelectCols + ` FROM Payloads WHERE PayloadId = ?;`
	p, err := scanPayload(dbms.database.QueryRow(q, id))
	if err != nil {
		if errors.Is(err, sql.ErrNoRows) {
			return p, fmt.Errorf("payload %d not found", id)
		}
		return p, err
	}
	return p, nil
}

func (dbms *DBMS) DbPayloadList(showHidden bool) ([]adaptix.PayloadData, error) {
	items, _, err := dbms.DbPayloadGetPage(0, 100000, showHidden, "", "Created", "desc")
	return items, err
}

var sortablePayloadColumns = map[string]string{
	"Created":  "Created",
	"Name":     "Name",
	"Type":     "AgentType",
	"Artifact": "Artifact",
	"Arch":     "Arch",
	"Size":     "Size",
	"Creator":  "Creator",
	"Filename": "Filename",
}

func (dbms *DBMS) DbPayloadGetPage(offset, limit int, showHidden bool, filterExpr, sortCol, sortOrder string) ([]adaptix.PayloadData, int, error) {
	if !dbms.DatabaseExists() {
		return nil, 0, errors.New("database does not exist")
	}
	if limit <= 0 {
		limit = 100
	}
	if offset < 0 {
		offset = 0
	}

	where := "1=1"
	var args []interface{}

	if !showHidden {
		where += " AND Hidden = 0"
	}

	if filterExpr != "" {
		node, err := filter.ParseFilterExpr(filterExpr)
		if err != nil {
			return nil, 0, fmt.Errorf("invalid filter: %w", err)
		}
		if node != nil {
			filterSQL, filterArgs := filter.ToSQL(node, []string{"PayloadId", "Name", "AgentType", "Artifact", "Arch", "Listeners", "Filename", "Creator", "Sha1", "Sha256", "Md5", "BuildId", "Watermark", "Notes"})
			if filterSQL != "" {
				where += " AND " + filterSQL
				args = append(args, filterArgs...)
			}
		}
	}

	orderClause := "Created DESC"
	if col, ok := sortablePayloadColumns[sortCol]; ok {
		dir := "DESC"
		if sortOrder == "asc" || sortOrder == "ASC" {
			dir = "ASC"
		}
		orderClause = fmt.Sprintf(`"%s" %s`, col, dir)
	}

	var total int
	if err := dbms.database.QueryRow("SELECT COUNT(*) FROM Payloads WHERE "+where, args...).Scan(&total); err != nil {
		return nil, 0, err
	}

	selectArgs := append(append([]interface{}(nil), args...), limit, offset)
	rows, err := dbms.database.Query(`SELECT `+payloadSelectCols+` FROM Payloads WHERE `+where+` ORDER BY `+orderClause+` LIMIT ? OFFSET ?`, selectArgs...)
	if err != nil {
		return nil, 0, err
	}
	defer func() { _ = rows.Close() }()

	out := make([]adaptix.PayloadData, 0, limit)
	for rows.Next() {
		p, err := scanPayload(rows)
		if err != nil {
			continue
		}
		out = append(out, p)
	}
	return out, total, nil
}

func (dbms *DBMS) DbPayloadSetHidden(ids []int64, hidden bool) error {
	if len(ids) == 0 {
		return nil
	}
	if !dbms.DatabaseExists() {
		return errors.New("database does not exist")
	}
	ph := make([]string, len(ids))
	args := make([]interface{}, 0, len(ids)+1)
	hv := 0
	if hidden {
		hv = 1
	}
	args = append(args, hv)
	for i, id := range ids {
		ph[i] = "?"
		args = append(args, id)
	}
	q := fmt.Sprintf(`UPDATE Payloads SET Hidden = ? WHERE PayloadId IN (%s);`, strings.Join(ph, ","))
	_, err := dbms.database.Exec(q, args...)
	return err
}

func (dbms *DBMS) DbPayloadSetColor(ids []int64, color string) error {
	if len(ids) == 0 {
		return nil
	}
	if !dbms.DatabaseExists() {
		return errors.New("database does not exist")
	}
	ph := make([]string, len(ids))
	args := make([]interface{}, 0, len(ids)+1)
	args = append(args, color)
	for i, id := range ids {
		ph[i] = "?"
		args = append(args, id)
	}
	q := fmt.Sprintf(`UPDATE Payloads SET Color = ? WHERE PayloadId IN (%s);`, strings.Join(ph, ","))
	_, err := dbms.database.Exec(q, args...)
	return err
}

func (dbms *DBMS) DbPayloadUpdateMeta(id int64, name, notes, artifact, arch string, hidden bool) error {
	if !dbms.DatabaseExists() {
		return errors.New("database does not exist")
	}
	hv := 0
	if hidden {
		hv = 1
	}
	q := `UPDATE Payloads SET Name = ?, Notes = ?, Artifact = ?, Arch = ?, Hidden = ? WHERE PayloadId = ?;`
	res, err := dbms.database.Exec(q, name, notes, artifact, arch, hv, id)
	if err != nil {
		return err
	}
	n, _ := res.RowsAffected()
	if n == 0 {
		return fmt.Errorf("payload %d not found", id)
	}
	return nil
}

func (dbms *DBMS) DbPayloadDelete(ids []int64) error {
	if len(ids) == 0 {
		return nil
	}
	if !dbms.DatabaseExists() {
		return errors.New("database does not exist")
	}
	ph := make([]string, len(ids))
	args := make([]interface{}, len(ids))
	for i, id := range ids {
		ph[i] = "?"
		args[i] = id
	}
	q := fmt.Sprintf(`DELETE FROM Payloads WHERE PayloadId IN (%s);`, strings.Join(ph, ","))
	_, err := dbms.database.Exec(q, args...)
	return err
}
