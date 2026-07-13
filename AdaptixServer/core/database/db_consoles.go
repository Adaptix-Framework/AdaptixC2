package database

import (
	"database/sql"
	"encoding/json"
	"errors"
	"strings"

	"github.com/Adaptix-Framework/axc2/v2"
)

func (dbms *DBMS) DbConsoleInsert(agentId int64, client string, packet interface{}) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	data, err := json.Marshal(packet)
	if err != nil {
		return err
	}

	dbms.enqueueBatchWrite(`INSERT INTO Consoles (AgentId, Client, Packet) values(?,?,?);`, agentId, client, data)
	return nil
}

func (dbms *DBMS) DbConsoleDelete(agentId int64) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	deleteQuery := `DELETE FROM Consoles WHERE AgentId = ?;`
	_, err := dbms.database.Exec(deleteQuery, agentId)

	return err
}

func (dbms *DBMS) DbConsoleAll(agentId int64) [][]byte {
	var consoles [][]byte

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT Packet FROM Consoles WHERE AgentId = ? ORDER BY Id;`
		query, err := dbms.database.Query(selectQuery, agentId)
		if err == nil {
			defer query.Close()
			for query.Next() {
				var message []byte
				err = query.Scan(&message)
				if err != nil {
					continue
				}
				consoles = append(consoles, message)
			}
		} else {
			dbms.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server:database", "Failed to query consoles: %s", err.Error())
		}
	}
	return consoles
}

func (dbms *DBMS) DbConsoleLimited(agentId int64, limit int) [][]byte {
	var consoles [][]byte

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT Packet FROM Consoles WHERE AgentId = ? ORDER BY Id DESC LIMIT ?;`
		query, err := dbms.database.Query(selectQuery, agentId, limit)
		if err == nil {
			defer query.Close()
			for query.Next() {
				var message []byte
				err = query.Scan(&message)
				if err != nil {
					continue
				}
				consoles = append(consoles, message)
			}
		} else {
			dbms.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server:database", "Failed to query consoles: %s", err.Error())
		}
	}
	for i, j := 0, len(consoles)-1; i < j; i, j = i+1, j-1 {
		consoles[i], consoles[j] = consoles[j], consoles[i]
	}
	return consoles
}

func (dbms *DBMS) DbConsoleGetPage(agentId int64, afterId int64, limit int, client string, teamMode bool) ([][]byte, int, int64, error) {
	if !dbms.DatabaseExists() {
		return nil, 0, 0, errors.New("database does not exist")
	}

	var total int
	var rows *sql.Rows
	var err error

	if teamMode || client == "" {
		if err = dbms.database.QueryRow(`SELECT COUNT(*) FROM Consoles WHERE AgentId = ?;`, agentId).Scan(&total); err != nil {
			return nil, 0, 0, err
		}
		if afterId > 0 {
			rows, err = dbms.database.Query(`SELECT Id, Packet FROM Consoles WHERE AgentId = ? AND Id < ? ORDER BY Id DESC LIMIT ?;`, agentId, afterId, limit)
		} else {
			rows, err = dbms.database.Query(`SELECT Id, Packet FROM Consoles WHERE AgentId = ? ORDER BY Id DESC LIMIT ?;`, agentId, limit)
		}
	} else {
		if err = dbms.database.QueryRow(`SELECT COUNT(*) FROM Consoles WHERE AgentId = ? AND (Client = '' OR Client = ?);`, agentId, client).Scan(&total); err != nil {
			return nil, 0, 0, err
		}
		if afterId > 0 {
			rows, err = dbms.database.Query(`SELECT Id, Packet FROM Consoles WHERE AgentId = ? AND (Client = '' OR Client = ?) AND Id < ? ORDER BY Id DESC LIMIT ?;`, agentId, client, afterId, limit)
		} else {
			rows, err = dbms.database.Query(`SELECT Id, Packet FROM Consoles WHERE AgentId = ? AND (Client = '' OR Client = ?) ORDER BY Id DESC LIMIT ?;`, agentId, client, limit)
		}
	}

	if err != nil {
		return nil, 0, 0, err
	}
	defer func(r *sql.Rows) { _ = r.Close() }(rows)

	type idPacket struct {
		id   int64
		data []byte
	}
	var rawItems []idPacket
	for rows.Next() {
		var id int64
		var message []byte
		if err := rows.Scan(&id, &message); err != nil {
			continue
		}
		rawItems = append(rawItems, idPacket{id, message})
	}

	for i, j := 0, len(rawItems)-1; i < j; i, j = i+1, j-1 {
		rawItems[i], rawItems[j] = rawItems[j], rawItems[i]
	}

	var items [][]byte
	var oldestId int64
	for _, ip := range rawItems {
		items = append(items, ip.data)
	}
	if len(rawItems) > 0 {
		oldestId = rawItems[0].id
	}

	return items, total, oldestId, nil
}

