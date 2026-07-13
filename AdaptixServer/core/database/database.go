package database

import (
	"database/sql"
	"fmt"
	"sync"
	"time"

	"github.com/Adaptix-Framework/axc2/v2"
	_ "github.com/mattn/go-sqlite3"
)

type Teamserver interface {
	TsLogAdd(status adaptix.LogStatus, level int, source string, format string, args ...any)
}

type batchWriteOp struct {
	query string
	args  []interface{}
}

type DBMS struct {
	database *sql.DB
	exists   bool
	ts       Teamserver

	writeChan  chan batchWriteOp
	writerDone chan struct{}
	writerOnce sync.Once
}

func (dbms *DBMS) GetDB() *sql.DB {
	return dbms.database
}

func NewDatabase(dbPath string, ts Teamserver) (*DBMS, error) {
	var err error

	dbms := &DBMS{
		exists: true,
		ts:     ts,
	}

	// Enable WAL mode and other performance optimizations via connection string
	connStr := dbPath + "?_journal_mode=WAL&_synchronous=NORMAL&_busy_timeout=10000&_cache_size=-64000"
	dbms.database, err = sql.Open("sqlite3", connStr)
	if err != nil {
		return nil, err
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
		dbms.startBatchWriter()
	}

	return dbms, err
}

func (dbms *DBMS) startBatchWriter() {
	dbms.writeChan = make(chan batchWriteOp, 8192)
	dbms.writerDone = make(chan struct{})

	go func() {
		defer close(dbms.writerDone)

		ticker := time.NewTicker(200 * time.Millisecond)
		defer ticker.Stop()

		batch := make([]batchWriteOp, 0, 256)

		for {
			select {
			case op, ok := <-dbms.writeChan:
				if !ok {
					goto shutdown
				}
				batch = append(batch, op)

				for len(batch) < 256 {
					select {
					case op2, ok2 := <-dbms.writeChan:
						if !ok2 {
							goto shutdown
						}
						batch = append(batch, op2)
					default:
						goto flush
					}
				}
			flush:
				dbms.flushBatch(batch)
				batch = batch[:0]

			case <-ticker.C:
				if len(batch) > 0 {
					dbms.flushBatch(batch)
					batch = batch[:0]
				}
			}
		}
	shutdown:
		if len(batch) > 0 {
			dbms.flushBatch(batch)
		}
	}()
}

func (dbms *DBMS) flushBatch(batch []batchWriteOp) {
	if len(batch) == 0 {
		return
	}

	tx, err := dbms.database.Begin()
	if err != nil {
		dbms.ts.TsLogAdd(adaptix.LogStatusError, 0, "server:database", "batch writer: BEGIN failed: %v", err)
		return
	}

	for _, op := range batch {
		_, err := tx.Exec(op.query, op.args...)
		if err != nil {
			dbms.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server:database", "batch writer: Exec failed: %v", err)
		}
	}

	if err := tx.Commit(); err != nil {
		dbms.ts.TsLogAdd(adaptix.LogStatusError, 0, "server:database", "batch writer: COMMIT failed: %v", err)
		_ = tx.Rollback()
	}
}

func (dbms *DBMS) StopBatchWriter() {
	dbms.writerOnce.Do(func() {
		if dbms.writeChan != nil {
			close(dbms.writeChan)
			<-dbms.writerDone
		}
	})
}

