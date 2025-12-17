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

var (
	ErrTerminalNotFound    = errors.New("terminal not found")
	ErrInvalidTerminalType = errors.New("invalid terminal type")
)

func (ts *Teamserver) TsAgentTerminalCreateChannel(terminalData string, wsconn *websocket.Conn) error {
	data, err := base64.StdEncoding.DecodeString(terminalData)
	if err != nil {
		return errors.New("invalid terminal data")
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
		return errors.New("invalid terminal data")
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

	agent, err := ts.getAgent(agentId)
	if err != nil {
		return err
	}
	if !agent.Active {
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
		return nil, nil, ErrTerminalNotFound
	}
	terminal, ok := value.(*Terminal)
	if !ok {
		return nil, nil, ErrInvalidTerminalType
	}
	return terminal.prSrv, terminal.pwTun, nil
}

func (ts *Teamserver) TsTerminalConnResume(agentId string, terminalId string) {
	agent, err := ts.getAgent(agentId)
	if err != nil {
		return
	}

	value, ok := ts.terminals.Get(terminalId)
	if !ok {
		return
	}
	terminal, ok := value.(*Terminal)
	if !ok {
		return
	}

	relayWebsocketToTerminal(ts, agent, terminal, terminalId)
}

func (ts *Teamserver) TsTerminalConnClose(terminalId string, status string) error {
	value, ok := ts.terminals.GetDelete(terminalId)
	if !ok {
		return ErrTerminalNotFound
	}
	terminal, ok := value.(*Terminal)
	if !ok {
		return ErrInvalidTerminalType
	}

	closeTerminalResources(terminal)
	return nil
}

func closeTerminalResources(terminal *Terminal) {
	if terminal == nil {
		return
	}
	if terminal.wsconn != nil {
		_ = terminal.wsconn.Close()
	}
	if terminal.prSrv != nil {
		_ = terminal.prSrv.Close()
	}
	if terminal.pwSrv != nil {
		_ = terminal.pwSrv.Close()
	}
	if terminal.prTun != nil {
		_ = terminal.prTun.Close()
	}
	if terminal.pwTun != nil {
		_ = terminal.pwTun.Close()
	}
}

///

func relayWebsocketToTerminal(ts *Teamserver, agent *Agent, terminal *Terminal, terminalId string) {
	var closeOnce sync.Once
	var wsWriteMu sync.Mutex

	closeAll := func() {
		closeOnce.Do(func() {
			ts.terminals.Delete(terminalId)
			closeTerminalResources(terminal)

			taskData, err := terminal.handlerClose(terminal.TerminalId)
			if err == nil {
				tunnelManageTask(agent, taskData)
			}
		})
	}

	go func() {
		defer closeAll()
		for {
			_, msg, err := terminal.wsconn.ReadMessage()
			if err != nil {
				return
			}
			if terminal.pwSrv == nil {
				return
			}
			if _, err = terminal.pwSrv.Write(msg); err != nil {
				return
			}
		}
	}()

	go func() {
		defer closeAll()
		buf := ts.TunnelManager.GetBuffer()
		defer ts.TunnelManager.PutBuffer(buf)

		for {
			if terminal.prTun == nil {
				return
			}
			n, err := terminal.prTun.Read(buf)
			if err != nil {
				return
			}

			wsWriteMu.Lock()
			writeErr := terminal.wsconn.WriteMessage(websocket.BinaryMessage, buf[:n])
			wsWriteMu.Unlock()

			if writeErr != nil {
				return
			}
		}
	}()
}
