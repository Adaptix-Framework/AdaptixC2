package server

import (
	"bytes"
	"encoding/json"
	"fmt"
	adaptix "github.com/Adaptix-Framework/axc2"
	"io"
	"net/http"
	"strings"
	"time"
)

func (ts *Teamserver) TsEventClient(connected bool, username string) {
	var packet SpEvent

	if connected {
		message := fmt.Sprintf("Client '%v' connected to teamserver", username)
		packet = CreateSpEvent(EVENT_CLIENT_CONNECT, message)
	} else {
		message := fmt.Sprintf("Client '%v' disconnected from teamserver", username)
		packet = CreateSpEvent(EVENT_CLIENT_DISCONNECT, message)
	}

	ts.TsSyncAllClients(packet)
	ts.events.Put(packet)
}

func (ts *Teamserver) TsEventListenerStart(restart bool, listenerName string, listenerType string) {
	var message string
	if restart {
		message = fmt.Sprintf("Listener '%v' reconfigured", listenerName)
	} else {
		message = fmt.Sprintf("Listener '%v' (%v) started", listenerName, listenerType)
	}

	packet := CreateSpEvent(EVENT_LISTENER_START, message)
	ts.TsSyncAllClients(packet)
	ts.events.Put(packet)
}

func (ts *Teamserver) TsEventListenerStop(listenerName string, listenerType string) {
	message := fmt.Sprintf("Listener '%v' stopped", listenerName)
	packet := CreateSpEvent(EVENT_LISTENER_STOP, message)
	ts.TsSyncAllClients(packet)
	ts.events.Put(packet)
}

func (ts *Teamserver) TsEventAgent(restore bool, agentData adaptix.AgentData) {
	message := ""
	if restore {
		createdAt := time.Unix(agentData.CreateTime, 0).Format("15:04:05 02.01.2006")
		if len(agentData.Domain) == 0 || strings.ToLower(agentData.Computer) == strings.ToLower(agentData.Domain) {
			message = fmt.Sprintf("Restore '%v' (%v) executed on '%v @ %v' (%v) [created '%v']", agentData.Name, agentData.Id, agentData.Username, agentData.Computer, agentData.InternalIP, createdAt)
		} else {
			message = fmt.Sprintf("Restore '%v' (%v) executed on '%v @ %v.%v' (%v) [created '%v']", agentData.Name, agentData.Id, agentData.Username, agentData.Computer, agentData.Domain, agentData.InternalIP, createdAt)
		}
	} else {
		if len(agentData.Domain) == 0 || strings.ToLower(agentData.Computer) == strings.ToLower(agentData.Domain) {
			message = fmt.Sprintf("New '%v' (%v) executed on '%v @ %v' (%v)", agentData.Name, agentData.Id, agentData.Username, agentData.Computer, agentData.InternalIP)
		} else {
			message = fmt.Sprintf("New '%v' (%v) executed on '%v @ %v.%v' (%v)", agentData.Name, agentData.Id, agentData.Username, agentData.Computer, agentData.Domain, agentData.InternalIP)
		}
	}

	packet := CreateSpEvent(EVENT_AGENT_NEW, message)
	ts.TsSyncAllClients(packet)
	ts.events.Put(packet)

	if !restore && ts.Profile.Callbacks.Telegram.Token != "" && ts.Profile.Callbacks.Telegram.ChatsId != nil && ts.Profile.Callbacks.NewAgentMessage != "" {
		msg := ts.Profile.Callbacks.NewAgentMessage
		msg = strings.ReplaceAll(msg, "%type%", agentData.Name)
		msg = strings.ReplaceAll(msg, "%id%", agentData.Id)
		msg = strings.ReplaceAll(msg, "%user%", agentData.Username)
		msg = strings.ReplaceAll(msg, "%computer%", agentData.Computer)
		msg = strings.ReplaceAll(msg, "%externalip%", agentData.ExternalIP)
		msg = strings.ReplaceAll(msg, "%internalip%", agentData.InternalIP)
		msg = strings.ReplaceAll(msg, "%domain%", agentData.Domain)
		msg = strings.ReplaceAll(msg, "%elevated%", fmt.Sprintf("%v", agentData.Elevated))
		msg = strings.ReplaceAll(msg, "%pid%", fmt.Sprintf("%v", agentData.Elevated))

		for _, chatId := range ts.Profile.Callbacks.Telegram.ChatsId {
			SendTelegram(msg, ts.Profile.Callbacks.Telegram.Token, chatId)
		}
	}
}

func (ts *Teamserver) TsEventTunnelAdd(tunnel *Tunnel) {
	if tunnel.Data.Client == "" {
		message := ""
		switch tunnel.Type {
		case TUNNEL_SOCKS4:
			message = fmt.Sprintf("SOCKS4 server started on '%s:%s'", tunnel.Data.Interface, tunnel.Data.Port)
		case TUNNEL_SOCKS5:
			message = fmt.Sprintf("SOCKS5 server started on '%s:%s'", tunnel.Data.Interface, tunnel.Data.Port)
		case TUNNEL_SOCKS5_AUTH:
			message = fmt.Sprintf("SOCKS5 (with Auth) server started on '%s:%s'", tunnel.Data.Interface, tunnel.Data.Port)
		case TUNNEL_LPORTFWD:
			message = fmt.Sprintf("Local port forward started on '%s:%s'", tunnel.Data.Interface, tunnel.Data.Port)
		case TUNNEL_RPORTFWD:
			message = fmt.Sprintf("Reverse port forward '%s' to '%s:%s'", tunnel.Data.Port, tunnel.Data.Fhost, tunnel.Data.Fport)
		default:
			return
		}

		packet := CreateSpEvent(EVENT_TUNNEL_START, message)
		ts.TsSyncAllClients(packet)
		ts.events.Put(packet)
	}
}

func (ts *Teamserver) TsEventTunnelRemove(tunnel *Tunnel) {
	if tunnel.Data.Client == "" {
		message := ""
		switch tunnel.Type {
		case TUNNEL_SOCKS4:
			message = fmt.Sprintf("SOCKS4 server ':%s' stopped", tunnel.Data.Port)
		case TUNNEL_SOCKS5:
			message = fmt.Sprintf("SOCKS5 server ':%s' stopped", tunnel.Data.Port)
		case TUNNEL_SOCKS5_AUTH:
			message = fmt.Sprintf("SOCKS5 (with Auth) server ':%s' stopped", tunnel.Data.Port)
		case TUNNEL_LPORTFWD:
			message = fmt.Sprintf("Local port forward on ':%s' stopped", tunnel.Data.Port)
		case TUNNEL_RPORTFWD:
			message = fmt.Sprintf("Remote port forward to '%s:%s' stopped", tunnel.Data.Fhost, tunnel.Data.Fport)
		default:
			return
		}

		packet := CreateSpEvent(EVENT_TUNNEL_STOP, message)
		ts.TsSyncAllClients(packet)
		ts.events.Put(packet)
	}
}

func SendTelegram(text, botToken, chatID string) {
	go func() {
		url := fmt.Sprintf("https://api.telegram.org/bot%s/sendMessage", botToken)
		payload := map[string]string{
			"chat_id": chatID,
			"text":    text,
		}
		data, _ := json.Marshal(payload)

		req, err := http.NewRequest("POST", url, bytes.NewBuffer(data))
		if err != nil {
			return
		}
		req.Header.Set("Content-Type", "application/json")

		resp, err := http.DefaultClient.Do(req)
		if err != nil {
			return
		}
		defer func(Body io.ReadCloser) {
			_ = Body.Close()
		}(resp.Body)
	}()
}
