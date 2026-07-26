package database

import (
	"AdaptixServer/core/utils/filter"
	"database/sql"
	"errors"
	"fmt"
	"strings"

	"github.com/Adaptix-Framework/axc2/v2"
)

var sortableUploadColumns = map[string]string{
	"FileId":       "FileId",
	"AgentId":      "AgentId",
	"AgentName":    "AgentName",
	"User":         "User",
	"Computer":     "Computer",
	"RemotePath":   "RemotePath",
	"TotalSize":    "TotalSize",
	"Progress":     "Progress",
	"Date":         "Date",
	"State":        "State",
	"Tag":          "Tag",
	"Kind":         "Kind",
	"ArtifactName": "ArtifactName",
	"ArtifactType": "ArtifactType",
}

func (dbms *DBMS) DbUploadExist(fileId int64) bool {
	var id int64
	err := dbms.database.QueryRow("SELECT FileId FROM Uploads WHERE FileId = ? LIMIT 1;", fileId).Scan(&id)
	return err == nil
}

func (dbms *DBMS) DbUploadInsert(uploadData adaptix.TransferData) error {
	if !dbms.DatabaseExists() {
		return errors.New("database does not exist")
	}

	insertQuery := `INSERT OR IGNORE INTO Uploads (FileId, AgentId, AgentName, User, Computer, RemotePath, LocalPath, TotalSize, Progress, Date, State, Tag, Cancellable, Kind, ArtifactName, ArtifactType) values(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);`
	cancellable := 0
	if uploadData.Cancellable {
		cancellable = 1
	}
	result, err := dbms.database.Exec(insertQuery,
		uploadData.FileId, uploadData.AgentId, uploadData.AgentName, uploadData.User, uploadData.Computer, uploadData.RemotePath,
		uploadData.LocalPath, uploadData.TotalSize, uploadData.Progress, uploadData.Date, uploadData.State, uploadData.Tag,
		cancellable, uploadData.Kind, uploadData.ArtifactName, uploadData.ArtifactType,
	)
	if err != nil {
		return err
	}
	rows, _ := result.RowsAffected()
	if rows == 0 {
		return fmt.Errorf("upload %d already exists", uploadData.FileId)
	}
	return nil
}

func (dbms *DBMS) DbUploadDelete(fileId int64) error {
	if !dbms.DatabaseExists() {
		return errors.New("database does not exist")
	}
	deleteQuery := `DELETE FROM Uploads WHERE FileId = ?;`
	_, err := dbms.database.Exec(deleteQuery, fileId)
	return err
}

func (dbms *DBMS) DbUploadDeleteBatch(fileIds []int64) error {
	if len(fileIds) == 0 {
		return nil
	}
	if !dbms.DatabaseExists() {
		return errors.New("database does not exist")
	}

	placeholders := make([]string, len(fileIds))
	args := make([]interface{}, len(fileIds))
	for i, id := range fileIds {
		placeholders[i] = "?"
		args[i] = id
	}

	deleteQuery := fmt.Sprintf("DELETE FROM Uploads WHERE FileId IN (%s);", strings.Join(placeholders, ","))
	_, err := dbms.database.Exec(deleteQuery, args...)
	return err
}

func (dbms *DBMS) DbUploadGet(fileId int64) (adaptix.TransferData, error) {
	var d adaptix.TransferData
	if !dbms.DatabaseExists() {
		return d, errors.New("database does not exist")
	}

	var cancellable int
	selectQuery := `SELECT FileId, AgentId, AgentName, User, Computer, RemotePath, LocalPath, TotalSize, Progress, Date, State, Tag, Cancellable, Kind, ArtifactName, ArtifactType FROM Uploads WHERE FileId = ?;`
	err := dbms.database.QueryRow(selectQuery, fileId).Scan(
		&d.FileId, &d.AgentId, &d.AgentName, &d.User, &d.Computer, &d.RemotePath,
		&d.LocalPath, &d.TotalSize, &d.Progress, &d.Date, &d.State, &d.Tag,
		&cancellable, &d.Kind, &d.ArtifactName, &d.ArtifactType,
	)
	if err != nil {
		return d, fmt.Errorf("upload %d not found", fileId)
	}
	d.Cancellable = cancellable != 0
	return d, nil
}

