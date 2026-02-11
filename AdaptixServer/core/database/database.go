package database

import (
	"database/sql"

	_ "github.com/mattn/go-sqlite3"
)

type DBMS struct {
	database *sql.DB
	exists   bool

	stmtAgentTick     *sql.Stmt
	stmtConsoleInsert *sql.Stmt
	stmtTaskInsert    *sql.Stmt
	stmtTaskUpdate    *sql.Stmt
}

func NewDatabase(dbPath string) (*DBMS, error) {
	var err error

	dbms := &DBMS{
		exists: true,
	}

	// Enable WAL mode and other performance optimizations via connection string
	connStr := dbPath + "?_journal_mode=WAL&_synchronous=NORMAL&_busy_timeout=10000&_cache_size=-64000"
	dbms.database, err = sql.Open("sqlite3", connStr)
	if err != nil {
		dbms.exists = false
	}

	if dbms.exists {
		dbms.database.SetMaxOpenConns(1)
		dbms.database.SetMaxIdleConns(1)

		err = dbms.DatabaseInit()
		if err != nil {
			dbms.exists = false
		}
	}

	if dbms.exists {
		dbms.prepareStatements()
	}

	return dbms, err
}

func (dbms *DBMS) prepareStatements() {
	var err error

	dbms.stmtAgentTick, err = dbms.database.Prepare(`UPDATE Agents SET LastTick = ? WHERE Id = ?;`)
	if err != nil {
		dbms.stmtAgentTick = nil
	}

	dbms.stmtConsoleInsert, err = dbms.database.Prepare(`INSERT INTO Consoles (AgentId, Packet) values(?,?);`)
	if err != nil {
		dbms.stmtConsoleInsert = nil
	}

	dbms.stmtTaskInsert, err = dbms.database.Prepare(`INSERT OR IGNORE INTO Tasks (TaskId, AgentId, TaskType, Client, User, Computer, StartDate, FinishDate, CommandLine, MessageType, Message, ClearText, Completed) values(?,?,?,?,?,?,?,?,?,?,?,?,?);`)
	if err != nil {
		dbms.stmtTaskInsert = nil
	}

	dbms.stmtTaskUpdate, err = dbms.database.Prepare(`UPDATE Tasks SET FinishDate = ?, MessageType = ?, Message = ?, ClearText = ?, Completed = ? WHERE TaskId = ?;`)
	if err != nil {
		dbms.stmtTaskUpdate = nil
	}
}

