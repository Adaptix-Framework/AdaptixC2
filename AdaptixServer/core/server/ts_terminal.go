package server

import (
	"context"
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
	if len(d) != 6 {
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

	OemCP, err := strconv.Atoi(d[5])
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
		CodePage:   OemCP,
	}

	terminal.prSrv, terminal.pwSrv = io.Pipe()
	terminal.prTun, terminal.pwTun = io.Pipe()

	terminal.Callbacks = agent.TerminalCallbacks()

	taskData := terminal.Callbacks.Start(terminal.TerminalId, program, sizeH, sizeW, OemCP)

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

func (ts *Teamserver) TsTerminalConnResume(agentId string, terminalId string, ioDirect bool) {
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

	relayWebsocketToTerminal(ts, agent, terminal, terminalId, ioDirect)
}

func (ts *Teamserver) TsTerminalConnData(terminalId string, data []byte) {
	value, ok := ts.terminals.Get(terminalId)
	if !ok {
		return
	}
	terminal, ok := value.(*Terminal)
	if !ok {
		return
	}

	terminal.mu.Lock()
	defer terminal.mu.Unlock()

	if terminal.closed || terminal.pwTun == nil {
		return
	}
	_, _ = terminal.pwTun.Write(data)
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

	terminal.mu.Lock()
	if terminal.closed {
		terminal.mu.Unlock()
		return
	}
	terminal.closed = true
	terminal.mu.Unlock()

	if terminal.wsconn != nil {
		_ = terminal.wsconn.Close()
	}
	if terminal.pwTun != nil {
		_ = terminal.pwTun.Close()
	}
	if terminal.prTun != nil {
		_ = terminal.prTun.Close()
	}
	if terminal.pwSrv != nil {
		_ = terminal.pwSrv.Close()
	}
	if terminal.prSrv != nil {
		_ = terminal.prSrv.Close()
	}
}

///

func relayWebsocketToTerminal(ts *Teamserver, agent *Agent, terminal *Terminal, terminalId string, direct bool) {
	ctx, cancel := context.WithCancel(context.Background())
	var once sync.Once
	var wsWriteMu sync.Mutex
	finish := func() {
		once.Do(func() {
			cancel()
			ts.terminals.Delete(terminalId)
			closeTerminalResources(terminal)

			taskData := terminal.Callbacks.Close(terminal.TerminalId)
			tunnelManageTask(agent, taskData)
		})
	}

	go func() {
		defer finish()
		if terminal.wsconn == nil || terminal.pwSrv == nil {
			return
		}
		for {
			_, msg, err := terminal.wsconn.ReadMessage()
			if err != nil {
				break
			}
			if _, err := terminal.pwSrv.Write(msg); err != nil {
				break
			}
		}
		_ = terminal.pwSrv.Close()
	}()

	go func() {
		defer finish()
		if terminal.wsconn == nil || terminal.prTun == nil {
			return
		}
		buf := ts.TunnelManager.GetBuffer()
		defer ts.TunnelManager.PutBuffer(buf)
		for {
			n, err := terminal.prTun.Read(buf)
			if n > 0 {
				wsWriteMu.Lock()
				writeErr := terminal.wsconn.WriteMessage(websocket.BinaryMessage, buf[:n])
				wsWriteMu.Unlock()
				if writeErr != nil {
					break
				}
			}
			if err != nil {
				break
			}
		}
	}()

	if !direct {
		go func() {
			buf := ts.TunnelManager.GetBuffer()
			defer ts.TunnelManager.PutBuffer(buf)
			for {
				select {
				case <-ctx.Done():
					return
				default:
					n, err := terminal.prSrv.Read(buf)
					if n > 0 {
						taskData := terminal.Callbacks.Write(terminal.TerminalId, terminal.CodePage, buf[:n])
						tunnelManageTask(agent, taskData)
						relayPipeToTaskData(agent, terminal.TerminalId, taskData)
					}
					if err != nil {
						return
					}
				}
			}
		}()
	}
}
