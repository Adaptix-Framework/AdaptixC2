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
    	"ListenerConfig" TEXT NOT NULL,
    	"Watermark" TEXT NOT NULL,
    	"CustomData" BLOB
    );`
	_, err = dbms.database.Exec(createTableQuery)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Downloads" (
    	"FileId" TEXT NOT NULL UNIQUE, 
    	"AgentId" TEXT NOT NULL,
    	"AgentName" TEXT NOT NULL,
    	"User" TEXT NOT NULL,
    	"Computer" TEXT NOT NULL,
    	"RemotePath" TEXT NOT NULL,
    	"LocalPath" TEXT NOT NULL,
    	"TotalSize" INTEGER,
    	"RecvSize" INTEGER,
    	"Date" BIGINT,
    	"State" INTEGER
    );`
	_, err = dbms.database.Exec(createTableQuery)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Agents" (
    	"Id" TEXT NOT NULL UNIQUE, 
    	"Crc" TEXT NOT NULL,
    	"Name" TEXT NOT NULL,
    	"SessionKey" BLOB NOT NULL,
    	"Listener" TEXT NOT NULL,
    	"Async" INTEGER,
    	"ExternalIP" TEXT,
    	"InternalIP" TEXT,
    	"GmtOffset" INTEGER,
    	"Sleep" INTEGER,
    	"Jitter" INTEGER,
    	"Pid" TEXT,
    	"Tid" TEXT,
    	"Arch" TEXT,
    	"Elevated" INTEGER,
    	"Process" TEXT,
    	"Os" INTEGER,
    	"OsDesc" TEXT,
    	"Domain" TEXT,
    	"Computer" TEXT,
    	"Username" TEXT,
    	"Impersonated" TEXT,
    	"OemCP" INTEGER,
    	"ACP" INTEGER,
    	"CreateTime" BIGINT,
    	"LastTick" INTEGER,
    	"Tags" TEXT,
    	"Mark" TEXT,
    	"Color" TEXT
    );`
	_, err = dbms.database.Exec(createTableQuery)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Tasks" (
    	"TaskId" TEXT NOT NULL UNIQUE, 
    	"AgentId" TEXT NOT NULL,
    	"TaskType" INTEGER,
    	"Client" TEXT,
    	"User" TEXT,
    	"Computer" TEXT,
    	"StartDate" BIGINT,
    	"FinishDate" BIGINT,
    	"CommandLine" TEXT NOT NULL,
    	"MessageType" INTEGER,
    	"Message" TEXT,
    	"ClearText" TEXT,
    	"Completed" INTEGER
    );`
	_, err = dbms.database.Exec(createTableQuery)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Consoles" (
		"Id" INTEGER PRIMARY KEY AUTOINCREMENT,
    	"AgentId" TEXT NOT NULL,
    	"Packet" BLOB
    );`
	_, err = dbms.database.Exec(createTableQuery)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Pivots" (
		"Id" INTEGER PRIMARY KEY AUTOINCREMENT,
    	"PivotId" TEXT NOT NULL,
    	"PivotName" TEXT NOT NULL,
    	"ParentAgentId" TEXT NOT NULL,
    	"ChildAgentId" TEXT NOT NULL
    );`
	_, err = dbms.database.Exec(createTableQuery)

	return err
}

func (dbms *DBMS) DatabaseExists() bool {
	return dbms.exists
}