func (dbms *DBMS) DbUploadAll() []adaptix.TransferData {
	var uploads []adaptix.TransferData

	if !dbms.DatabaseExists() {
		return uploads
	}

	selectQuery := `SELECT FileId, AgentId, AgentName, User, Computer, RemotePath, LocalPath, TotalSize, Progress, Date, State, Tag, Cancellable, Kind, ArtifactName, ArtifactType FROM Uploads ORDER BY Date;`
	query, err := dbms.database.Query(selectQuery)
	if err != nil {
		dbms.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server", "database", "Failed to query uploads: %s", err.Error())
		return uploads
	}
	defer func(query *sql.Rows) { _ = query.Close() }(query)

	for query.Next() {
		var d adaptix.TransferData
		var cancellable int
		err = query.Scan(&d.FileId, &d.AgentId, &d.AgentName, &d.User, &d.Computer, &d.RemotePath,
			&d.LocalPath, &d.TotalSize, &d.Progress, &d.Date, &d.State, &d.Tag,
			&cancellable, &d.Kind, &d.ArtifactName, &d.ArtifactType,
		)
		if err != nil {
			continue
		}
		d.Cancellable = cancellable != 0
		uploads = append(uploads, d)
	}
	return uploads
}

func (dbms *DBMS) DbUploadsGetPage(agentId int64, offset, limit int, filterExpr, sortCol, sortOrder string) ([]adaptix.TransferData, int, error) {
	if !dbms.DatabaseExists() {
		return nil, 0, errors.New("database does not exist")
	}

	where := "1=1"
	var args []interface{}

	if agentId != 0 {
		where += " AND AgentId = ?"
		args = append(args, agentId)
	}

	if filterExpr != "" {
		node, err := filter.ParseFilterExpr(filterExpr)
		if err != nil {
			return nil, 0, fmt.Errorf("invalid filter: %w", err)
		}
		if node != nil {
			filterSQL, filterArgs := filter.ToSQL(node, []string{"FileId", "AgentId", "AgentName", "User", "Computer", "RemotePath", "Tag", "ArtifactName", "ArtifactType"})
			if filterSQL != "" {
				where += " AND " + filterSQL
				args = append(args, filterArgs...)
			}
		}
	}

	orderClause := "Date DESC"
	if col, ok := sortableUploadColumns[sortCol]; ok {
		dir := "DESC"
		if sortOrder == "asc" || sortOrder == "ASC" {
			dir = "ASC"
		}
		orderClause = fmt.Sprintf(`"%s" %s`, col, dir)
	}

	var total int
	if err := dbms.database.QueryRow("SELECT COUNT(*) FROM Uploads WHERE "+where, args...).Scan(&total); err != nil {
		return nil, 0, err
	}

	selectArgs := append(append([]interface{}(nil), args...), limit, offset)
	rows, err := dbms.database.Query(
		"SELECT FileId, AgentId, AgentName, User, Computer, RemotePath, LocalPath, TotalSize, Progress, Date, State, Tag, Cancellable, Kind, ArtifactName, ArtifactType"+
			" FROM Uploads WHERE "+where+" ORDER BY "+orderClause+" LIMIT ? OFFSET ?",
		selectArgs...,
	)
	if err != nil {
		return nil, 0, err
	}
	defer func() { _ = rows.Close() }()

	out := make([]adaptix.TransferData, 0, limit)
	for rows.Next() {
		var d adaptix.TransferData
		var cancellable int
		err := rows.Scan(&d.FileId, &d.AgentId, &d.AgentName, &d.User, &d.Computer, &d.RemotePath,
			&d.LocalPath, &d.TotalSize, &d.Progress, &d.Date, &d.State, &d.Tag,
			&cancellable, &d.Kind, &d.ArtifactName, &d.ArtifactType,
		)
		if err != nil {
			continue
		}
		d.Cancellable = cancellable != 0
		d.LocalPath = "******"
		out = append(out, d)
	}
	return out, total, nil
}

func (dbms *DBMS) DbUploadUpdateState(fileId int64, state int, progress int64) error {
	if !dbms.DatabaseExists() {
		return errors.New("database does not exist")
	}
	_, err := dbms.database.Exec(
		"UPDATE Uploads SET State = ?, Progress = ? WHERE FileId = ?;",
		state, progress, fileId,
	)
	return err
}
