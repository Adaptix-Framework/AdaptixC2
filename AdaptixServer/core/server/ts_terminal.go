package server

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"strconv"
	"sync"

	"github.com/gorilla/websocket"
)

var (
	ErrTerminalNotFound    = errors.New("terminal not found")
	ErrInvalidTerminalType = errors.New("invalid terminal type")
)

type TerminalChannelData struct {
	AgentId    string `json:"agent_id"`
	TerminalId string `json:"terminal_id"`
	Program    string `json:"program"`
	SizeH      int    `json:"size_h"`
	SizeW      int    `json:"size_w"`
	OemCP      int    `json:"oem_cp"`
}

func (ts *Teamserver) TsAgentTerminalCreateChannel(terminalData string, wsconn *websocket.Conn) error {
	var td TerminalChannelData
	if err := json.Unmarshal([]byte(terminalData), &td); err != nil {
		return errors.New("invalid terminal data")
	}

	termId, err := strconv.ParseInt(td.TerminalId, 16, 64)
	if err != nil {
		return errors.New("TerminalId not supported")
	}

	agent, err := ts.getAgent(td.AgentId)
	if err != nil {
		return err
	}
	if !agent.Active {
		return fmt.Errorf("agent '%v' not active", td.AgentId)
	}

	terminal := &Terminal{
		TerminalId: int(termId),
		agent:      agent,
		wsconn:     wsconn,
		CodePage:   td.OemCP,
	}

	terminal.prSrv, terminal.pwSrv = io.Pipe()
	terminal.prTun, terminal.pwTun = io.Pipe()

	terminal.Callbacks = agent.TerminalCallbacks()

	taskData := terminal.Callbacks.Start(terminal.TerminalId, td.Program, td.SizeH, td.SizeW, td.OemCP)

	tunnelManageTask(agent, taskData)

	ts.terminals.Put(td.TerminalId, terminal)

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
			terminal.mu.Lock()
			closed := terminal.closed
			terminal.mu.Unlock()
			if closed {
				break
			}
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