func (dbms *DBMS) enqueueBatchWrite(query string, args ...interface{}) {
	if dbms.writeChan == nil {
		return
	}
	dbms.writeChan <- batchWriteOp{query: query, args: args}
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
	if err != nil {
		return fmt.Errorf("create table failed: %w", err)
	}

	// TODO CLEAR: Soft migration for old databases
	_, _ = dbms.database.Exec(`ALTER TABLE "Listeners" ADD COLUMN "ListenerStatus" TEXT;`)
	_, _ = dbms.database.Exec(`ALTER TABLE "Listeners" ADD COLUMN "Tags" TEXT DEFAULT '';`)
	_, _ = dbms.database.Exec(`UPDATE "Listeners" SET "ListenerStatus" = 'Listen' WHERE "ListenerStatus" IS NULL;`)
	_, _ = dbms.database.Exec(`ALTER TABLE "Downloads" ADD COLUMN "Tag" TEXT;`)
	_, _ = dbms.database.Exec(`ALTER TABLE "Consoles" ADD COLUMN "Client" TEXT;`)
	_, _ = dbms.database.Exec(`CREATE INDEX IF NOT EXISTS idx_consoles_agent_client ON Consoles(AgentId, Client);`)
	_, _ = dbms.database.Exec(`ALTER TABLE "Screenshots" ADD COLUMN "AgentId" INTEGER NOT NULL DEFAULT 0;`)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Chat" (
    	"Id" INTEGER PRIMARY KEY AUTOINCREMENT,
    	"Username" TEXT NOT NULL,
    	"Message" TEXT NOT NULL,
    	"Date" BIGINT
    );`
	_, err = dbms.database.Exec(createTableQuery)
	if err != nil {
		return fmt.Errorf("create table failed: %w", err)
	}

	_, _ = dbms.database.Exec(`ALTER TABLE "Chat" ADD COLUMN "Edited" BOOLEAN DEFAULT FALSE;`)
	_, _ = dbms.database.Exec(`ALTER TABLE "Chat" ADD COLUMN "Deleted" BOOLEAN DEFAULT FALSE;`)
	_, _ = dbms.database.Exec(`ALTER TABLE "Chat" ADD COLUMN "Reactions" TEXT DEFAULT '{}';`)
	_, _ = dbms.database.Exec(`ALTER TABLE "Chat" ADD COLUMN "ReplyToId" BIGINT DEFAULT 0;`)
	_, _ = dbms.database.Exec(`ALTER TABLE "Chat" ADD COLUMN "ReplyToName" TEXT DEFAULT '';`)
	_, _ = dbms.database.Exec(`ALTER TABLE "Chat" ADD COLUMN "DeletedDate" BIGINT DEFAULT 0;`)
	_, _ = dbms.database.Exec(`CREATE INDEX IF NOT EXISTS idx_chat_id ON Chat(Id);`)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "ChatTodo" (
		"Id"        INTEGER PRIMARY KEY,
		"Content"   TEXT NOT NULL DEFAULT '',
		"UpdatedBy" TEXT NOT NULL DEFAULT '',
		"UpdatedAt" BIGINT DEFAULT 0
	);`
	_, err = dbms.database.Exec(createTableQuery)
	if err != nil {
		return fmt.Errorf("create table failed: %w", err)
	}
	_, _ = dbms.database.Exec(`INSERT OR IGNORE INTO ChatTodo(Id, Content, UpdatedBy, UpdatedAt) VALUES(1, '', '', 0);`)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Downloads" (
    	"FileId" INTEGER NOT NULL UNIQUE, 
    	"AgentId" INTEGER NOT NULL,
    	"AgentName" TEXT NOT NULL,
    	"User" TEXT NOT NULL,
    	"Computer" TEXT NOT NULL,
    	"RemotePath" TEXT NOT NULL,
    	"LocalPath" TEXT NOT NULL,
    	"TotalSize" BIGINT,
    	"RecvSize" BIGINT,
    	"Date" BIGINT,
    	"State" INTEGER,
    	"Tag" TEXT
    );`
	_, err = dbms.database.Exec(createTableQuery)
	if err != nil {
		return fmt.Errorf("create table failed: %w", err)
	}

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Uploads" (
    	"FileId" INTEGER NOT NULL UNIQUE,
    	"AgentId" INTEGER NOT NULL,
    	"AgentName" TEXT NOT NULL,
    	"User" TEXT NOT NULL,
    	"Computer" TEXT NOT NULL,
    	"RemotePath" TEXT NOT NULL,
    	"LocalPath" TEXT NOT NULL,
    	"TotalSize" BIGINT,
    	"Progress" BIGINT,
    	"Date" BIGINT,
    	"State" INTEGER,
    	"Tag" TEXT,
    	"Cancellable" INTEGER DEFAULT 1,
    	"Kind" INTEGER DEFAULT 0,
    	"ArtifactName" TEXT,
    	"ArtifactType" TEXT
    );`
	_, err = dbms.database.Exec(createTableQuery)
	if err != nil {
		return fmt.Errorf("create table failed: %w", err)
	}

	_, _ = dbms.database.Exec(
		`UPDATE Uploads SET State = ? WHERE State = ?;`,
		adaptix.TRANSFER_STATE_CANCELED, adaptix.TRANSFER_STATE_RUNNING,
	)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Screenshots" (
    	"ScreenId" INTEGER NOT NULL UNIQUE,
    	"AgentId" INTEGER NOT NULL DEFAULT 0,
    	"User" TEXT NOT NULL,
    	"Computer" TEXT NOT NULL,
    	"LocalPath" TEXT NOT NULL,
    	"Note" TEXT,
    	"Date" BIGINT
    );`
	_, err = dbms.database.Exec(createTableQuery)
	if err != nil {
		return fmt.Errorf("create table failed: %w", err)
	}

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Agents" (
    	"Id" INTEGER NOT NULL UNIQUE, 
    	"Crc" TEXT NOT NULL,
    	"UID" BLOB,
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
    	"TargetId" INTEGER,
    	"CustomData" BLOB
    );`
	_, err = dbms.database.Exec(createTableQuery)
	if err != nil {
		return fmt.Errorf("create table failed: %w", err)
	}

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Tasks" (
    	"TaskId" INTEGER NOT NULL UNIQUE, 
    	"AgentId" INTEGER NOT NULL,
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
    	"Completed" INTEGER,
    	"Dispatched" INTEGER DEFAULT 0
    );`
	_, err = dbms.database.Exec(createTableQuery)
	if err != nil {
		return fmt.Errorf("create table failed: %w", err)
	}

	_, _ = dbms.database.Exec(`ALTER TABLE "Tasks" ADD COLUMN "Dispatched" INTEGER DEFAULT 0;`)

	_, _ = dbms.database.Exec(`DELETE FROM Tasks WHERE TaskType = 2;`)

	_, _ = dbms.database.Exec(
		`UPDATE Tasks SET Completed = 1, FinishDate = ?, MessageType = 6, Message = ? `+
			`WHERE Completed = 0 AND Dispatched = 0;`,
		time.Now().Unix(),
		"lost on server restart before delivery",
	)

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Consoles" (
		"Id" INTEGER PRIMARY KEY AUTOINCREMENT,
    	"AgentId" INTEGER NOT NULL,
    	"Packet" BLOB
    );`
	_, err = dbms.database.Exec(createTableQuery)
	if err != nil {
		return fmt.Errorf("create table failed: %w", err)
	}

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Pivots" (
		"Id" INTEGER PRIMARY KEY AUTOINCREMENT,
    	"PivotId" TEXT NOT NULL,
    	"PivotName" TEXT NOT NULL,
    	"ParentAgentId" INTEGER NOT NULL,
    	"ChildAgentId" INTEGER NOT NULL
    );`
	_, err = dbms.database.Exec(createTableQuery)
	if err != nil {
		return fmt.Errorf("create table failed: %w", err)
	}

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Credentials" (
		"Id" INTEGER PRIMARY KEY AUTOINCREMENT,
    	"CredId" INTEGER NOT NULL,
    	"Username" TEXT,
    	"Password" TEXT,
    	"Realm" TEXT,
    	"Type" TEXT,
    	"Tag" TEXT,
    	"Date" BIGINT,
    	"Storage" TEXT,
		"AgentId" INTEGER,
		"Host" TEXT
    );`
	_, err = dbms.database.Exec(createTableQuery)
	if err != nil {
		return fmt.Errorf("create table failed: %w", err)
	}

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Targets" (
		"Id" INTEGER PRIMARY KEY AUTOINCREMENT,
    	"TargetId" INTEGER NOT NULL,
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
	if err != nil {
		return fmt.Errorf("create table failed: %w", err)
	}

	createTableQuery = `CREATE TABLE IF NOT EXISTS "ExtenderData" (
		"Id" INTEGER PRIMARY KEY AUTOINCREMENT,
    	"ExtenderName" TEXT NOT NULL,
    	"Key" TEXT NOT NULL,
    	"Value" BLOB,
		UNIQUE("ExtenderName", "Key")
    );`
	_, err = dbms.database.Exec(createTableQuery)
	if err != nil {
		return fmt.Errorf("create table failed: %w", err)
	}

	createTableQuery = `CREATE TABLE IF NOT EXISTS "Groups" (
		"GroupId" INTEGER PRIMARY KEY AUTOINCREMENT,
		"ParentGroupId" INTEGER NOT NULL DEFAULT 0,
		"GroupName" TEXT NOT NULL,
		"Scope" TEXT NOT NULL,
		"Members" TEXT NOT NULL DEFAULT ''
    );`
	_, err = dbms.database.Exec(createTableQuery)
	if err != nil {
		return fmt.Errorf("create table failed: %w", err)
	}

	indexQueries := []string{
		`CREATE INDEX IF NOT EXISTS idx_tasks_agentid ON Tasks(AgentId);`,
		`CREATE INDEX IF NOT EXISTS idx_tasks_startdate ON Tasks(StartDate);`,
		`CREATE INDEX IF NOT EXISTS idx_tasks_completed ON Tasks(Completed);`,

		// WHERE AgentId = ? ORDER BY StartDate DESC LIMIT ? OFFSET ?
		`CREATE INDEX IF NOT EXISTS idx_tasks_agent_date ON Tasks(AgentId, StartDate);`,
		// WHERE AgentId = ? AND Completed = ? ORDER BY StartDate DESC LIMIT ? OFFSET ?
		`CREATE INDEX IF NOT EXISTS idx_tasks_agent_completed_date ON Tasks(AgentId, Completed, StartDate);`,

		// WHERE AgentId = ?
		`CREATE INDEX IF NOT EXISTS idx_consoles_agentid ON Consoles(AgentId);`,
		// WHERE AgentId = ? ORDER BY Id DESC LIMIT ? OFFSET ?  (Id — autoincrement)
		`CREATE INDEX IF NOT EXISTS idx_consoles_agent_id ON Consoles(AgentId, Id);`,

		`CREATE INDEX IF NOT EXISTS idx_downloads_agentid ON Downloads(AgentId);`,
		// ORDER BY Date DESC LIMIT ? OFFSET ?
		`CREATE INDEX IF NOT EXISTS idx_downloads_date ON Downloads(Date);`,

		// ORDER BY Date DESC LIMIT ? OFFSET ?
		`CREATE INDEX IF NOT EXISTS idx_screenshots_date ON Screenshots(Date);`,

		`CREATE INDEX IF NOT EXISTS idx_credentials_agentid ON Credentials(AgentId);`,
		`CREATE INDEX IF NOT EXISTS idx_credentials_credid ON Credentials(CredId);`,
		`CREATE INDEX IF NOT EXISTS idx_credentials_dup ON Credentials(Username, Realm, Password);`,
		// ORDER BY Date DESC LIMIT ? OFFSET ?
		`CREATE INDEX IF NOT EXISTS idx_credentials_date ON Credentials(Date);`,

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
		"Screenshots": true,
		"Tasks":       true,
		"Consoles":    true,
		"Downloads":   true,
		"Credentials": true,
		"Targets":     true,
		"Chat":        true,
	}
	if !allowed[table] {
		return 0
	}
	var count int
	_ = dbms.database.QueryRow("SELECT COUNT(*) FROM " + table).Scan(&count)
	return count
}
