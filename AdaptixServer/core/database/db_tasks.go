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
		if err == nil {

			for query.Next() {
				taskData := adaptix.TaskData{}
				err = query.Scan(&taskData.TaskId, &taskData.AgentId, &taskData.Type, &taskData.Client, &taskData.User, &taskData.Computer, &taskData.StartDate, &taskData.FinishDate, &taskData.CommandLine, &taskData.MessageType, &taskData.Message, &taskData.ClearText, &taskData.Completed)
				if err != nil {
					continue
				}
				tasks = append(tasks, taskData)
			}
		} else {
			logs.Debug("", "Failed to query tasks: "+err.Error())
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)
	}
	return tasks
}