func (dbms *DBMS) DbConsoleCount(agentId int64) int {
	ok := dbms.DatabaseExists()
	if !ok {
		return 0
	}

	var count int
	selectQuery := `SELECT COUNT(*) FROM Consoles WHERE AgentId = ?;`
	err := dbms.database.QueryRow(selectQuery, agentId).Scan(&count)
	if err != nil {
		return 0
	}
	return count
}

type ConsoleSearchHit struct {
	Id      int64
	Packet  []byte
	Snippet string
}

func (dbms *DBMS) DbConsoleSearch(agentId int64, query string, limit, offset int, client string, teamMode bool) ([]ConsoleSearchHit, int, error) {
	if !dbms.DatabaseExists() {
		return nil, 0, errors.New("database does not exist")
	}
	query = strings.TrimSpace(query)
	if query == "" {
		return nil, 0, nil
	}
	if limit <= 0 {
		limit = 50
	}
	if limit > 200 {
		limit = 200
	}
	if offset < 0 {
		offset = 0
	}

	var (
		rows *sql.Rows
		err  error
	)
	if teamMode || client == "" {
		rows, err = dbms.database.Query(
			`SELECT Id, Packet FROM Consoles WHERE AgentId = ? ORDER BY Id DESC;`,
			agentId,
		)
	} else {
		rows, err = dbms.database.Query(
			`SELECT Id, Packet FROM Consoles WHERE AgentId = ? AND (Client = '' OR Client = ?) ORDER BY Id DESC;`,
			agentId, client,
		)
	}
	if err != nil {
		return nil, 0, err
	}
	defer func(r *sql.Rows) { _ = r.Close() }(rows)

	needle := strings.ToLower(query)
	var matched []ConsoleSearchHit
	for rows.Next() {
		var h ConsoleSearchHit
		if err := rows.Scan(&h.Id, &h.Packet); err != nil {
			continue
		}
		if !consolePacketMatches(h.Packet, needle) {
			continue
		}
		h.Snippet = consoleMatchLine(h.Packet, query, 160)
		matched = append(matched, h)
	}
	if err := rows.Err(); err != nil {
		return nil, 0, err
	}

	total := len(matched)
	if offset >= total {
		return []ConsoleSearchHit{}, total, nil
	}
	end := offset + limit
	if end > total {
		end = total
	}
	return matched[offset:end], total, nil
}

func consolePacketMatches(packet []byte, needleLower string) bool {
	if needleLower == "" {
		return false
	}
	var obj map[string]interface{}
	if json.Unmarshal(packet, &obj) != nil {
		return strings.Contains(strings.ToLower(string(packet)), needleLower)
	}
	for _, key := range []string{"a_cmdline", "a_message", "a_text", "a_client"} {
		if v, ok := obj[key]; ok {
			if s, ok := v.(string); ok && s != "" {
				if strings.Contains(strings.ToLower(s), needleLower) {
					return true
				}
			}
		}
	}
	return false
}

