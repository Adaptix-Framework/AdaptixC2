package database

import (
	"database/sql"
	"errors"
	"fmt"
	"strings"

	"AdaptixServer/core/utils/logs"

	"github.com/Adaptix-Framework/axc2"
)

func (dbms *DBMS) DbDownloadExist(fileId string) bool {
	var id string
	err := dbms.database.QueryRow("SELECT FileId FROM Downloads WHERE FileId = ? LIMIT 1;", fileId).Scan(&id)
	return err == nil
}

func (dbms *DBMS) DbDownloadInsert(downloadData adaptix.DownloadData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	insertQuery := `INSERT OR IGNORE INTO Downloads (FileId, AgentId, AgentName, User, Computer, RemotePath, LocalPath, TotalSize, RecvSize, Date, State) values(?,?,?,?,?,?,?,?,?,?,?);`
	result, err := dbms.database.Exec(insertQuery,
		downloadData.FileId, downloadData.AgentId, downloadData.AgentName, downloadData.User, downloadData.Computer, downloadData.RemotePath,
		downloadData.LocalPath, downloadData.TotalSize, downloadData.RecvSize, downloadData.Date, downloadData.State,
	)
	if err != nil {
		return err
	}
	rows, _ := result.RowsAffected()
	if rows == 0 {
		return fmt.Errorf("download %s already exists", downloadData.FileId)
	}
	return nil
}

func (dbms *DBMS) DbDownloadDelete(fileId string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	deleteQuery := `DELETE FROM Downloads WHERE FileId = ?;`
	_, err := dbms.database.Exec(deleteQuery, fileId)
	return err
}

func (dbms *DBMS) DbDownloadDeleteBatch(fileIds []string) error {
	if len(fileIds) == 0 {
		return nil
	}

	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	placeholders := make([]string, len(fileIds))
	args := make([]interface{}, len(fileIds))
	for i, id := range fileIds {
		placeholders[i] = "?"
		args[i] = id
	}

	deleteQuery := fmt.Sprintf("DELETE FROM Downloads WHERE FileId IN (%s);",
		strings.Join(placeholders, ","))
	_, err := dbms.database.Exec(deleteQuery, args...)
	return err
}

func (dbms *DBMS) DbDownloadGet(fileId string) (adaptix.DownloadData, error) {
	var downloadData adaptix.DownloadData

	if !dbms.DatabaseExists() {
		return downloadData, errors.New("database does not exist")
	}

	selectQuery := `SELECT FileId, AgentId, AgentName, User, Computer, RemotePath, LocalPath, TotalSize, RecvSize, Date, State FROM Downloads WHERE FileId = ?;`
	err := dbms.database.QueryRow(selectQuery, fileId).Scan(&downloadData.FileId, &downloadData.AgentId, &downloadData.AgentName, &downloadData.User, &downloadData.Computer, &downloadData.RemotePath,
		&downloadData.LocalPath, &downloadData.TotalSize, &downloadData.RecvSize, &downloadData.Date, &downloadData.State,
	)
	if err != nil {
		return downloadData, fmt.Errorf("download %s not found", fileId)
	}
	return downloadData, nil
}

func (dbms *DBMS) DbDownloadAll() []adaptix.DownloadData {
	var downloads []adaptix.DownloadData

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT FileId, AgentId, AgentName, User, Computer, RemotePath, LocalPath, TotalSize, RecvSize, Date, State FROM Downloads ORDER BY Date;`
		query, err := dbms.database.Query(selectQuery)
		if err != nil {
			logs.Debug("", "Failed to query downloads: "+err.Error())
			return downloads
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)

		for query.Next() {
			downloadData := adaptix.DownloadData{}
			err = query.Scan(&downloadData.FileId, &downloadData.AgentId, &downloadData.AgentName, &downloadData.User, &downloadData.Computer, &downloadData.RemotePath,
				&downloadData.LocalPath, &downloadData.TotalSize, &downloadData.RecvSize, &downloadData.Date, &downloadData.State,
			)
			if err != nil {
				continue
			}
			downloads = append(downloads, downloadData)
		}
	}
	return downloads
}
