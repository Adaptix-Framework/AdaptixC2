package server

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"sync"
	"time"

	"github.com/Adaptix-Framework/axc2/v2"

	"github.com/gorilla/websocket"
)

const terminalStartTimeout = 60 * time.Second

var (
	ErrTerminalNotFound    = errors.New("terminal not found")
	ErrInvalidTerminalType = errors.New("invalid terminal type")
	ErrTerminalNoSupport   = errors.New("agent does not support terminal")
)

type TerminalChannelData struct {
	AgentId    int64  `json:"agent_id"`
	TerminalId int64  `json:"terminal_id"`
	Program    string `json:"program"`
	SizeH      int    `json:"size_h"`
	SizeW      int    `json:"size_w"`
	OemCP      int    `json:"oem_cp"`
}

func (ts *Teamserver) TsAgentTerminalCreateChannel(terminalData string, wsconn adaptix.WebSocketConn) error {
	var td TerminalChannelData
	if err := json.Unmarshal([]byte(terminalData), &td); err != nil {
		return errors.New("invalid terminal data")
	}

	agent, ok := ts.Agents.Get(td.AgentId)
	if !ok {
		return fmt.Errorf("agent %v not found", td.AgentId)
	}
	if !agent.IsActive() {
		return fmt.Errorf("agent '%v' not active", td.AgentId)
	}

	if agent.Fn.TerminalCB.Start == nil {
		return ErrTerminalNoSupport
	}

	if td.TerminalId == 0 {
		return errors.New("invalid terminal id")
	}
	if ts.terminals.Contains(td.TerminalId) {
		return fmt.Errorf("terminal %v already exists", td.TerminalId)
	}

	terminal := &Terminal{
		TerminalId: td.TerminalId,
		agentId:    td.AgentId,
		wsconn:     wsconn,
		CodePage:   td.OemCP,
		Callbacks:  agent.Fn.TerminalCB,
	}

	terminal.prSrv, terminal.pwSrv = io.Pipe()
	terminal.prTun, terminal.pwTun = io.Pipe()

	ts.terminals.Put(td.TerminalId, terminal)

	taskData := terminal.Callbacks.Start(terminal.TerminalId, td.Program, td.SizeH, td.SizeW, td.OemCP)
	tunnelManageTask(ts, td.AgentId, taskData)

	go ts.terminalStartWatchdog(td.TerminalId, td.AgentId)

	return nil
}

func (ts *Teamserver) terminalStartWatchdog(terminalId int64, agentId int64) {
	timer := time.NewTimer(terminalStartTimeout)
	defer timer.Stop()
	<-timer.C

	terminal, ok := ts.terminals.Get(terminalId)
	if !ok {
		return
	}
	if !terminal.resumed.CompareAndSwap(false, true) {
		return
	}

	terminal, ok = ts.terminals.GetDelete(terminalId)
	if !ok {
		return
	}

	closeTerminalResources(terminal, "terminal start timeout")
	if terminal.Callbacks.Close != nil {
		tunnelManageTask(ts, agentId, terminal.Callbacks.Close(terminalId))
	}
}

func (ts *Teamserver) TsAgentTerminalCloseChannel(terminalId int64, status string) error {
	return ts.TsTerminalConnClose(terminalId, status)
}

///

func (ts *Teamserver) TsTerminalConnExists(terminalId int64) bool {
	return ts.terminals.Contains(terminalId)
}

func (ts *Teamserver) TsTerminalGetPipe(AgentId int64, terminalId int64) (*io.PipeReader, *io.PipeWriter, error) {
	terminal, ok := ts.terminals.Get(terminalId)
	if !ok {
		return nil, nil, ErrTerminalNotFound
	}
	_ = AgentId
	return terminal.prSrv, terminal.pwTun, nil
}

func (ts *Teamserver) TsTerminalConnResume(agentId int64, terminalId int64, ioDirect bool) {
	terminal, ok := ts.terminals.Get(terminalId)
	if !ok {
		return
	}

	_ = agentId
	if _, ok := ts.Agents.Get(terminal.agentId); !ok {
		return
	}

	if !terminal.resumed.CompareAndSwap(false, true) {
		return
	}

	relayWebsocketToTerminal(ts, terminal.agentId, terminal, terminalId, ioDirect)
}

