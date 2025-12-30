package server

import (
	"AdaptixServer/core/profile"
	"AdaptixServer/core/utils/tformat"
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"

	adaptix "github.com/Adaptix-Framework/axc2"
)

func (ts *Teamserver) TsEventClient(connected bool, username string) {
	var packet SpEvent

	if connected {
		message := fmt.Sprintf("Operator '%v' connected to teamserver", username)
		packet = CreateSpEvent(EVENT_CLIENT_CONNECT, message)
	} else {
		message := fmt.Sprintf("Operator '%v' disconnected from teamserver", username)
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
	message := "New "
	postMsg := agentData.Computer

	if len(agentData.Domain) != 0 && strings.ToLower(agentData.Computer) != strings.ToLower(agentData.Domain) {
		postMsg += "." + agentData.Domain + "'"
	} else {
		postMsg += "'"
	}
	if len(agentData.InternalIP) != 0 {
		postMsg = postMsg + " (" + agentData.InternalIP + ")"
	}
	if restore {
		message = "Restore "
		createdAt := time.Unix(agentData.CreateTime, 0).Format("15:04:05 02.01.2006")
		postMsg = postMsg + fmt.Sprintf(" [created '%v']", createdAt)
	}

	message += fmt.Sprintf("'%v' (%v) executed on '%v @ %v", agentData.Name, agentData.Id, agentData.Username, postMsg)

	packet := CreateSpEvent(EVENT_AGENT_NEW, message)
	ts.TsSyncAllClients(packet)
	ts.events.Put(packet)

	if !restore {
		go ts.TsEventCallbackAgent(agentData)
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

/// CALLBACKS

func (ts *Teamserver) hasAnyCallback() bool {
	if ts.Profile.Callbacks == nil {
		return false
	}
	if ts.Profile.Callbacks.Telegram.Token != "" && len(ts.Profile.Callbacks.Telegram.ChatsId) > 0 {
		return true
	}
	if ts.Profile.Callbacks.Slack.WebhookURL != "" {
		return true
	}
	for _, webhook := range ts.Profile.Callbacks.Webhooks {
		if webhook.URL != "" {
			return true
		}
	}
	return false
}

func (ts *Teamserver) sendToAllChannels(msg string) {
	if ts.Profile.Callbacks.Telegram.Token != "" {
		for _, chatId := range ts.Profile.Callbacks.Telegram.ChatsId {
			sendTelegram(msg, ts.Profile.Callbacks.Telegram.Token, chatId)
		}
	}

	if ts.Profile.Callbacks.Slack.WebhookURL != "" {
		sendSlack(msg, ts.Profile.Callbacks.Slack.WebhookURL)
	}

	for _, webhook := range ts.Profile.Callbacks.Webhooks {
		if webhook.URL != "" {
			sendWebhook(msg, webhook)
		}
	}
}

func (ts *Teamserver) TsEventCallbackAgent(agentData adaptix.AgentData) {
	if ts.Profile.Callbacks == nil || ts.Profile.Callbacks.NewAgentMessage == "" || !ts.hasAnyCallback() {
		return
	}

	msg := ts.Profile.Callbacks.NewAgentMessage
	msg = strings.ReplaceAll(msg, "%type%", agentData.Name)
	msg = strings.ReplaceAll(msg, "%id%", agentData.Id)
	msg = strings.ReplaceAll(msg, "%user%", agentData.Username)
	msg = strings.ReplaceAll(msg, "%computer%", agentData.Computer)
	msg = strings.ReplaceAll(msg, "%externalip%", agentData.ExternalIP)
	msg = strings.ReplaceAll(msg, "%internalip%", agentData.InternalIP)
	msg = strings.ReplaceAll(msg, "%domain%", agentData.Domain)
	msg = strings.ReplaceAll(msg, "%elevated%", fmt.Sprintf("%v", agentData.Elevated))
	msg = strings.ReplaceAll(msg, "%pid%", fmt.Sprintf("%v", agentData.Pid))

	ts.sendToAllChannels(msg)
}

func (ts *Teamserver) TsEventCallbackCreds(creds []adaptix.CredsData) {
	if ts.Profile.Callbacks == nil || ts.Profile.Callbacks.NewCredMessage == "" || !ts.hasAnyCallback() {
		return
	}

	if len(creds) > 4 {
		msg := fmt.Sprintf("Added %d new credentials", len(creds))
		ts.sendToAllChannels(msg)
		return
	}

	for _, credData := range creds {
		secret := "*****"
		if len(credData.Password) >= 3 {
			secret = credData.Password[:3] + "*****"
		} else if len(credData.Password) > 0 {
			secret = credData.Password[:1] + "*****"
		}

		msg := ts.Profile.Callbacks.NewCredMessage
		msg = strings.ReplaceAll(msg, "%username%", credData.Username)
		msg = strings.ReplaceAll(msg, "%password%", secret)
		msg = strings.ReplaceAll(msg, "%domain%", credData.Realm)
		msg = strings.ReplaceAll(msg, "%type%", credData.Type)
		msg = strings.ReplaceAll(msg, "%storage%", credData.Storage)
		msg = strings.ReplaceAll(msg, "%host%", credData.Host)

		ts.sendToAllChannels(msg)
	}
}

func (ts *Teamserver) TsEventCallbackDownloads(downloadData adaptix.DownloadData) {
	if ts.Profile.Callbacks == nil || ts.Profile.Callbacks.NewDownloadMessage == "" || !ts.hasAnyCallback() {
		return
	}

	msg := ts.Profile.Callbacks.NewDownloadMessage
	msg = strings.ReplaceAll(msg, "%user%", downloadData.User)
	msg = strings.ReplaceAll(msg, "%computer%", downloadData.Computer)
	msg = strings.ReplaceAll(msg, "%path%", downloadData.RemotePath)
	msg = strings.ReplaceAll(msg, "%size%", tformat.SizeBytesToFormat(uint64(downloadData.TotalSize)))

	ts.sendToAllChannels(msg)
}

///

func sendTelegram(text, botToken, chatID string) {
	go func() {
		ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
		defer cancel()

		url := fmt.Sprintf("https://api.telegram.org/bot%s/sendMessage", botToken)
		payload := map[string]string{
			"chat_id": chatID,
			"text":    text,
		}
		data, _ := json.Marshal(payload)

		req, err := http.NewRequestWithContext(ctx, "POST", url, bytes.NewBuffer(data))
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

func sendWebhook(text string, webhook profile.WebhookConfig) {
	go func() {
		ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
		defer cancel()

		text = strings.ReplaceAll(webhook.Data, "%data%", text)

		req, err := http.NewRequestWithContext(ctx, webhook.Method, webhook.URL, strings.NewReader(text))
		if err != nil {
			return
		}

		for key, value := range webhook.Headers {
			req.Header.Set(key, value)
		}

		if _, hasContentType := webhook.Headers["Content-Type"]; !hasContentType {
			req.Header.Set("Content-Type", "text/plain")
		}

		resp, err := http.DefaultClient.Do(req)
		if err != nil {
			return
		}
		defer func(Body io.ReadCloser) {
			_ = Body.Close()
		}(resp.Body)
	}()
}

func sendSlack(text, webhookURL string) {
	go func() {
		ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
		defer cancel()

		payload := map[string]string{
			"text": text,
		}
		data, _ := json.Marshal(payload)

		req, err := http.NewRequestWithContext(ctx, "POST", webhookURL, bytes.NewBuffer(data))
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
