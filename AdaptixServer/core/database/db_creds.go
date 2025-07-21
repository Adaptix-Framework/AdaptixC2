package database

import (
	"database/sql"
	"errors"
	"fmt"
	adaptix "github.com/Adaptix-Framework/axc2"
)

func (dbms *DBMS) DbCredentialsExist(creedsId string) bool {
	rows, err := dbms.database.Query("SELECT CredId FROM Credentials;")
	if err != nil {
		return false
	}
	defer func(rows *sql.Rows) {
		_ = rows.Close()
	}(rows)

	for rows.Next() {
		rowCredsId := ""
		_ = rows.Scan(&rowCredsId)
		if creedsId == rowCredsId {
			return true
		}
	}
	return false
}

func (dbms *DBMS) DbCredentialsAdd(credsData adaptix.CredsData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.DbCredentialsExist(credsData.CredId)
	if ok {
		return fmt.Errorf("creds %s alredy exists", credsData.CredId)
	}

	insertQuery := `INSERT INTO Credentials (CredId, Username, Password, Realm, Type, Tag, Date, Storage, AgentId, Host) values(?,?,?,?,?,?,?,?,?,?);`
	_, err := dbms.database.Exec(insertQuery, credsData.CredId, credsData.Username, credsData.Password, credsData.Realm, credsData.Type,
		credsData.Tag, credsData.Date, credsData.Storage, credsData.AgentId, credsData.Host)
	return err
}

func (dbms *DBMS) DbCredentialsUpdate(credsData adaptix.CredsData) error {

	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.DbCredentialsExist(credsData.CredId)
	if !ok {
		return fmt.Errorf("creds %s not exists", credsData.CredId)
	}

	updateQuery := `UPDATE Credentials SET Username = ?, Password = ?, Realm = ?, Type = ?, Tag = ?, Storage = ?, Host = ? WHERE CredId = ?;`
	_, err := dbms.database.Exec(updateQuery, credsData.Username, credsData.Password, credsData.Realm, credsData.Type,
		credsData.Tag, credsData.Storage, credsData.Host, credsData.CredId)
	return err
}

func (dbms *DBMS) DbCredentialsDelete(credId string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.DbCredentialsExist(credId)
	if !ok {
		return fmt.Errorf("creds %s not exists", credId)
	}

	deleteQuery := `DELETE FROM Credentials WHERE CredId = ?;`
	_, err := dbms.database.Exec(deleteQuery, credId)
	return err
}

func (dbms *DBMS) DbCredentialsAll() []*adaptix.CredsData {
	var creds []*adaptix.CredsData

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT CredId, Username, Password, Realm, Type, Tag, Date, Storage, AgentId, Host FROM Credentials;`
		query, err := dbms.database.Query(selectQuery)
		if err == nil {
			for query.Next() {
				credsData := &adaptix.CredsData{}
				err = query.Scan(&credsData.CredId, &credsData.Username, &credsData.Password, &credsData.Realm, &credsData.Type,
					&credsData.Tag, &credsData.Date, &credsData.Storage, &credsData.AgentId, &credsData.Host)
				if err != nil {
					continue
				}
				creds = append(creds, credsData)
			}
		} else {
			fmt.Println(err.Error() + " --- Clear database file!")
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)
	}
	return creds
}
