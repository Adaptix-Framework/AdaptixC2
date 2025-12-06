package server

import (
	"AdaptixServer/core/profile"
	"AdaptixServer/core/utils/tformat"
	"bytes"
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

func (ts *Teamserver) TsEventCallbackAgent(agentData adaptix.AgentData) {
	if ts.Profile.Callbacks.NewAgentMessage != "" {
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

		if ts.Profile.Callbacks.Telegram.Token != "" && ts.Profile.Callbacks.Telegram.ChatsId != nil {
			for _, chatId := range ts.Profile.Callbacks.Telegram.ChatsId {
				SendTelegram(msg, ts.Profile.Callbacks.Telegram.Token, chatId)
			}
		}

		for _, webhook := range ts.Profile.Callbacks.Webhooks {
			if webhook.URL != "" {
				SendWebhook(msg, webhook)
			}
		}
	}
}

func (ts *Teamserver) TsEventCallbackCreds(creds []adaptix.CredsData) {
	if ts.Profile.Callbacks.NewCredMessage != "" {

		if len(creds) > 4 {
			msg := fmt.Sprintf("Added %d new credentials", len(creds))
			if ts.Profile.Callbacks.Telegram.Token != "" && ts.Profile.Callbacks.Telegram.ChatsId != nil {
				for _, chatId := range ts.Profile.Callbacks.Telegram.ChatsId {
					SendTelegram(msg, ts.Profile.Callbacks.Telegram.Token, chatId)
				}
			}
			for _, webhook := range ts.Profile.Callbacks.Webhooks {
				if webhook.URL != "" {
					SendWebhook(msg, webhook)
				}
			}
		} else {
			for _, credData := range creds {
				secret := credData.Password[:3] + "*****"

				msg := ts.Profile.Callbacks.NewCredMessage
				msg = strings.ReplaceAll(msg, "%username%", credData.Username)
				msg = strings.ReplaceAll(msg, "%password%", secret)
				msg = strings.ReplaceAll(msg, "%domain%", credData.Realm)
				msg = strings.ReplaceAll(msg, "%type%", credData.Type)
				msg = strings.ReplaceAll(msg, "%storage%", credData.Storage)
				msg = strings.ReplaceAll(msg, "%host%", credData.Host)

				if ts.Profile.Callbacks.Telegram.Token != "" && ts.Profile.Callbacks.Telegram.ChatsId != nil {
					for _, chatId := range ts.Profile.Callbacks.Telegram.ChatsId {
						SendTelegram(msg, ts.Profile.Callbacks.Telegram.Token, chatId)
					}
				}
				for _, webhook := range ts.Profile.Callbacks.Webhooks {
					if webhook.URL != "" {
						SendWebhook(msg, webhook)
					}
				}
			}
		}
	}
}

func (ts *Teamserver) TsEventCallbackDownloads(downloadData adaptix.DownloadData) {
	if ts.Profile.Callbacks.NewDownloadMessage != "" {
		msg := ts.Profile.Callbacks.NewDownloadMessage
		msg = strings.ReplaceAll(msg, "%user%", downloadData.User)
		msg = strings.ReplaceAll(msg, "%computer%", downloadData.Computer)
		msg = strings.ReplaceAll(msg, "%path%", downloadData.RemotePath)
		msg = strings.ReplaceAll(msg, "%size%", tformat.SizeBytesToFormat(uint64(downloadData.TotalSize)))

		if ts.Profile.Callbacks.Telegram.Token != "" && ts.Profile.Callbacks.Telegram.ChatsId != nil {
			for _, chatId := range ts.Profile.Callbacks.Telegram.ChatsId {
				SendTelegram(msg, ts.Profile.Callbacks.Telegram.Token, chatId)
			}
		}
		for _, webhook := range ts.Profile.Callbacks.Webhooks {
			if webhook.URL != "" {
				SendWebhook(msg, webhook)
			}
		}
	}
}

///

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

func SendWebhook(text string, webhook profile.WebhookConfig) {
	go func() {

		text = strings.ReplaceAll(webhook.Data, "%data%", text)

		req, err := http.NewRequest(webhook.Method, webhook.URL, strings.NewReader(text))
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
