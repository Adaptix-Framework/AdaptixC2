package database

import (
	"AdaptixServer/core/utils/logs"
	"database/sql"
	"errors"
	"fmt"

	"github.com/Adaptix-Framework/axc2"
)

func (dbms *DBMS) DbTaskExist(taskId string) bool {
	rows, err := dbms.database.Query("SELECT TaskId FROM Tasks WHERE TaskId = ?;", taskId)
	if err != nil {
		return false
	}
	defer func(rows *sql.Rows) {
		_ = rows.Close()
	}(rows)

	return rows.Next()
}

func (dbms *DBMS) DbTaskInsert(taskData adaptix.TaskData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	ok = dbms.DbTaskExist(taskData.TaskId)
	if ok {
		return fmt.Errorf("task %s already exists", taskData.TaskId)
	}

	insertQuery := `INSERT INTO Tasks (TaskId, AgentId, TaskType, Client, User, Computer, StartDate, FinishDate, CommandLine, MessageType, Message, ClearText, Completed) values(?,?,?,?,?,?,?,?,?,?,?,?,?);`
	_, err := dbms.database.Exec(insertQuery, taskData.TaskId, taskData.AgentId, taskData.Type, taskData.Client, taskData.User, taskData.Computer, taskData.StartDate, taskData.FinishDate, taskData.CommandLine, taskData.MessageType, taskData.Message, taskData.ClearText, taskData.Completed)
	return err
}

func (dbms *DBMS) DbTaskUpdate(taskData adaptix.TaskData) error {
	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	ok = dbms.DbTaskExist(taskData.TaskId)
	if !ok {
		return fmt.Errorf("task %s does not exist", taskData.TaskId)
	}

	updateQuery := `UPDATE Tasks SET FinishDate = ?, MessageType = ?, Message = ?, ClearText = ?, Completed = ? WHERE TaskId = ?;`
	_, err := dbms.database.Exec(updateQuery, taskData.FinishDate, taskData.MessageType, taskData.Message, taskData.ClearText, taskData.Completed, taskData.TaskId)
	return err
}

func (dbms *DBMS) DbTaskDelete(taskId string, agentId string) error {
	var err error

	ok := dbms.DatabaseExists()
	if !ok {
		return errors.New("database does not exist")
	}

	if taskId != "" {
		deleteQuery := `DELETE FROM Tasks WHERE TaskId = ?;`
		_, err = dbms.database.Exec(deleteQuery, taskId)
	} else if agentId != "" {
		deleteQuery := `DELETE FROM Tasks WHERE AgentId = ?;`
		_, err = dbms.database.Exec(deleteQuery, agentId)
	}

	return err
}

func (dbms *DBMS) DbTasksAll(agentId string) []adaptix.TaskData {
	var tasks []adaptix.TaskData

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT TaskId, AgentId, TaskType, Client, User, Computer, StartDate, FinishDate, CommandLine, MessageType, Message, ClearText, Completed FROM Tasks WHERE AgentId = ? ORDER BY StartDate;`
		query, err := dbms.database.Query(selectQuery, agentId)
		if err != nil {
			logs.Debug("", "Failed to query tasks: "+err.Error())
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

func (dbms *DBMS) DbTasksListCompleted(agentId string, limit int, offset int) ([]adaptix.TaskData, error) {
	if !dbms.DatabaseExists() {
		return nil, errors.New("database does not exist")
	}

	selectQuery := `SELECT TaskId, AgentId, TaskType, Client, User, Computer, StartDate, FinishDate, CommandLine, MessageType, Message, ClearText, Completed FROM Tasks WHERE AgentId = ? AND Completed = 1 ORDER BY StartDate DESC LIMIT ? OFFSET ?;`
	rows, err := dbms.database.Query(selectQuery, agentId, limit, offset)
	if err != nil {
		return nil, err
	}
	defer func(rows *sql.Rows) {
		_ = rows.Close()
	}(rows)

	tasks := make([]adaptix.TaskData, 0, limit)
	for rows.Next() {
		var taskData adaptix.TaskData
		err = rows.Scan(&taskData.TaskId, &taskData.AgentId, &taskData.Type, &taskData.Client, &taskData.User, &taskData.Computer, &taskData.StartDate, &taskData.FinishDate, &taskData.CommandLine, &taskData.MessageType, &taskData.Message, &taskData.ClearText, &taskData.Completed)
		if err != nil {
			continue
		}
		tasks = append(tasks, taskData)
	}

	return tasks, nil
}

func (dbms *DBMS) DbTasksLimited(agentId string, limit int) []adaptix.TaskData {
	var tasks []adaptix.TaskData

	ok := dbms.DatabaseExists()
	if ok {
		selectQuery := `SELECT TaskId, AgentId, TaskType, Client, User, Computer, StartDate, FinishDate, CommandLine, MessageType, Message, ClearText, Completed FROM Tasks WHERE AgentId = ? ORDER BY StartDate DESC LIMIT ?;`
		query, err := dbms.database.Query(selectQuery, agentId, limit)
		if err != nil {
			logs.Debug("", "Failed to query tasks: "+err.Error())
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

func (dbms *DBMS) DbTasksCount(agentId string) int {
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
