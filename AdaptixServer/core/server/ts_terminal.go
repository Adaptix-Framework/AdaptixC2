package server

import (
	"AdaptixServer/core/utils/krypt"
	"context"
	"encoding/base64"
	"errors"
	"fmt"
	adaptix "github.com/Adaptix-Framework/axc2"
	"github.com/gorilla/websocket"
	"strconv"
	"strings"
)

func (ts *Teamserver) TsAgentTerminalCreateChannel(terminalData string, wsconn *websocket.Conn) error {

	data, err := base64.StdEncoding.DecodeString(terminalData)
	if err != nil {
		return errors.New("invalid tunnel data")
	}

	d := strings.Split(string(data), "|")
	if len(d) != 5 {
		return errors.New("invalid terminal data")
	}

	agentId := d[0]

	terminalId := d[1]
	termId, err := strconv.ParseInt(terminalId, 16, 64)
	if err != nil {
		return errors.New("TerminalId not supported")
	}

	decProgram, err := base64.StdEncoding.DecodeString(d[2])
	if err != nil {
		return errors.New("invalid tunnel data")
	}
	program := string(decProgram)

	sizeH, err := strconv.Atoi(d[3])
	if err != nil {
		return errors.New("invalid terminal data")
	}

	sizeW, err := strconv.Atoi(d[4])
	if err != nil {
		return errors.New("invalid terminal data")
	}

	value, ok := ts.agents.Get(agentId)
	if !ok {
		return errors.New("agent not found")
	}
	agent, _ := value.(*Agent)
	if agent.Active == false {
		return fmt.Errorf("agent '%v' not active", agentId)
	}

	terminal := &Terminal{
		TerminalId: int(termId),
		agent:      agent,
		wsconn:     wsconn,
	}
	terminal.ctx, terminal.ctxCancel = context.WithCancel(context.Background())

	terminal.handlerStart, terminal.handlerWrite, terminal.handlerClose, err = ts.Extender.ExAgentTerminalCallbacks(agent.Data)
	if err != nil {
		return err
	}

	taskData, err := terminal.handlerStart(terminal.TerminalId, program, sizeH, sizeW)
	if err != nil {
		return err
	}
	if taskData.TaskId == "" {
		taskData.TaskId, _ = krypt.GenerateUID(8)
	}

	terminal.TaskId = taskData.TaskId

	ts.terminals.Put(terminalId, terminal)

	agent.TunnelConnectTask.Put(taskData)

	return nil
}

func (ts *Teamserver) TsAgentTerminalCloseChannel(terminalId string, status string) error {

	_ = ts.TsTerminalConnClose(terminalId, status)

	/// ToDo: send close Msg
	return nil
}

///

func (ts *Teamserver) TsTerminalConnExists(terminalId string) bool {
	return ts.terminals.Contains(terminalId)
}

func (ts *Teamserver) TsTerminalConnData(terminalId string, data []byte) {
	value, ok := ts.terminals.Get(terminalId)
	if !ok {
		return
	}
	terminal, _ := value.(*Terminal)

	go terminalDataToWebSocket(terminal, data)
}

func (ts *Teamserver) TsTerminalConnResume(agentId string, terminalId string) {

	value, ok := ts.agents.Get(agentId)
	if !ok {
		return
	}
	agent, _ := value.(*Agent)

	value, ok = ts.terminals.Get(terminalId)
	if !ok {
		return
	}
	terminal, _ := value.(*Terminal)

	go webSocketToTerminalData(agent, terminal)
}

func (ts *Teamserver) TsTerminalConnClose(terminalId string, status string) error {
	value, ok := ts.terminals.GetDelete(terminalId)
	if !ok {
		return errors.New("terminal not found")
	}
	terminal, _ := value.(*Terminal)

	terminal.ctxCancel()
	terminal.wsconn.Close()

	return nil
}

///

func sendTerminalTaskData(agent *Agent, terminalId int, taskData adaptix.TaskData) {
	if taskData.TaskId == "" {
		taskData.TaskId, _ = krypt.GenerateUID(8)
	}
	taskData.AgentId = agent.Data.Id

	taskTunnel := adaptix.TaskDataTunnel{
		ChannelId: terminalId,
		Data:      taskData,
	}

	agent.TunnelQueue.Put(taskTunnel)
}

func webSocketToTerminalData(agent *Agent, terminal *Terminal) {
	var taskData adaptix.TaskData
	for {
		select {

		case <-terminal.ctx.Done():
			return

		default:
			_, data, err := terminal.wsconn.ReadMessage()
			if err != nil {
				taskData, _ = terminal.handlerClose(terminal.TerminalId)
				terminal.ctxCancel()
			} else {
				taskData, _ = terminal.handlerWrite(terminal.TerminalId, data)
			}
			sendTerminalTaskData(agent, terminal.TerminalId, taskData)
		}
	}
}

func terminalDataToWebSocket(terminal *Terminal, data []byte) {
	terminal.mu.Lock()
	defer terminal.mu.Unlock()
	_ = terminal.wsconn.WriteMessage(websocket.BinaryMessage, data)
}