func (ts *Teamserver) TsTerminalConnData(terminalId int64, data []byte) {
	if len(data) == 0 {
		return
	}

	terminal, ok := ts.terminals.Get(terminalId)
	if !ok {
		return
	}

	terminal.mu.Lock()
	closed := terminal.closed
	pw := terminal.pwTun
	terminal.mu.Unlock()

	if closed || pw == nil {
		return
	}
	_, _ = pw.Write(data)
}

func (ts *Teamserver) TsTerminalConnClose(terminalId int64, status string) error {
	terminal, ok := ts.terminals.GetDelete(terminalId)
	if !ok {
		return ErrTerminalNotFound
	}

	terminal.skipAgentClose.Store(true)
	closeTerminalResources(terminal, status)
	return nil
}

func closeTerminalResources(terminal *Terminal, status string) {
	if terminal == nil {
		return
	}

	terminal.mu.Lock()
	if terminal.closed {
		terminal.mu.Unlock()
		return
	}
	terminal.closed = true
	pwTun := terminal.pwTun
	prTun := terminal.prTun
	pwSrv := terminal.pwSrv
	prSrv := terminal.prSrv
	wsconn := terminal.wsconn
	terminal.mu.Unlock()

	terminal.wsWriteMu.Lock()
	if wsconn != nil {
		if status != "" {
			_ = wsconn.WriteMessage(websocket.TextMessage, []byte(status))
		}
		_ = wsconn.Close()
	}
	terminal.wsWriteMu.Unlock()

	if pwTun != nil {
		_ = pwTun.Close()
	}
	if prTun != nil {
		_ = prTun.Close()
	}
	if pwSrv != nil {
		_ = pwSrv.Close()
	}
	if prSrv != nil {
		_ = prSrv.Close()
	}
}

///

func relayWebsocketToTerminal(ts *Teamserver, agentId int64, terminal *Terminal, terminalId int64, direct bool) {
	ctx, cancel := context.WithCancel(context.Background())
	var once sync.Once
	finish := func() {
		once.Do(func() {
			cancel()
			ts.terminals.Delete(terminalId)
			closeTerminalResources(terminal, "")

			if terminal.skipAgentClose.Load() {
				return
			}
			if terminal.Callbacks.Close == nil {
				return
			}
			taskData := terminal.Callbacks.Close(terminal.TerminalId)
			tunnelManageTask(ts, agentId, taskData)
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
			if len(msg) == 0 {
				continue
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
				payload := make([]byte, n)
				copy(payload, buf[:n])

				terminal.wsWriteMu.Lock()
				writeErr := terminal.wsconn.WriteMessage(websocket.BinaryMessage, payload)
				terminal.wsWriteMu.Unlock()
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
			if terminal.Callbacks.Write == nil {
				return
			}
			buf := ts.TunnelManager.GetBuffer()
			defer ts.TunnelManager.PutBuffer(buf)

			agent, _ := ts.Agents.Get(agentId)
			backoff := time.Duration(1) * time.Millisecond
			const maxBackoff = 50 * time.Millisecond
			const minBackoff = 1 * time.Millisecond

			for {
				select {
				case <-ctx.Done():
					return
				default:
					if agent != nil && agent.HostedQueue != nil && agent.HostedQueue.Len() > 128 {
						time.Sleep(backoff)
						if backoff < maxBackoff {
							backoff *= 2
						}
						continue
					}

					n, err := terminal.prSrv.Read(buf)
					if n > 0 {
						backoff = minBackoff
						payload := make([]byte, n)
						copy(payload, buf[:n])
						taskData := terminal.Callbacks.Write(terminal.TerminalId, terminal.CodePage, payload)
						relayPipeToTaskData(ts, agentId, terminal.TerminalId, taskData)
					}
					if err != nil {
						return
					}
				}
			}
		}()
	}
}
