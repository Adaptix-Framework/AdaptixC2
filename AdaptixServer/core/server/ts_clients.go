package server

import (
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/safe"
	"bytes"
	"encoding/json"
	"sync"
	"time"

	"github.com/gorilla/websocket"
)

func (ts *Teamserver) TsClientExists(username string) bool {
	return ts.clients.Contains(username)
}

func (ts *Teamserver) TsClientConnect(username string, socket *websocket.Conn) {
	client := &Client{
		username:   username,
		synced:     false,
		lockSocket: &sync.Mutex{},
		socket:     socket,
		tmp_store:  safe.NewSlice(),
		msgQueue:   make(chan interface{}, 256), // 缓冲256条消息
		done:       make(chan bool),
	}

	ts.clients.Put(username, client)
	
	// 启动异步发送goroutine
	go ts.TsClientMessageSender(username, client)
}

func (ts *Teamserver) TsClientDisconnect(username string) {
	value, ok := ts.clients.GetDelete(username)
	if !ok {
		return
	}
	client := value.(*Client)

	client.synced = false
	close(client.done) // 通知发送goroutine退出
	client.socket.Close()

	ts.TsEventClient(false, username)

	var tunnels []string
	ts.tunnels.ForEach(func(key string, value interface{}) bool {
		tunnel := value.(*Tunnel)
		if tunnel.Data.Client == username {
			tunnels = append(tunnels, tunnel.Data.TunnelId)
		}
		return true
	})
	for _, id := range tunnels {
		_ = ts.TsTunnelStop(id)
	}
}

func (ts *Teamserver) TsClientSync(username string) {
	value, ok := ts.clients.Get(username)
	if !ok {
		return
	}
	client := value.(*Client)

	if !client.synced {
		ts.TsSyncStored(client)

		for {
			if client.tmp_store.Len() > 0 {
				arr := client.tmp_store.CutArray()
				for _, v := range arr {
					select {
					case client.msgQueue <- v:
					default:
						logs.Warn("", "Client %s message queue full, dropping message", username)
					}
				}
			} else {
				client.synced = true
				break
			}
		}
	}

	ts.TsEventClient(true, username)
}

// 异步消息发送循环
func (ts *Teamserver) TsClientMessageSender(username string, client *Client) {
	for {
		select {
		case <-client.done:
			return
		case msg := <-client.msgQueue:
			var buffer bytes.Buffer
			err := json.NewEncoder(&buffer).Encode(msg)
			if err != nil {
				continue
			}

			client.lockSocket.Lock()
			client.socket.SetWriteDeadline(time.Now().Add(10 * time.Second))
			err = client.socket.WriteMessage(websocket.BinaryMessage, buffer.Bytes())
			client.lockSocket.Unlock()

			if err != nil {
				tc.teamserver.TsClientDisconnect(username)
				return
			}
		}
	}
}
