package database

import (
	"AdaptixServer/core/utils/filter"
	"database/sql"
	"errors"
	"fmt"

	"github.com/Adaptix-Framework/axc2/v2"
)

func (dbms *DBMS) DbTaskExist(taskId int64) bool {
	var id string
	err := dbms.database.QueryRow("SELECT TaskId FROM Tasks WHERE TaskId = ? LIMIT 1;", taskId).Scan(&id)
	return err == nil
}

func (dbms *DBMS) DbTaskInsert(taskData adaptix.TaskData) error {
	if !dbms.DatabaseExists() {
		return errors.New("database does not exist")
	}

	dbms.enqueueBatchWrite(
		`INSERT OR IGNORE INTO Tasks (TaskId, AgentId, TaskType, Client, User, Computer, StartDate, FinishDate, CommandLine, MessageType, Message, ClearText, Completed) values(?,?,?,?,?,?,?,?,?,?,?,?,?);`,
		taskData.TaskId, taskData.AgentId, taskData.Type, taskData.Client, taskData.User, taskData.Computer, taskData.StartDate,
		taskData.FinishDate, taskData.CommandLine, taskData.MessageType, taskData.Message, taskData.ClearText, taskData.Completed,
	)
	return nil
}

func (dbms *DBMS) DbTaskUpdate(taskData adaptix.TaskData) error {
	if !dbms.DatabaseExists() {
		return errors.New("database does not exist")
	}

	dbms.enqueueBatchWrite(
		`UPDATE Tasks SET FinishDate = ?, MessageType = ?, Message = ?, ClearText = ?, Completed = ? WHERE TaskId = ?;`,
		taskData.FinishDate, taskData.MessageType, taskData.Message, taskData.ClearText, taskData.Completed, taskData.TaskId,
	)
	return nil
}

func (dbms *DBMS) DbTaskMarkDispatched(taskId int64) error {
	if !dbms.DatabaseExists() {
		return errors.New("database does not exist")
	}
	dbms.enqueueBatchWrite(
		`UPDATE Tasks SET Dispatched = 1 WHERE TaskId = ?;`,
		taskId,
	)
	return nil
}

func (dbms *DBMS) DbTaskDelete(taskId int64, agentId int64) error {
	var err error

	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	if taskId != 0 {
		deleteQuery := `DELETE FROM Tasks WHERE TaskId = ?;`
		_, err = dbms.database.Exec(deleteQuery, taskId)
	} else if agentId != 0 {
		deleteQuery := `DELETE FROM Tasks WHERE AgentId = ?;`
		_, err = dbms.database.Exec(deleteQuery, agentId)
	}

	return err
}

func (dbms *DBMS) DbTaskGet(taskId int64) (adaptix.TaskData, error) {
	var taskData adaptix.TaskData

	if !dbms.DatabaseExists() {
		return taskData, errors.New("database does not exist")
	}

	selectQuery := `SELECT TaskId, AgentId, TaskType, Client, User, Computer, StartDate, FinishDate, CommandLine, MessageType, Message, ClearText, Completed FROM Tasks WHERE TaskId = ?;`
	err := dbms.database.QueryRow(selectQuery, taskId).Scan(&taskData.TaskId, &taskData.AgentId, &taskData.Type, &taskData.Client, &taskData.User, &taskData.Computer, &taskData.StartDate, &taskData.FinishDate, &taskData.CommandLine, &taskData.MessageType, &taskData.Message, &taskData.ClearText, &taskData.Completed)
	if err != nil {
		return taskData, fmt.Errorf("task %d not found", taskId)
	}
	return taskData, nil
}

func (dbms *DBMS) DbTasksAll(agentId int64) []adaptix.TaskData {
	var tasks []adaptix.TaskData

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT TaskId, AgentId, TaskType, Client, User, Computer, StartDate, FinishDate, CommandLine, MessageType, Message, ClearText, Completed FROM Tasks WHERE AgentId = ? ORDER BY StartDate;`
		query, err := dbms.database.Query(selectQuery, agentId)
		if err != nil {
			dbms.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server", "database", "Failed to query tasks: %s", err.Error())
			return tasks
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)

		for query.Next() {
			taskData := adaptix.TaskData{}
			err = query.Scan(&taskData.TaskId, &taskData.AgentId, &taskData.Type, &taskData.Client, &taskData.User, &taskData.Computer, &taskData.StartDate, &taskData.FinishDate, &taskData.CommandLine, &taskData.MessageType, &taskData.Message, &taskData.ClearText, &taskData.Completed)
			if err != nil {
				continue
			}
			tasks = append(tasks, taskData)
		}
	}
	return tasks
}

func (dbms *DBMS) DbTasksLimited(agentId int64, limit int) []adaptix.TaskData {
	var tasks []adaptix.TaskData

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT TaskId, AgentId, TaskType, Client, User, Computer, StartDate, FinishDate, CommandLine, MessageType, Message, ClearText, Completed FROM Tasks WHERE AgentId = ? ORDER BY StartDate DESC LIMIT ?;`
		query, err := dbms.database.Query(selectQuery, agentId, limit)
		if err != nil {
			dbms.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server", "database", "Failed to query tasks: %s", err.Error())
			return tasks
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)

		for query.Next() {
			taskData := adaptix.TaskData{}
			err = query.Scan(&taskData.TaskId, &taskData.AgentId, &taskData.Type, &taskData.Client, &taskData.User, &taskData.Computer, &taskData.StartDate, &taskData.FinishDate, &taskData.CommandLine, &taskData.MessageType, &taskData.Message, &taskData.ClearText, &taskData.Completed)
			if err != nil {
				continue
			}
			tasks = append(tasks, taskData)
		}
	}
	for i, j := 0, len(tasks)-1; i < j; i, j = i+1, j-1 {
		tasks[i], tasks[j] = tasks[j], tasks[i]
	}
	return tasks
}

