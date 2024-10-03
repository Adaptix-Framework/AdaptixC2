package database

import (
	"database/sql"
	_ "github.com/mattn/go-sqlite3"
)

type DBMS struct {
	database *sql.DB
	exists   bool
}

func NewDatabase(dbPath string) (*DBMS, error) {
	var err error

	dbms := &DBMS{
		exists: true,
	}

	dbms.database, err = sql.Open("sqlite3", dbPath)
	if err != nil {
		dbms.exists = false
	}

	if dbms.exists {
		err = dbms.DatabaseInit()
		if err != nil {
			dbms.exists = false
		}
	}
	return dbms, err
}

func (dbms *DBMS) DatabaseInit() error {
	var (
		err              error
		createTableQuery string
	)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Listeners" (
    	"ListenerName" TEXT NOT NULL UNIQUE, 
    	"ListenerType" TEXT NOT NULL,
    	"ListenerConfig" TEXT NOT NULL
    );`
	_, err = dbms.database.Exec(createTableQuery)
	if err != nil {
		return err
	}

	return nil
}

func (dbms *DBMS) DatabaseExists() bool {
	return dbms.exists
}