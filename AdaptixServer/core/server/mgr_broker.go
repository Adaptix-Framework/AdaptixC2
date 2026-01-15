package server

import (
	"bytes"
	"encoding/json"
	"sync"
	"sync/atomic"
	"time"

	"github.com/gorilla/websocket"
)

const (
	PriorityNormal = 0
	PriorityHigh   = 1

	SendBufferSize     = 4096
	SyncBufferSize     = 8192
	BroadcastQueueSize = 8192

	WriteTimeout      = 300 * time.Second
	BackpressureWarn  = 0.75
	BackpressureDrop  = 0.95
	BackpressureClose = 1.0
)

type BrokerMessage struct {
	Data     []byte
	Target   string
	Exclude  string
	Priority int
}

type ClientHandler struct {
	username       string
	socket         *websocket.Conn
	versionSupport bool

	sendChan chan []byte
	syncChan chan []byte
	done     chan struct{}

	synced   atomic.Bool
	tmpStore []interface{}
	tmpMu    sync.Mutex

	stats ClientStats

	broker *MessageBroker
}

type ClientStats struct {
	MessagesSent    atomic.Uint64
	MessagesDropped atomic.Uint64
	BytesSent       atomic.Uint64
	LastActivity    atomic.Int64
}

type MessageBroker struct {
	mu         sync.RWMutex
	clients    map[string]*ClientHandler
	broadcast  chan *BrokerMessage
	register   chan *ClientHandler
	unregister chan string
	shutdown   chan struct{}
	wg         sync.WaitGroup
	running    atomic.Bool
}

func NewMessageBroker() *MessageBroker {
	return &MessageBroker{
		clients:    make(map[string]*ClientHandler),
		broadcast:  make(chan *BrokerMessage, BroadcastQueueSize),
		register:   make(chan *ClientHandler, 64),
		unregister: make(chan string, 64),
		shutdown:   make(chan struct{}),
	}
}

func (mb *MessageBroker) Start() {
	if mb.running.Swap(true) {
		return
	}

	mb.wg.Add(1)
	go mb.run()
}

func (mb *MessageBroker) Stop() {
	if !mb.running.Swap(false) {
		return
	}

	close(mb.shutdown)
	mb.wg.Wait()

	mb.mu.Lock()
	for _, client := range mb.clients {
		client.Close()
	}
	mb.clients = make(map[string]*ClientHandler)
	mb.mu.Unlock()
}

func (mb *MessageBroker) run() {
	defer mb.wg.Done()

	for {
		select {
		case <-mb.shutdown:
			return

		case client := <-mb.register:
			mb.mu.Lock()
			if existing, ok := mb.clients[client.username]; ok {
				existing.Close()
			}
			mb.clients[client.username] = client
			mb.mu.Unlock()

		case username := <-mb.unregister:
			mb.mu.Lock()
			if client, ok := mb.clients[username]; ok {
				client.Close()
				delete(mb.clients, username)
			}
			mb.mu.Unlock()

		case msg := <-mb.broadcast:
			mb.dispatch(msg)
		}
	}
}

func (mb *MessageBroker) dispatch(msg *BrokerMessage) {
	mb.mu.RLock()
	defer mb.mu.RUnlock()

	if msg.Target != "" {
		if client, ok := mb.clients[msg.Target]; ok {
			client.Send(msg.Data, msg.Priority)
		}
		return
	}

	for operator, client := range mb.clients {
		if msg.Exclude != operator {
			if client.synced.Load() {
				client.Send(msg.Data, msg.Priority)
			} else {
				client.BufferForSync(msg.Data)
			}
		}
	}
}

func (mb *MessageBroker) Publish(packet interface{}) {
	data := serializePacket(packet)
	if data == nil {
		return
	}

	msg := &BrokerMessage{
		Data:     data,
		Priority: PriorityNormal,
	}

	select {
	case mb.broadcast <- msg:
	default:
	}
}

func (mb *MessageBroker) PublishTo(username string, packet interface{}) {
	data := serializePacket(packet)
	if data == nil {
		return
	}

	msg := &BrokerMessage{
		Data:     data,
		Target:   username,
		Priority: PriorityNormal,
	}

	select {
	case mb.broadcast <- msg:
	default:
	}
}

func (mb *MessageBroker) PublishExclude(username string, packet interface{}) {
	data := serializePacket(packet)
	if data == nil {
		return
	}

	msg := &BrokerMessage{
		Data:     data,
		Exclude:  username,
		Priority: PriorityNormal,
	}

	select {
	case mb.broadcast <- msg:
	default:
	}
}

func (mb *MessageBroker) PublishDirect(data []byte) {
	msg := &BrokerMessage{
		Data:     data,
		Priority: PriorityNormal,
	}

	select {
	case mb.broadcast <- msg:
	default:
	}
}

func (mb *MessageBroker) PublishDirectTo(username string, data []byte) {
	msg := &BrokerMessage{
		Data:     data,
		Target:   username,
		Priority: PriorityNormal,
	}

	select {
	case mb.broadcast <- msg:
	default:
	}
}

func (mb *MessageBroker) Register(client *ClientHandler) {
	select {
	case mb.register <- client:
	case <-mb.shutdown:
	}
}