var sortableTaskColumns = map[string]string{
	"TaskId":      "TaskId",
	"TaskType":    "TaskType",
	"AgentId":     "AgentId",
	"Client":      "Client",
	"User":        "User",
	"Computer":    "Computer",
	"StartDate":   "StartDate",
	"FinishDate":  "FinishDate",
	"CommandLine": "CommandLine",
	"Completed":   "Completed",
	"Message":     "Message",
}

func (dbms *DBMS) DbTasksGetPage(agentId int64, offset, limit int, filterExpr, sortCol, sortOrder string, completedFilter *bool) ([]adaptix.TaskData, int, error) {
	if !dbms.DatabaseExists() {
		return nil, 0, errors.New("database does not exist")
	}

	where := "TaskType <> 2"
	var args []interface{}

	if agentId != 0 {
		where += " AND AgentId = ?"
		args = append(args, agentId)
	}

	if completedFilter != nil {
		where += " AND Completed = ?"
		if *completedFilter {
			args = append(args, 1)
		} else {
			args = append(args, 0)
		}
	}

	if filterExpr != "" {
		node, err := filter.ParseFilterExpr(filterExpr)
		if err != nil {
			return nil, 0, fmt.Errorf("invalid filter: %w", err)
		}
		if node != nil {
			filterSQL, filterArgs := filter.ToSQL(node, []string{
				"TaskId", "AgentId", "Client", "User", "Computer", "CommandLine", "Message", "ClearText",
				"CASE TaskType WHEN 1 THEN 'task' WHEN 2 THEN 'internal' WHEN 3 THEN 'job' WHEN 4 THEN 'tunnel' END",
			})
			if filterSQL != "" {
				where += " AND " + filterSQL
				args = append(args, filterArgs...)
			}
		}
	}

	orderClause := "StartDate DESC"
	if col, ok := sortableTaskColumns[sortCol]; ok {
		dir := "DESC"
		if sortOrder == "asc" || sortOrder == "ASC" {
			dir = "ASC"
		}
		orderClause = fmt.Sprintf(`"%s" %s`, col, dir)
	}

	var total int
	if err := dbms.database.QueryRow("SELECT COUNT(*) FROM Tasks WHERE "+where, args...).Scan(&total); err != nil {
		return nil, 0, err
	}

	selectArgs := append(append([]interface{}(nil), args...), limit, offset)
	rows, err := dbms.database.Query(
		"SELECT TaskId, AgentId, TaskType, Client, User, Computer, StartDate, FinishDate, CommandLine, MessageType, Message, ClearText, Completed"+
			" FROM Tasks WHERE "+where+" ORDER BY "+orderClause+" LIMIT ? OFFSET ?",
		selectArgs...,
	)
	if err != nil {
		return nil, 0, err
	}
	defer func() { _ = rows.Close() }()

	tasks := make([]adaptix.TaskData, 0, limit)
	for rows.Next() {
		var t adaptix.TaskData
		if err = rows.Scan(&t.TaskId, &t.AgentId, &t.Type, &t.Client, &t.User, &t.Computer, &t.StartDate, &t.FinishDate, &t.CommandLine, &t.MessageType, &t.Message, &t.ClearText, &t.Completed); err != nil {
			continue
		}
		tasks = append(tasks, t)
	}
	return tasks, total, nil
}

func (dbms *DBMS) DbTasksCount(agentId int64) int {
	ok := dbms.DatabaseExists()
	if !ok {
		return 0
	}

	var count int
	selectQuery := `SELECT COUNT(*) FROM Tasks WHERE AgentId = ?;`
	err := dbms.database.QueryRow(selectQuery, agentId).Scan(&count)
	if err != nil {
		return 0
	}
	return count
}
