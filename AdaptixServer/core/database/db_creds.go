package database

import (
	"AdaptixServer/core/utils/logs"
	"database/sql"
	"errors"
	"fmt"

	adaptix "github.com/Adaptix-Framework/axc2"
)

func (dbms *DBMS) DbCredentialsExist(credsId string) bool {
	rows, err := dbms.database.Query("SELECT CredId FROM Credentials WHERE CredId = ?;", credsId)
	if err != nil {
		return false
	}
	defer func(rows *sql.Rows) {
		_ = rows.Close()
	}(rows)

	return rows.Next()

}

func (dbms *DBMS) DbCredentialsAdd(credsData []*adaptix.CredsData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	for _, creds := range credsData {
		ok = dbms.DbCredentialsExist(creds.CredId)
		if ok {
			return fmt.Errorf("creds %s already exists", creds.CredId)
		}

		insertQuery := `INSERT INTO Credentials (CredId, Username, Password, Realm, Type, Tag, Date, Storage, AgentId, Host) values(?,?,?,?,?,?,?,?,?,?);`
		_, err := dbms.database.Exec(insertQuery, creds.CredId, creds.Username, creds.Password, creds.Realm, creds.Type,
			creds.Tag, creds.Date, creds.Storage, creds.AgentId, creds.Host)
		if err != nil {
			logs.Error("", err.Error())
			continue
		}
	}

	return nil
}

func (dbms *DBMS) DbCredentialsUpdate(credsData adaptix.CredsData) error {

	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	ok = dbms.DbCredentialsExist(credsData.CredId)
	if !ok {
		return fmt.Errorf("creds %s does not exist", credsData.CredId)
	}

	updateQuery := `UPDATE Credentials SET Username = ?, Password = ?, Realm = ?, Type = ?, Tag = ?, Storage = ?, Host = ? WHERE CredId = ?;`
	_, err := dbms.database.Exec(updateQuery, credsData.Username, credsData.Password, credsData.Realm, credsData.Type,
		credsData.Tag, credsData.Storage, credsData.Host, credsData.CredId)
	return err
}

func (dbms *DBMS) DbCredentialsDelete(credId string) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	ok = dbms.DbCredentialsExist(credId)
	if !ok {
		return fmt.Errorf("creds %s does not exist", credId)
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
		if err != nil {
			logs.Debug("", "Failed to query credentials: "+err.Error())
			return creds
		}
		defer query.Close()

		for query.Next() {
			credsData := &adaptix.CredsData{}
			err = query.Scan(&credsData.CredId, &credsData.Username, &credsData.Password, &credsData.Realm, &credsData.Type,
				&credsData.Tag, &credsData.Date, &credsData.Storage, &credsData.AgentId, &credsData.Host)
			if err != nil {
				continue
			}
			creds = append(creds, credsData)
		}
	}
	return creds
}