func (dbms *DBMS) DbConsoleGetAround(agentId int64, centerId int64, limit int, client string, teamMode bool) ([][]byte, int64, error) {
	if !dbms.DatabaseExists() {
		return nil, 0, errors.New("database does not exist")
	}
	if limit <= 0 {
		limit = 50
	}
	if limit > 500 {
		limit = 500
	}
	half := limit / 2
	if half < 1 {
		half = 1
	}

	type idPacket struct {
		id   int64
		data []byte
	}

	load := func(query string, args ...interface{}) ([]idPacket, error) {
		r, err := dbms.database.Query(query, args...)
		if err != nil {
			return nil, err
		}
		defer func() { _ = r.Close() }()
		var out []idPacket
		for r.Next() {
			var ip idPacket
			if err := r.Scan(&ip.id, &ip.data); err != nil {
				continue
			}
			out = append(out, ip)
		}
		return out, nil
	}

	var older, newer []idPacket
	var err error
	if teamMode || client == "" {
		older, err = load(
			`SELECT Id, Packet FROM Consoles WHERE AgentId = ? AND Id <= ? ORDER BY Id DESC LIMIT ?;`,
			agentId, centerId, half+1,
		)
		if err != nil {
			return nil, 0, err
		}
		newer, err = load(
			`SELECT Id, Packet FROM Consoles WHERE AgentId = ? AND Id > ? ORDER BY Id ASC LIMIT ?;`,
			agentId, centerId, half,
		)
	} else {
		older, err = load(
			`SELECT Id, Packet FROM Consoles WHERE AgentId = ? AND (Client = '' OR Client = ?) AND Id <= ? ORDER BY Id DESC LIMIT ?;`,
			agentId, client, centerId, half+1,
		)
		if err != nil {
			return nil, 0, err
		}
		newer, err = load(
			`SELECT Id, Packet FROM Consoles WHERE AgentId = ? AND (Client = '' OR Client = ?) AND Id > ? ORDER BY Id ASC LIMIT ?;`,
			agentId, client, centerId, half,
		)
	}
	if err != nil {
		return nil, 0, err
	}

	for i, j := 0, len(older)-1; i < j; i, j = i+1, j-1 {
		older[i], older[j] = older[j], older[i]
	}

	merged := append(older, newer...)
	items := make([][]byte, 0, len(merged))
	var oldestId int64
	for i, ip := range merged {
		items = append(items, ip.data)
		if i == 0 {
			oldestId = ip.id
		}
	}
	return items, oldestId, nil
}

func consoleMatchLine(packet []byte, query string, maxLen int) string {
	q := strings.TrimSpace(query)
	if q == "" {
		return ""
	}
	qlow := strings.ToLower(q)

	splitLines := func(s string) []string {
		if s == "" {
			return nil
		}
		s = strings.ReplaceAll(s, "\r\n", "\n")
		s = strings.ReplaceAll(s, "\r", "\n")
		s = strings.ReplaceAll(s, "\\n", "\n")
		s = strings.ReplaceAll(s, "\\r", "\n")
		raw := strings.Split(s, "\n")
		out := make([]string, 0, len(raw))
		for _, line := range raw {
			line = strings.TrimSpace(line)
			if line != "" {
				out = append(out, line)
			}
		}
		return out
	}

	var lines []string
	var obj map[string]interface{}
	if json.Unmarshal(packet, &obj) == nil {
		for _, key := range []string{"a_cmdline", "a_message", "a_text", "a_client"} {
			if v, ok := obj[key]; ok {
				if s, ok := v.(string); ok && s != "" {
					lines = append(lines, splitLines(s)...)
				}
			}
		}
	}
	if len(lines) == 0 {
		lines = splitLines(string(packet))
	}

	pick := ""
	for _, line := range lines {
		if strings.Contains(strings.ToLower(line), qlow) {
			pick = line
			break
		}
	}
	if pick == "" {
		return ""
	}

	pick = strings.ReplaceAll(pick, "\n", " ")
	pick = strings.ReplaceAll(pick, "\r", " ")
	pick = strings.Join(strings.Fields(pick), " ")

	if maxLen <= 0 {
		return pick
	}
	rs := []rune(pick)
	if len(rs) <= maxLen {
		return pick
	}

	low := strings.ToLower(pick)
	idx := strings.Index(low, qlow)
	start := 0
	if idx > maxLen/3 {
		start = idx - maxLen/3
		if start < 0 {
			start = 0
		}
		if start > len(rs) {
			start = 0
		}
	}
	if start+maxLen > len(rs) {
		start = len(rs) - maxLen
		if start < 0 {
			start = 0
		}
	}
	end := start + maxLen
	if end > len(rs) {
		end = len(rs)
	}
	snip := string(rs[start:end])
	if start > 0 {
		snip = "…" + snip
	}
	if end < len(rs) {
		snip = snip + "…"
	}
	return snip
}
