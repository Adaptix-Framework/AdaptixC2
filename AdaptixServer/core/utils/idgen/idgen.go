package idgen

import (
	"database/sql"
	"fmt"
	"log"
	"sync/atomic"
)

type Generator struct {
	db       *sql.DB
	counters map[string]*atomic.Int64
}

func New(entities ...string) *Generator {
	g := &Generator{
		counters: make(map[string]*atomic.Int64, len(entities)),
	}
	for _, e := range entities {
		g.counters[e] = &atomic.Int64{}
	}
	return g
}

func (g *Generator) Next(entity string) int64 {
	c, ok := g.counters[entity]
	if !ok {
		panic(fmt.Sprintf("idgen: unknown entity %q", entity))
	}
	next := c.Add(1)

	if g.db != nil {
		if _, err := g.db.Exec(`UPDATE "IdCounters" SET "Value" = ? WHERE "Entity" = ?`, next, entity); err != nil {
			log.Printf("idgen: failed to persist counter for %q: %v", entity, err)
		}
	}
	return next
}

func (g *Generator) Current(entity string) int64 {
	c, ok := g.counters[entity]
	if !ok {
		return 0
	}
	return c.Load()
}

func (g *Generator) Bind(db *sql.DB) error {
	_, err := db.Exec(`CREATE TABLE IF NOT EXISTS "IdCounters" ( "Entity" TEXT PRIMARY KEY, "Value"  INTEGER NOT NULL DEFAULT 0 );`)
	if err != nil {
		return err
	}
	g.db = db

	for entity, c := range g.counters {
		_, _ = db.Exec(`INSERT OR IGNORE INTO "IdCounters"("Entity", "Value") VALUES(?, 0);`, entity)

		var stored sql.NullInt64
		_ = db.QueryRow(`SELECT "Value" FROM "IdCounters" WHERE "Entity" = ?`, entity).Scan(&stored)
		if stored.Valid {
			c.Store(stored.Int64)
		}
	}
	return nil
}
