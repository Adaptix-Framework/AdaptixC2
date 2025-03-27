package database

import (
	"AdaptixServer/core/adaptix"
	"database/sql"
	"errors"
	"fmt"
)

func (dbms *DBMS) DbDownloadExist(fileId string) bool {
	rows, err := dbms.database.Query("SELECT FileId FROM Downloads;")
	if err != nil {
		return false
	}
	defer func(rows *sql.Rows) {
		_ = rows.Close()
	}(rows)

	for rows.Next() {
		rowFileId := ""
		_ = rows.Scan(&rowFileId)
		if fileId == rowFileId {
			return true
		}
	}
	return false
}

func (dbms *DBMS) DbDownloadInsert(downloadData adaptix.DownloadData) error {
	var (
		err         error
		ok          bool
		insertQuery string
	)

	ok = dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.DbDownloadExist(downloadData.FileId)
	if ok {
		return fmt.Errorf("download %s alredy exists", downloadData.FileId)
	}

	insertQuery = `INSERT INTO Downloads (FileId, AgentId, AgentName, User, Computer, RemotePath, LocalPath, TotalSize, RecvSize, Date, State) values(?,?,?,?,?,?,?,?,?,?,?);`
	_, err = dbms.database.Exec(insertQuery,
		downloadData.FileId, downloadData.AgentId, downloadData.AgentName, downloadData.User, downloadData.Computer, downloadData.RemotePath,
		downloadData.LocalPath, downloadData.TotalSize, downloadData.RecvSize, downloadData.Date, downloadData.State,
	)
	return err
}

func (dbms *DBMS) DbDownloadDelete(fileId string) error {
	var (
		ok          bool
		err         error
		deleteQuery string
	)

	ok = dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.DbDownloadExist(fileId)
	if !ok {
		return fmt.Errorf("download %s does not exists", fileId)
	}

	deleteQuery = `DELETE FROM Downloads WHERE FileId = ?;`
	_, err = dbms.database.Exec(deleteQuery, fileId)

	return err
}

func (dbms *DBMS) DbDownloadAll() []adaptix.DownloadData {
	var (
		downloads   []adaptix.DownloadData
		ok          bool
		selectQuery string
	)

	ok = dbms.DatabaseExists()
	if ok {
		selectQuery = `SELECT FileId, AgentId, AgentName, User, Computer, RemotePath, LocalPath, TotalSize, RecvSize, Date, State FROM Downloads;`
		query, err := dbms.database.Query(selectQuery)
		if err == nil {

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
		} else {
			fmt.Println(err.Error() + " --- Clear database file!")
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)
	}
	return downloads
}
