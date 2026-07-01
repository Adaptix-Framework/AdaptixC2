package main

import (
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"os"

	"github.com/gorilla/websocket"
	"golang.org/x/term"
)

func runAgentTerminal(c *Client, agentID string, shellType string, rows int, cols int) error {
	if err := c.connectWebSocket(); err != nil {
		return fmt.Errorf("connect: %w", err)
	}
	defer c.disconnect()

	if err := c.syncAndWait(); err != nil {
		return err
	}

	agent := getAgentInfo(c, agentID)
	if shellType == "" {
		shellType = detectShellType(agent)
	}
	program := shellToProgram(shellType)

	terminalID := generateHexID(8)

	fd := int(os.Stdin.Fd())
	tw, th, err := term.GetSize(fd)
	if err == nil {
		rows = th
		cols = tw
	} else if rows == 0 {
		rows = 24
	}
	if cols == 0 {
		cols = 80
	}
	oemCP := 0
	if agent != nil {
		oemCP = agent.OemCP
	}

	termConfig := map[string]interface{}{
		"agent_id":    agentID,
		"terminal_id": terminalID,
		"program":     program,
		"size_h":      rows,
		"size_w":      cols,
		"oem_cp":      oemCP,
	}

	channelWS, err := c.connectChannelWebSocket("channel_terminal", termConfig)
	if err != nil {
		return fmt.Errorf("open terminal channel: %w", err)
	}
	defer channelWS.Close()

	oldState, err := term.MakeRaw(fd)
	if err != nil {
		return fmt.Errorf("make raw: %w", err)
	}
	defer term.Restore(fd, oldState)

	done := make(chan struct{}, 2)

	go func() {
		buf := make([]byte, 4096)
		for {
			n, err := os.Stdin.Read(buf)
			if err != nil {
				break
			}
			if err := channelWS.WriteMessage(websocket.BinaryMessage, buf[:n]); err != nil {
				break
			}
		}
		done <- struct{}{}
	}()

	go func() {
		for {
			_, msg, err := channelWS.ReadMessage()
			if err != nil {
				break
			}
			os.Stdout.Write(msg)
		}
		done <- struct{}{}
	}()

	<-done
	close(done)
	channelWS.WriteMessage(websocket.CloseMessage, websocket.FormatCloseMessage(websocket.CloseNormalClosure, ""))
	return nil
}

func shellToProgram(shell string) string {
	switch shell {
	case "bash":
		return "/bin/bash"
	case "sh":
		return "/bin/sh"
	case "cmd":
		return "cmd.exe"
	case "powershell", "ps":
		return "powershell"
	default:
		return "/bin/sh"
	}
}

func generateHexID(n int) string {
	b := make([]byte, n)
	rand.Read(b)
	return hex.EncodeToString(b)[:n]
}
