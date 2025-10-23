package client

import (
	"fmt"
	"sync"
	"time"

	"github.com/adaptix/adaptix_mcp/pkg/utils"
	"github.com/gorilla/websocket"
)

// Request Client MCP Bridge请求
type Request struct {
	RequestID string                 `json:"request_id"`
	Type      string                 `json:"type"`
	Params    map[string]interface{} `json:"params"`
}

// Response Client MCP Bridge响应
type Response struct {
	RequestID string                 `json:"request_id"`
	Status    string                 `json:"status"`
	Message   string                 `json:"message"`
	Data      map[string]interface{} `json:"data"`
	Version   string                 `json:"version"`
}

// Connector WebSocket连接器
type Connector struct {
	url     string
	conn    *websocket.Conn
	mu      sync.Mutex
	pending map[string]chan *Response

	reconnectInterval time.Duration
	timeout           time.Duration

	connected    bool
	reconnecting bool
	stopChan     chan struct{}
}

// NewConnector 创建连接器
func NewConnector(url string) *Connector {
	return &Connector{
		url:               url,
		pending:           make(map[string]chan *Response),
		reconnectInterval: 5 * time.Second,
		timeout:           30 * time.Second,
		stopChan:          make(chan struct{}),
	}
}

// Connect 连接到Client MCP Bridge
func (c *Connector) Connect() error {
	conn, _, err := websocket.DefaultDialer.Dial(c.url, nil)
	if err != nil {
		return fmt.Errorf("failed to connect to %s: %w", c.url, err)
	}

	c.conn = conn
	c.connected = true

	utils.InfoLogger.Printf("✅ Connected to Client MCP Bridge at %s", c.url)

	// 启动响应监听goroutine
	go c.listenResponses()

	return nil
}

// Close 关闭连接
func (c *Connector) Close() error {
	close(c.stopChan)
	c.connected = false

	if c.conn != nil {
		return c.conn.Close()
	}

	return nil
}

// SendCommand 发送命令到Client
func (c *Connector) SendCommand(commandType string, params map[string]interface{}) (*Response, error) {
	if !c.connected {
		return nil, fmt.Errorf("not connected to Client MCP Bridge")
	}

	requestID := generateID()

	request := Request{
		RequestID: requestID,
		Type:      commandType,
		Params:    params,
	}

	// 创建响应通道
	respChan := make(chan *Response, 1)

	// 注册pending（需要加锁）
	c.mu.Lock()
	c.pending[requestID] = respChan
	c.mu.Unlock()

	// 发送请求（不持有锁）
	if err := c.conn.WriteJSON(request); err != nil {
		c.mu.Lock()
		delete(c.pending, requestID)
		c.mu.Unlock()
		return nil, fmt.Errorf("failed to send command: %w", err)
	}

	utils.DebugLogger.Printf("→ Sent command: %s (ID: %s)", commandType, requestID)

	// 等待响应（带超时，不持有锁）
	select {
	case resp := <-respChan:
		if resp.Status == "success" {
			utils.DebugLogger.Printf("← Received response: %s (ID: %s)", commandType, requestID)
			return resp, nil
		} else {
			return nil, fmt.Errorf("command failed: %s", resp.Message)
		}
	case <-time.After(c.timeout):
		c.mu.Lock()
		delete(c.pending, requestID)
		c.mu.Unlock()
		return nil, fmt.Errorf("command timeout after %v", c.timeout)
	}
}

// listenResponses 监听WebSocket响应
func (c *Connector) listenResponses() {
	defer func() {
		c.mu.Lock()
		c.connected = false
		shouldReconnect := !c.reconnecting
		if shouldReconnect {
			c.reconnecting = true
		}
		c.mu.Unlock()

		if shouldReconnect {
			utils.WarnLogger.Println("Response listener stopped, will attempt reconnect...")
			// 自动重连
			go c.autoReconnect()
		}
	}()

	for {
		select {
		case <-c.stopChan:
			return
		default:
			var resp Response
			if err := c.conn.ReadJSON(&resp); err != nil {
				if websocket.IsCloseError(err, websocket.CloseNormalClosure, websocket.CloseGoingAway) {
					utils.InfoLogger.Printf("WebSocket closed normally: %v", err)
				} else {
					utils.ErrorLogger.Printf("Failed to read response: %v", err)
				}
				return
			}

			// 将响应发送到对应的通道
			c.mu.Lock()
			if ch, ok := c.pending[resp.RequestID]; ok {
				ch <- &resp
				close(ch)
				delete(c.pending, resp.RequestID)
			}
			c.mu.Unlock()
		}
	}
}

// autoReconnect 自动重连（带指数退避）
func (c *Connector) autoReconnect() {
	defer func() {
		c.mu.Lock()
		c.reconnecting = false
		c.mu.Unlock()
	}()

	backoff := c.reconnectInterval
	maxBackoff := 30 * time.Second
	maxRetries := 10
	retries := 0

	for retries < maxRetries {
		select {
		case <-c.stopChan:
			return
		default:
		}

		utils.InfoLogger.Printf("Attempting to reconnect in %v... (attempt %d/%d)", backoff, retries+1, maxRetries)
		time.Sleep(backoff)

		if err := c.Connect(); err != nil {
			utils.ErrorLogger.Printf("Failed to reconnect: %v", err)

			// 指数退避
			retries++
			backoff *= 2
			if backoff > maxBackoff {
				backoff = maxBackoff
			}
		} else {
			utils.InfoLogger.Println("✅ Reconnected successfully!")
			return
		}
	}

	utils.ErrorLogger.Printf("❌ Failed to reconnect after %d attempts, giving up", maxRetries)
}

// generateID 生成唯一ID
func generateID() string {
	return fmt.Sprintf("%d", time.Now().UnixNano())
}

// IsConnected 检查连接状态
func (c *Connector) IsConnected() bool {
	return c.connected
}
