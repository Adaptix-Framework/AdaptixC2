package database

import (
	"AdaptixServer/core/adaptix"
	"database/sql"
	"errors"
	"fmt"
)

func (dbms *DBMS) DbTaskExist(taskId string) bool {
	rows, err := dbms.database.Query("SELECT TaskId FROM Tasks;")
	if err != nil {
		return false
	}
	defer func(rows *sql.Rows) {
		_ = rows.Close()
	}(rows)

	for rows.Next() {
		rowTaskId := ""
		_ = rows.Scan(&rowTaskId)
		if taskId == rowTaskId {
			return true
		}
	}
	return false
}

func (dbms *DBMS) DbTaskInsert(taskData adaptix.TaskData) error {
	var (
		err         error
		ok          bool
		insertQuery string
	)

	ok = dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.DbTaskExist(taskData.TaskId)
	if ok {
		return fmt.Errorf("task %s alredy exists", taskData.TaskId)
	}

	insertQuery = `INSERT INTO Tasks (TaskId, AgentId, TaskType, Client, User, Computer, StartDate, FinishDate, CommandLine, MessageType, Message, ClearText, Completed) values(?,?,?,?,?,?,?,?,?,?,?,?,?);`
	_, err = dbms.database.Exec(insertQuery, taskData.TaskId, taskData.AgentId, taskData.Type, taskData.Client, taskData.User, taskData.Computer, taskData.StartDate, taskData.FinishDate, taskData.CommandLine, taskData.MessageType, taskData.Message, taskData.ClearText, taskData.Completed)
	return err
}

func (dbms *DBMS) DbTaskUpdate(taskData adaptix.TaskData) error {
	var (
		err         error
		ok          bool
		updateQuery string
	)

	ok = dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	ok = dbms.DbTaskExist(taskData.TaskId)
	if !ok {
		return fmt.Errorf("task %s does not exists", taskData.TaskId)
	}

	updateQuery = `UPDATE Tasks SET FinishDate = ?, MessageType = ?, Message = ?, ClearText = ?, Completed = ? WHERE TaskId = ?;`
	_, err = dbms.database.Exec(updateQuery, taskData.FinishDate, taskData.MessageType, taskData.Message, taskData.ClearText, taskData.Completed, taskData.TaskId)
	return err
}

func (dbms *DBMS) DbTaskDelete(taskId string, agentId string) error {
	var (
		ok          bool
		err         error
		deleteQuery string
	)

	ok = dbms.DatabaseExists()
	if !ok {
		return errors.New("database not exists")
	}

	if taskId != "" {
		deleteQuery = `DELETE FROM Tasks WHERE TaskId = ?;`
		_, err = dbms.database.Exec(deleteQuery, taskId)
	} else if agentId != "" {
		deleteQuery = `DELETE FROM Tasks WHERE AgentId = ?;`
		_, err = dbms.database.Exec(deleteQuery, agentId)
	}

	return err
}

func (dbms *DBMS) DbTasksAll(agentId string) []adaptix.TaskData {
	var (
		tasks       []adaptix.TaskData
		ok          bool
		selectQuery string
	)

	ok = dbms.DatabaseExists()
	if ok {
		selectQuery = `SELECT TaskId, AgentId, TaskType, Client, User, Computer, StartDate, FinishDate, CommandLine, MessageType, Message, ClearText, Completed FROM Tasks WHERE AgentId = ? ORDER BY StartDate;`
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
			fmt.Println(err.Error() + " --- Clear database file!")
		}
		defer func(query *sql.Rows) {
			_ = query.Close()
		}(query)
	}
	return tasks
}