func (mb *MessageBroker) Unregister(username string) {
	select {
	case mb.unregister <- username:
	case <-mb.shutdown:
	}
}

func (mb *MessageBroker) GetClient(username string) (*ClientHandler, bool) {
	mb.mu.RLock()
	defer mb.mu.RUnlock()
	client, ok := mb.clients[username]
	return client, ok
}

func (mb *MessageBroker) ClientExists(username string) bool {
	mb.mu.RLock()
	defer mb.mu.RUnlock()
	_, ok := mb.clients[username]
	return ok
}

func (mb *MessageBroker) ForEachClient(fn func(username string, client *ClientHandler) bool) {
	mb.mu.RLock()
	clients := make(map[string]*ClientHandler, len(mb.clients))
	for k, v := range mb.clients {
		clients[k] = v
	}
	mb.mu.RUnlock()

	for username, client := range clients {
		if !fn(username, client) {
			break
		}
	}
}

func serializePacket(packet interface{}) []byte {
	var buffer bytes.Buffer
	encoder := json.NewEncoder(&buffer)
	encoder.SetEscapeHTML(false)
	if err := encoder.Encode(packet); err != nil {
		return nil
	}
	return buffer.Bytes()
}

func NewClientHandler(username string, socket *websocket.Conn, versionSupport bool, broker *MessageBroker) *ClientHandler {
	ch := &ClientHandler{
		username:       username,
		socket:         socket,
		versionSupport: versionSupport,
		sendChan:       make(chan []byte, SendBufferSize),
		syncChan:       make(chan []byte, SyncBufferSize),
		done:           make(chan struct{}),
		tmpStore:       make([]interface{}, 0),
		broker:         broker,
	}
	ch.stats.LastActivity.Store(time.Now().Unix())
	return ch
}

func (ch *ClientHandler) Start() {
	go ch.writeLoop()
}

func (ch *ClientHandler) Close() {
	select {
	case <-ch.done:
		return
	default:
		close(ch.done)
	}

	ch.socket.Close()
}

func (ch *ClientHandler) IsClosed() bool {
	select {
	case <-ch.done:
		return true
	default:
		return false
	}
}

func (ch *ClientHandler) writeLoop() {
	defer ch.socket.Close()

	for {
		select {
		case <-ch.done:
			return

		case data := <-ch.syncChan:
			if err := ch.writeToSocket(data); err != nil {
				ch.broker.Unregister(ch.username)
				return
			}

		case data := <-ch.sendChan:
			if err := ch.writeToSocket(data); err != nil {
				ch.broker.Unregister(ch.username)
				return
			}
		}
	}
}

func (ch *ClientHandler) writeToSocket(data []byte) error {
	ch.socket.SetWriteDeadline(time.Now().Add(WriteTimeout))
	err := ch.socket.WriteMessage(websocket.BinaryMessage, data)
	if err == nil {
		ch.stats.MessagesSent.Add(1)
		ch.stats.BytesSent.Add(uint64(len(data)))
		ch.stats.LastActivity.Store(time.Now().Unix())
	}
	return err
}

func (ch *ClientHandler) Send(data []byte, priority int) {
	if ch.IsClosed() {
		return
	}

	targetChan := ch.sendChan
	if priority == PriorityHigh {
		targetChan = ch.syncChan
	}

	bufferUsage := float64(len(targetChan)) / float64(cap(targetChan))

	if bufferUsage >= BackpressureClose {
		ch.stats.MessagesDropped.Add(1)
		return
	}

	if bufferUsage >= BackpressureDrop {
		select {
		case targetChan <- data:
		default:
			ch.stats.MessagesDropped.Add(1)
		}
		return
	}

	select {
	case targetChan <- data:
	case <-ch.done:
	default:
		ch.stats.MessagesDropped.Add(1)
	}
}

func (ch *ClientHandler) SendSync(data []byte) {
	if ch.IsClosed() {
		return
	}

	select {
	case ch.syncChan <- data:
	case <-ch.done:
	case <-time.After(30 * time.Second):
		// Use timeout to prevent infinite blocking when buffer is full
		// If we can't send within 30 seconds, the client is likely too slow
		ch.stats.MessagesDropped.Add(1)
	}
}

func (ch *ClientHandler) BufferForSync(data interface{}) {
	ch.tmpMu.Lock()
	ch.tmpStore = append(ch.tmpStore, data)
	ch.tmpMu.Unlock()
}

func (ch *ClientHandler) GetAndClearBuffer() []interface{} {
	ch.tmpMu.Lock()
	defer ch.tmpMu.Unlock()

	buf := ch.tmpStore
	ch.tmpStore = make([]interface{}, 0)
	return buf
}

func (ch *ClientHandler) SetSynced(synced bool) {
	ch.synced.Store(synced)
}

func (ch *ClientHandler) IsSynced() bool {
	return ch.synced.Load()
}

func (ch *ClientHandler) Username() string {
	return ch.username
}

func (ch *ClientHandler) VersionSupport() bool {
	return ch.versionSupport
}

func (ch *ClientHandler) Socket() *websocket.Conn {
	return ch.socket
}

func (ch *ClientHandler) Stats() ClientStats {
	return ch.stats
}