func (dbms *DBMS) DatabaseInit() error {
	var (
		err              error
		createTableQuery string
	)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Listeners" (
    	"ListenerName" TEXT NOT NULL UNIQUE, 
    	"ListenerRegName" TEXT NOT NULL,
    	"ListenerConfig" TEXT NOT NULL,
		"ListenerStatus" TEXT,
    	"CreateTime" BIGINT,
    	"Watermark" TEXT NOT NULL,
    	"CustomData" BLOB
    );`
	_, err = dbms.database.Exec(createTableQuery)

	// TODO CLEAR: Soft migration for old databases
	_, _ = dbms.database.Exec(`ALTER TABLE "Listeners" ADD COLUMN "ListenerStatus" TEXT;`)
	_, _ = dbms.database.Exec(`UPDATE "Listeners" SET "ListenerStatus" = 'Listen' WHERE "ListenerStatus" IS NULL;`)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Chat" (
    	"Id" INTEGER PRIMARY KEY AUTOINCREMENT,
    	"Username" TEXT NOT NULL,
    	"Message" TEXT NOT NULL,
    	"Date" BIGINT
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

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Screenshots" (
    	"ScreenId" TEXT NOT NULL UNIQUE, 
    	"User" TEXT NOT NULL,
    	"Computer" TEXT NOT NULL,
    	"LocalPath" TEXT NOT NULL,
    	"Note" TEXT,
    	"Date" BIGINT
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
    	"WorkingTime" INTEGER,	
    	"KillDate" INTEGER,
    	"Tags" TEXT,
    	"Mark" TEXT,
    	"Color" TEXT,
    	"TargetId" TEXT,
    	"CustomData" BLOB
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

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Credentials" (
		"Id" INTEGER PRIMARY KEY AUTOINCREMENT,
    	"CredId" TEXT NOT NULL,
    	"Username" TEXT,
    	"Password" TEXT,
    	"Realm" TEXT,
    	"Type" TEXT,
    	"Tag" TEXT,
    	"Date" BIGINT,
    	"Storage" TEXT,
		"AgentId" TEXT,
		"Host" TEXT
    );`
	_, err = dbms.database.Exec(createTableQuery)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Targets" (
		"Id" INTEGER PRIMARY KEY AUTOINCREMENT,
    	"TargetId" TEXT NOT NULL,
    	"Computer" TEXT,
    	"Domain" TEXT,
    	"Address" TEXT,
    	"Os" INTEGER,
    	"OsDesk" TEXT,
    	"Tag" TEXT,
    	"Info" TEXT,
    	"Date" BIGINT,
		"Alive" BOOLEAN,
		"Agents" TEXT
    );`
	_, err = dbms.database.Exec(createTableQuery)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "ExtenderData" (
		"Id" INTEGER PRIMARY KEY AUTOINCREMENT,
    	"ExtenderName" TEXT NOT NULL,
    	"Key" TEXT NOT NULL,
    	"Value" BLOB,
		UNIQUE("ExtenderName", "Key")
    );`
	_, err = dbms.database.Exec(createTableQuery)

	indexQueries := []string{
		`CREATE INDEX IF NOT EXISTS idx_tasks_agentid ON Tasks(AgentId);`,
		`CREATE INDEX IF NOT EXISTS idx_tasks_startdate ON Tasks(StartDate);`,
		`CREATE INDEX IF NOT EXISTS idx_tasks_completed ON Tasks(Completed);`,
		`CREATE INDEX IF NOT EXISTS idx_consoles_agentid ON Consoles(AgentId);`,
		`CREATE INDEX IF NOT EXISTS idx_downloads_agentid ON Downloads(AgentId);`,
		`CREATE INDEX IF NOT EXISTS idx_downloads_date ON Downloads(Date);`,
		`CREATE INDEX IF NOT EXISTS idx_screenshots_date ON Screenshots(Date);`,
		`CREATE INDEX IF NOT EXISTS idx_credentials_agentid ON Credentials(AgentId);`,
		`CREATE INDEX IF NOT EXISTS idx_credentials_credid ON Credentials(CredId);`,
		`CREATE INDEX IF NOT EXISTS idx_credentials_dup ON Credentials(Username, Realm, Password);`,
		`CREATE INDEX IF NOT EXISTS idx_targets_targetid ON Targets(TargetId);`,
		`CREATE INDEX IF NOT EXISTS idx_targets_address ON Targets(Address);`,
		`CREATE INDEX IF NOT EXISTS idx_targets_computer_domain ON Targets(Computer, Domain);`,
		`CREATE INDEX IF NOT EXISTS idx_pivots_parentagentid ON Pivots(ParentAgentId);`,
		`CREATE INDEX IF NOT EXISTS idx_pivots_childagentid ON Pivots(ChildAgentId);`,
	}
	for _, indexQuery := range indexQueries {
		_, _ = dbms.database.Exec(indexQuery)
	}

	return err
}

func (dbms *DBMS) DatabaseExists() bool {
	return dbms.exists
}

func (dbms *DBMS) DbTableCount(table string) int {
	if !dbms.exists {
		return 0
	}
	allowed := map[string]bool{
		"Screenshots": true, "Tasks": true, "Consoles": true,
		"Downloads": true, "Credentials": true, "Targets": true, "Chat": true,
	}
	if !allowed[table] {
		return 0
	}
	var count int
	_ = dbms.database.QueryRow("SELECT COUNT(*) FROM " + table).Scan(&count)
	return count
}
