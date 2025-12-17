package server

import (
	"encoding/base64"
	"errors"
	"fmt"
	"io"
	"strconv"
	"strings"
	"sync"

	"github.com/gorilla/websocket"
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

	value, ok := ts.Agents.Get(agentId)
	if !ok {
		return errors.New("agent not found")
	}
	agent, ok := value.(*Agent)
	if !ok {
		return errors.New("invalid agent type")
	}
	if agent.Active == false {
		return fmt.Errorf("agent '%v' not active", agentId)
	}

	terminal := &Terminal{
		TerminalId: int(termId),
		agent:      agent,
		wsconn:     wsconn,
	}

	terminal.prSrv, terminal.pwSrv = io.Pipe()
	terminal.prTun, terminal.pwTun = io.Pipe()

	terminal.handlerStart, terminal.handlerWrite, terminal.handlerClose, err = ts.Extender.ExAgentTerminalCallbacks(agent.GetData())
	if err != nil {
		return err
	}

	taskData, err := terminal.handlerStart(terminal.TerminalId, program, sizeH, sizeW)
	if err != nil {
		return err
	}

	tunnelManageTask(agent, taskData)

	ts.terminals.Put(terminalId, terminal)

	return nil
}

func (ts *Teamserver) TsAgentTerminalCloseChannel(terminalId string, status string) error {
	_ = ts.TsTerminalConnClose(terminalId, status)
	return nil
}

///

func (ts *Teamserver) TsTerminalConnExists(terminalId string) bool {
	return ts.terminals.Contains(terminalId)
}

func (ts *Teamserver) TsTerminalGetPipe(AgentId string, terminalId string) (*io.PipeReader, *io.PipeWriter, error) {
	value, ok := ts.terminals.Get(terminalId)
	if !ok {
		return nil, nil, errors.New("terminal not found")
	}
	terminal, _ := value.(*Terminal)

	return terminal.prSrv, terminal.pwTun, nil
}

func (ts *Teamserver) TsTerminalConnResume(agentId string, terminalId string) {
	value, ok := ts.Agents.Get(agentId)
	if !ok {
		return
	}
	agent, ok := value.(*Agent)
	if !ok {
		return
	}

	value, ok = ts.terminals.Get(terminalId)
	if !ok {
		return
	}
	terminal, ok := value.(*Terminal)
	if !ok {
		return
	}

	relayWebsocketToTerminal(agent, terminal)
}

func (ts *Teamserver) TsTerminalConnClose(terminalId string, status string) error {
	value, ok := ts.terminals.GetDelete(terminalId)
	if !ok {
		return errors.New("terminal not found")
	}
	terminal, ok := value.(*Terminal)
	if !ok {
		return errors.New("invalid terminal type")
	}

	terminal.wsconn.Close()

	terminal.prSrv.Close()
	terminal.pwSrv.Close()
	terminal.prTun.Close()
	terminal.pwTun.Close()

	return nil
}

///

func relayWebsocketToTerminal(agent *Agent, terminal *Terminal) {
	var closeOnce sync.Once
	closeChannel := func() {
		closeOnce.Do(func() {
			_ = terminal.wsconn.Close()
			taskData, err := terminal.handlerClose(terminal.TerminalId)
			if err != nil {
				return
			}
			tunnelManageTask(agent, taskData)
		})
	}

	go func() {
		for {
			_, msg, err := terminal.wsconn.ReadMessage()
			if err != nil {
				break
			}
			_, err = terminal.pwSrv.Write(msg)
			if err != nil {
				break
			}
		}
		closeChannel()
	}()

	go func() {
		buf := make([]byte, 0x8000)
		for {
			n, err := terminal.prTun.Read(buf)
			if err != nil {
				break
			}
			if err := terminal.wsconn.WriteMessage(websocket.BinaryMessage, buf[:n]); err != nil {
				break
			}
		}
		closeChannel()
	}()
}
