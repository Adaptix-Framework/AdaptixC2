package server

import (
	"AdaptixServer/core/adaptix"
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/safe"
	"context"
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"math/rand"
	"net"
	"strconv"
	"time"
)

type TunnelConnection struct {
	channelId    int
	conn         net.Conn
	ctx          context.Context
	handleCancel context.CancelFunc
}

type Tunnel struct {
	Data adaptix.TunnelData

	listener    net.Listener
	connections safe.Map

	handlerConnect func(channelId int, addr string, port int) []byte
	handlerWrite   func(channelId int, data []byte) []byte
	handlerClose   func(channelId int) []byte
}

/// socket

func socketToTunnelData(agent *Agent, channelId int) {
	value, ok := connections.Get(strconv.Itoa(channelId))
	if !ok {
		return
	}
	tunnelConnection := value.(*TunnelConnection)

	buffer := make([]byte, 0x10000)
	for {
		n, err := tunnelConnection.conn.Read(buffer)
		if err != nil {
			if err == io.EOF {
				rawTaskData := handlerClose(tunnelConnection.channelId)

				var taskData adaptix.TaskData
				err = json.Unmarshal(rawTaskData, &taskData)
				if err != nil {
					return
				}

				if taskData.TaskId == "" {
					taskData.TaskId, _ = krypt.GenerateUID(8)
				}
				taskData.AgentId = agent.Data.Id
				agent.TunnelQueue.Put(taskData)
			} else {
				fmt.Printf("Error read data: %v\n", err)
			}
			break
		}

		rawTaskData := handlerWrite(tunnelConnection.channelId, buffer[:n])

		var taskData adaptix.TaskData
		err = json.Unmarshal(rawTaskData, &taskData)
		if err != nil {
			return
		}

		if taskData.TaskId == "" {
			taskData.TaskId, _ = krypt.GenerateUID(8)
		}
		taskData.AgentId = agent.Data.Id
		agent.TunnelQueue.Put(taskData)
	}
}

func tunnelDataToSocket(agent *Agent, channelId int, data []byte) {
	value, ok := connections.Get(strconv.Itoa(channelId))
	if !ok {
		return
	}
	tunnelConnection, ok := value.(*TunnelConnection)
	if !ok {
		return
	}
	tunnelConnection.conn.Write(data)
}

//////////

func (ts *Teamserver) TsTunnelConnectionData(AgentId string, channelId int, data []byte) {
	value, ok := ts.agents.Get(AgentId)
	if !ok {
		return
	}
	agent, _ := value.(*Agent)

	go tunnelDataToSocket(agent, channelId, data)
}

func (ts *Teamserver) TsTunnelConnectionResume(AgentId string, channelId int) {
	value, ok := ts.agents.Get(AgentId)
	if !ok {
		return
	}
	agent, _ := value.(*Agent)

	go socketToTunnelData(agent, channelId)
}

func (ts *Teamserver) TsTunnelConnectionClose(channelId int) {
	value, ok := connections.Get(strconv.Itoa(channelId))
	if !ok {
		return
	}

	tunnelConnection := value.(*TunnelConnection)
	tunnelConnection.handleCancel()
	tunnelConnection.conn.Close()
	connections.Delete(strconv.Itoa(channelId))
}
