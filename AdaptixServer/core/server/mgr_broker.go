package server

import (
	"bytes"
	"encoding/json"
	"sync"
	"sync/atomic"
	"time"

	adaptix "github.com/Adaptix-Framework/axc2"
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

var bufferPool = sync.Pool{
	New: func() interface{} {
		return new(bytes.Buffer)
	},
}

type ClientType uint8

const (
	ClientTypeUI  ClientType = 1
	ClientTypeWEB ClientType = 2
	ClientTypeCLI ClientType = 3
)

const (
	SyncCategoryExtenders           = "extenders"
	SyncCategoryListeners           = "listeners"
	SyncCategoryAgents              = "agents"
	SyncCategoryAgentsOnlyActive    = "agents_only_active"
	SyncCategoryAgentsInactive      = "agents_inactive"
	SyncCategoryPivots              = "pivots"
	SyncCategoryTasksHistory        = "tasks_history"
	SyncCategoryTasksManager        = "tasks_manager"
	SyncCategoryTasksOnlyJobs       = "tasks_only_jobs"
	SyncCategoryConsoleHistory      = "console_history"
	SyncCategoryChatHistory         = "chat_history"
	SyncCategoryChatRealtime        = "chat_realtime"
	SyncCategoryDownloadsHistory    = "downloads_history"
	SyncCategoryDownloadsRealtime   = "downloads_realtime"
	SyncCategoryScreenshotHistory   = "screenshot_history"
	SyncCategoryScreenshotRealtime  = "screenshot_realtime"
	SyncCategoryCredentialsHistory  = "credentials_history"
	SyncCategoryCredentialsRealtime = "credentials_realtime"
	SyncCategoryTargetsHistory      = "targets_history"
	SyncCategoryTargetsRealtime     = "targets_realtime"
	SyncCategoryNotifications       = "notifications"
	SyncCategoryTunnels             = "tunnels"
)

const (
	MsgTypeEvent = iota // Create, Remove, Task, Console
	MsgTypeState        // Update, Tick
)

type BrokerMessage struct {
	Data       []byte
	Target     string
	Exclude    string
	Priority   int
	Category   string
	TaskClient string
	RawPacket  interface{}
	MsgType    int    // MsgTypeEvent, MsgTypeState
	StateKey   string // state-msg
	TaskType   int
}

func taskTypeFromPacket(packet interface{}) int {
	switch p := packet.(type) {
	case SyncPackerAgentTaskSync:
		return p.TaskType
	case SyncPackerAgentTaskUpdate:
		return p.TaskType
	default:
		return 0
	}
}

type ClientHandler struct {
	username       string
	socket         *websocket.Conn
	socketMu       sync.Mutex
	versionSupport bool

	sendChan chan []byte
	syncChan chan []byte
	done     chan struct{}
	ready    chan struct{}

	synced        atomic.Bool
	tmpStore      []interface{}     // Event-buff
	tmpStateStore map[string][]byte // State-buff: key → last value
	tmpMu         sync.Mutex

	stats ClientStats

	broker *MessageBroker

	clientType      uint8
	role            string
	subscriptions   map[string]bool
	subMu           sync.RWMutex
	consoleTeamMode bool
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
			if msg.Category == SyncCategoryAgents {
				if !client.IsSubscribed(SyncCategoryAgents) && !client.IsSubscribed(SyncCategoryAgentsOnlyActive) && !client.IsSubscribed(SyncCategoryAgentsInactive) {
					continue
				}
			} else if msg.Category == SyncCategoryConsoleHistory {
				if !client.ConsoleTeamMode() {
					if msg.TaskClient == "" || msg.TaskClient != operator {
						continue
					}
				}
			} else if (msg.Category == SyncCategoryTasksHistory || msg.Category == SyncCategoryTasksManager) && client.IsSubscribed(SyncCategoryTasksOnlyJobs) {
				if msg.TaskType != adaptix.TASK_TYPE_JOB {
					continue
				}
			} else if msg.Category != "" && !client.IsSubscribed(msg.Category) {
				continue
			}
			if client.synced.Load() {
				client.Send(msg.Data, msg.Priority)
			} else {
				client.BufferForSync(msg.Data, msg.MsgType, msg.StateKey)
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

func (mb *MessageBroker) PublishStateWithCategory(packet interface{}, stateKey string, category string) {
	data := serializePacket(packet)
	if data == nil {
		return
	}

	msg := &BrokerMessage{
		Data:     data,
		Priority: PriorityNormal,
		Category: category,
		MsgType:  MsgTypeState,
		StateKey: stateKey,
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

func (mb *MessageBroker) PublishState(packet interface{}, stateKey string) {
	data := serializePacket(packet)
	if data == nil {
		return
	}

	msg := &BrokerMessage{
		Data:     data,
		Priority: PriorityNormal,
		MsgType:  MsgTypeState,
		StateKey: stateKey,
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

func (mb *MessageBroker) PublishWithCategory(packet interface{}, category string) {
	data := serializePacket(packet)
	if data == nil {
		return
	}

	msg := &BrokerMessage{
		Data:     data,
		Priority: PriorityNormal,
		Category: category,
		TaskType: taskTypeFromPacket(packet),
	}

	select {
	case mb.broadcast <- msg:
	default:
	}
}

func (mb *MessageBroker) PublishExcludeWithCategory(username string, packet interface{}, category string) {
	data := serializePacket(packet)
	if data == nil {
		return
	}

	msg := &BrokerMessage{
		Data:     data,
		Exclude:  username,
		Priority: PriorityNormal,
		Category: category,
		TaskType: taskTypeFromPacket(packet),
	}

	select {
	case mb.broadcast <- msg:
	default:
	}
}

func (mb *MessageBroker) PublishConsole(packet interface{}, taskClient string) {
	data := serializePacket(packet)
	if data == nil {
		return
	}

	msg := &BrokerMessage{
		Data:       data,
		Priority:   PriorityNormal,
		Category:   SyncCategoryConsoleHistory,
		TaskClient: taskClient,
	}

	select {
	case mb.broadcast <- msg:
	default:
	}
}

func (mb *MessageBroker) PublishAgentActivated(packet interface{}) {
	data := serializePacket(packet)
	if data == nil {
		return
	}

	mb.mu.RLock()
	defer mb.mu.RUnlock()

	for _, client := range mb.clients {
		if client.IsSubscribed(SyncCategoryAgents) && !client.IsSubscribed(SyncCategoryAgentsOnlyActive) {
			if client.synced.Load() {
				client.Send(data, PriorityNormal)
			} else {
				client.BufferForSync(data, MsgTypeEvent, "")
			}
		}
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
	buffer := bufferPool.Get().(*bytes.Buffer)
	buffer.Reset()
	defer bufferPool.Put(buffer)

	encoder := json.NewEncoder(buffer)
	encoder.SetEscapeHTML(false)
	if err := encoder.Encode(packet); err != nil {
		return nil
	}

	result := make([]byte, buffer.Len())
	copy(result, buffer.Bytes())
	return result
}

func NewClientHandler(username string, socket *websocket.Conn, versionSupport bool, broker *MessageBroker, clientType uint8, consoleTeamMode bool) *ClientHandler {
	ch := &ClientHandler{
		username:        username,
		socket:          socket,
		versionSupport:  versionSupport,
		sendChan:        make(chan []byte, SendBufferSize),
		syncChan:        make(chan []byte, SyncBufferSize),
		done:            make(chan struct{}),
		ready:           make(chan struct{}),
		tmpStore:        make([]interface{}, 0),
		tmpStateStore:   make(map[string][]byte),
		broker:          broker,
		clientType:      clientType,
		subscriptions:   make(map[string]bool),
		consoleTeamMode: consoleTeamMode,
	}
	ch.stats.LastActivity.Store(time.Now().Unix())
	return ch
}

func (ch *ClientHandler) Start() {
	go ch.writeLoop()
	<-ch.ready
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

	close(ch.ready)

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
	ch.socketMu.Lock()
	defer ch.socketMu.Unlock()

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

	if err := ch.writeToSocket(data); err != nil {
		ch.broker.Unregister(ch.username)
	}
}

func (ch *ClientHandler) BufferForSync(data interface{}, msgType int, stateKey string) {
	ch.tmpMu.Lock()
	defer ch.tmpMu.Unlock()

	if msgType == MsgTypeState && stateKey != "" {
		if dataBytes, ok := data.([]byte); ok {
			ch.tmpStateStore[stateKey] = dataBytes
		}
	} else {
		ch.tmpStore = append(ch.tmpStore, data)
	}
}

func (ch *ClientHandler) GetAndClearBuffer() []interface{} {
	ch.tmpMu.Lock()
	defer ch.tmpMu.Unlock()

	buf := ch.tmpStore
	ch.tmpStore = make([]interface{}, 0)
	return buf
}

func (ch *ClientHandler) GetAndClearStateBuffer() map[string][]byte {
	ch.tmpMu.Lock()
	defer ch.tmpMu.Unlock()

	buf := ch.tmpStateStore
	ch.tmpStateStore = make(map[string][]byte)
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

func (ch *ClientHandler) Stats() *ClientStats {
	return &ch.stats
}

func (ch *ClientHandler) Subscribe(categories ...string) {
	ch.subMu.Lock()
	defer ch.subMu.Unlock()
	for _, cat := range categories {
		ch.subscriptions[cat] = true
	}
}

func (ch *ClientHandler) IsSubscribed(category string) bool {
	ch.subMu.RLock()
	defer ch.subMu.RUnlock()
	return ch.subscriptions[category]
}

func (ch *ClientHandler) GetSubscriptions() []string {
	ch.subMu.RLock()
	defer ch.subMu.RUnlock()
	subs := make([]string, 0, len(ch.subscriptions))
	for cat := range ch.subscriptions {
		subs = append(subs, cat)
	}
	return subs
}

func (ch *ClientHandler) ClientType() uint8 {
	return ch.clientType
}

func (ch *ClientHandler) Role() string {
	return ch.role
}

func (ch *ClientHandler) SetRole(role string) {
	ch.role = role
}

func (ch *ClientHandler) ConsoleTeamMode() bool {
	return ch.consoleTeamMode
}

func (ch *ClientHandler) SetConsoleTeamMode(teamMode bool) {
	ch.consoleTeamMode = teamMode
}
