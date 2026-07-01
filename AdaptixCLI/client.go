package main

import (
	"bytes"
	"crypto/tls"
	"encoding/json"
	"fmt"
	"io"
	"net/http"

	"net/url"
	"time"

	"github.com/gorilla/websocket"
)

const clientType uint8 = 3

type Client struct {
	cfg          *Config
	baseURL      string
	wsURL        string
	accessToken  string
	refreshToken string

	httpClient *http.Client
	wsConn     *websocket.Conn
}

type apiResponse struct {
	OK      bool   `json:"ok"`
	Message string `json:"message"`
}

func newClient(cfg *Config) *Client {
	transport := &http.Transport{
		TLSClientConfig: &tls.Config{
			InsecureSkipVerify: true,
			MinVersion:         tls.VersionTLS12,
		},
	}
	return &Client{
		cfg:    cfg,
		baseURL: fmt.Sprintf("https://%s:%d%s", cfg.Host, cfg.Port, cfg.Endpoint),
		wsURL:  fmt.Sprintf("wss://%s:%d%s", cfg.Host, cfg.Port, cfg.Endpoint),
		httpClient: &http.Client{
			Transport: transport,
			Timeout:   30 * time.Second,
		},
	}
}

func (c *Client) doJSON(method, path string, body interface{}) ([]byte, error) {
	var bodyReader io.Reader
	if body != nil {
		data, err := json.Marshal(body)
		if err != nil {
			return nil, fmt.Errorf("marshal body: %w", err)
		}
		bodyReader = bytes.NewReader(data)
	}

	req, err := http.NewRequest(method, c.baseURL+path, bodyReader)
	if err != nil {
		return nil, fmt.Errorf("create request: %w", err)
	}
	req.Header.Set("Content-Type", "application/json")
	if c.accessToken != "" {
		req.Header.Set("Authorization", "Bearer "+c.accessToken)
	}

	resp, err := c.httpClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("request failed: %w", err)
	}
	defer resp.Body.Close()

	respBody, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("read response: %w", err)
	}

	if resp.StatusCode == http.StatusNetworkAuthenticationRequired {
		return nil, fmt.Errorf("already connected from another session")
	}
	if resp.StatusCode >= 400 {
		return nil, fmt.Errorf("HTTP %d: %s", resp.StatusCode, string(respBody))
	}

	return respBody, nil
}

func (c *Client) doAPI(method, path string, body interface{}) (*apiResponse, []byte, error) {
	respBody, err := c.doJSON(method, path, body)
	if err != nil {
		return nil, nil, err
	}

	var apiResp apiResponse
	if err := json.Unmarshal(respBody, &apiResp); err == nil && apiResp.Message != "" || !apiResp.OK {
		if !apiResp.OK && apiResp.Message != "" {
			return &apiResp, nil, fmt.Errorf("%s", apiResp.Message)
		}
		return &apiResp, respBody, nil
	}

	return &apiResp, respBody, nil
}

func (c *Client) login() error {
	respBody, err := c.doJSON("POST", "/login", map[string]string{
		"username": c.cfg.Username,
		"password": c.cfg.Password,
	})
	if err != nil {
		return err
	}

	var result struct {
		AccessToken  string `json:"access_token"`
		RefreshToken string `json:"refresh_token"`
		Version      string `json:"version"`
	}
	if err := json.Unmarshal(respBody, &result); err != nil {
		return fmt.Errorf("parse login response: %w", err)
	}
	c.accessToken = result.AccessToken
	c.refreshToken = result.RefreshToken
	return nil
}

func (c *Client) getOTP(otpType string, data interface{}) (string, error) {
	respBody, err := c.doJSON("POST", "/otp/generate", map[string]interface{}{
		"type": otpType,
		"data": data,
	})
	if err != nil {
		return "", err
	}

	var result struct {
		OK      bool   `json:"ok"`
		Message string `json:"message"`
	}
	json.Unmarshal(respBody, &result)
	if !result.OK {
		return "", fmt.Errorf("OTP failed: %s", result.Message)
	}
	return result.Message, nil
}

func (c *Client) connectChannelWebSocket(otpType string, data interface{}) (*websocket.Conn, error) {
	otp, err := c.getOTP(otpType, data)
	if err != nil {
		return nil, fmt.Errorf("get channel OTP: %w", err)
	}

	u, _ := url.Parse(c.wsURL + "/channel")
	q := u.Query()
	q.Set("otp", otp)
	u.RawQuery = q.Encode()

	dialer := websocket.Dialer{
		TLSClientConfig:  &tls.Config{InsecureSkipVerify: true},
		HandshakeTimeout: 10 * time.Second,
	}
	conn, resp, err := dialer.Dial(u.String(), nil)
	if err != nil {
		if resp != nil && resp.StatusCode == http.StatusNetworkAuthenticationRequired {
			return nil, fmt.Errorf("channel connect failed (main client not connected)")
		}
		return nil, fmt.Errorf("channel dial: %w", err)
	}
	return conn, nil
}

func (c *Client) connectWebSocket() error {
	otpData := map[string]interface{}{
		"client_type":    clientType,
		"console_team_mode": false,
		"subscriptions":  []string{"agents", "console", "tasks", "downloads", "listeners"},
	}
	otp, err := c.getOTP("connect", otpData)
	if err != nil {
		return fmt.Errorf("get OTP: %w", err)
	}

	u, _ := url.Parse(c.wsURL + "/connect")
	q := u.Query()
	q.Set("otp", otp)
	u.RawQuery = q.Encode()

	dialer := websocket.Dialer{
		TLSClientConfig:  &tls.Config{InsecureSkipVerify: true},
		HandshakeTimeout: 10 * time.Second,
	}
	conn, resp, err := dialer.Dial(u.String(), nil)
	if err != nil {
		if resp != nil && resp.StatusCode == http.StatusNetworkAuthenticationRequired {
			return fmt.Errorf("client already connected - disconnect the GUI client first")
		}
		return fmt.Errorf("websocket dial: %w", err)
	}
	c.wsConn = conn
	return nil
}

func (c *Client) waitSync() error {
	if c.wsConn == nil {
		return fmt.Errorf("not connected")
	}

	for {
		_, msg, err := c.wsConn.ReadMessage()
		if err != nil {
			c.wsConn = nil
			return fmt.Errorf("ws read during sync: %w", err)
		}

		var packet struct {
			Type int `json:"type"`
		}
		if err := json.Unmarshal(msg, &packet); err != nil {
			continue
		}

		if packet.Type == 0x12 {
			return nil
		}
	}
}

func (c *Client) wsReadMessage() ([]byte, error) {
	if c.wsConn == nil {
		return nil, fmt.Errorf("not connected")
	}
	_, msg, err := c.wsConn.ReadMessage()
	if err != nil {
		c.wsConn = nil
	}
	return msg, err
}

func (c *Client) disconnect() {
	if c.wsConn != nil {
		c.wsConn.Close()
		c.wsConn = nil
	}
}

func (c *Client) httpDo(req *http.Request) (*http.Response, error) {
	return c.httpClient.Do(req)
}


