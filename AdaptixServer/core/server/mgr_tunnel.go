package server

import (
	"context"
	"io"
	"net"
	"strconv"
	"sync"
	"sync/atomic"

	"github.com/Adaptix-Framework/axc2"
	"github.com/gorilla/websocket"
)

const (
	TunnelBufferSize    = 0x8000
	TunnelBufferPoolCap = 256
)

type TunnelManager struct {
	ts *Teamserver

	tunnels      sync.Map // tunnelId string -> *Tunnel
	channelIndex sync.Map // "tunnelId:channelId" string -> *ChannelEntry

	bufferPool sync.Pool

	stats TunnelStats
}

func channelKey(tunnelId string, channelId int) string {
	return tunnelId + ":" + strconv.Itoa(channelId)
}

type ChannelEntry struct {
	TunnelId string
	Tunnel   *Tunnel
	Channel  *TunnelChannel
}

type TunnelStats struct {
	ActiveTunnels  atomic.Int64
	ActiveChannels atomic.Int64
	TotalBytesSent atomic.Uint64
	TotalBytesRecv atomic.Uint64
}

type TunnelChannelSafe struct {
	TunnelChannel
	mu     sync.Mutex
	closed atomic.Bool
	ctx    context.Context
	cancel context.CancelFunc
}

func NewTunnelManager(ts *Teamserver) *TunnelManager {
	tm := &TunnelManager{
		ts: ts,
		bufferPool: sync.Pool{
			New: func() interface{} {
				buf := make([]byte, TunnelBufferSize)
				return &buf
			},
		},
	}
	return tm
}

func (tm *TunnelManager) GetBuffer() []byte {
	return *tm.bufferPool.Get().(*[]byte)
}

func (tm *TunnelManager) PutBuffer(buf []byte) {
	if cap(buf) >= TunnelBufferSize {
		tm.bufferPool.Put(&buf)
	}
}

func (tm *TunnelManager) GetTunnel(tunnelId string) (*Tunnel, bool) {
	value, ok := tm.tunnels.Load(tunnelId)
	if !ok {
		return nil, false
	}
	return value.(*Tunnel), true
}

func (tm *TunnelManager) PutTunnel(tunnel *Tunnel) {
	tm.tunnels.Store(tunnel.Data.TunnelId, tunnel)
	tm.stats.ActiveTunnels.Add(1)
}

func (tm *TunnelManager) DeleteTunnel(tunnelId string) (*Tunnel, bool) {
	value, ok := tm.tunnels.LoadAndDelete(tunnelId)
	if !ok {
		return nil, false
	}
	tm.stats.ActiveTunnels.Add(-1)
	return value.(*Tunnel), true
}

func (tm *TunnelManager) TunnelExists(tunnelId string) bool {
	_, ok := tm.tunnels.Load(tunnelId)
	return ok
}

func (tm *TunnelManager) ForEachTunnel(fn func(tunnelId string, tunnel *Tunnel) bool) {
	tm.tunnels.Range(func(key, value interface{}) bool {
		return fn(key.(string), value.(*Tunnel))
	})
}

func (tm *TunnelManager) RegisterChannel(tunnelId string, tunnel *Tunnel, channel *TunnelChannel) {
	entry := &ChannelEntry{
		TunnelId: tunnelId,
		Tunnel:   tunnel,
		Channel:  channel,
	}
	key := channelKey(tunnelId, channel.channelId)
	tm.channelIndex.Store(key, entry)
	tunnel.connections.Put(strconv.Itoa(channel.channelId), channel)
	tm.stats.ActiveChannels.Add(1)
}

func (tm *TunnelManager) UnregisterChannel(tunnelId string, channelId int) {
	key := channelKey(tunnelId, channelId)
	if value, ok := tm.channelIndex.LoadAndDelete(key); ok {
		entry := value.(*ChannelEntry)
		entry.Tunnel.connections.Delete(strconv.Itoa(channelId))
		tm.stats.ActiveChannels.Add(-1)
	}
}

func (tm *TunnelManager) GetChannel(tunnelId string, channelId int) (*ChannelEntry, bool) {
	key := channelKey(tunnelId, channelId)
	value, ok := tm.channelIndex.Load(key)
	if !ok {
		return nil, false
	}
	return value.(*ChannelEntry), true
}

func (tm *TunnelManager) GetChannelByIdOnly(channelId int) (*ChannelEntry, bool) {
	var result *ChannelEntry
	found := false
	tm.channelIndex.Range(func(key, value interface{}) bool {
		entry := value.(*ChannelEntry)
		if entry.Channel.channelId == channelId {
			result = entry
			found = true
			return false
		}
		return true
	})
	return result, found
}

func (tm *TunnelManager) ChannelExists(tunnelId string, channelId int) bool {
	key := channelKey(tunnelId, channelId)
	_, ok := tm.channelIndex.Load(key)
	return ok
}

func (tm *TunnelManager) ChannelExistsInTunnel(tunnel *Tunnel, channelId int) bool {
	return tunnel.connections.Contains(strconv.Itoa(channelId))
}

func (tm *TunnelManager) CloseChannel(tunnelId string, channelId int) {
	entry, ok := tm.GetChannel(tunnelId, channelId)
	if !ok {
		return
	}
	tm.closeChannelInternal(entry.Tunnel, entry.Channel)
}

func (tm *TunnelManager) CloseChannelByIdOnly(channelId int) {
	entry, ok := tm.GetChannelByIdOnly(channelId)
	if !ok {
		return
	}
	tm.closeChannelInternal(entry.Tunnel, entry.Channel)
}

func (tm *TunnelManager) closeChannelInternal(tunnel *Tunnel, channel *TunnelChannel) {
	if channel == nil {
		return
	}

	if channel.conn != nil {
		_ = channel.conn.Close()
	}
	if channel.wsconn != nil {
		_ = channel.wsconn.Close()
	}

	if channel.pwTun != nil {
		_ = channel.pwTun.Close()
	}
	if channel.prTun != nil {
		_ = channel.prTun.Close()
	}
	if channel.pwSrv != nil {
		_ = channel.pwSrv.Close()
	}
	if channel.prSrv != nil {
		_ = channel.prSrv.Close()
	}

	if tunnel != nil {
		tm.UnregisterChannel(tunnel.Data.TunnelId, channel.channelId)
	}
}

func (tm *TunnelManager) CloseAllChannels(tunnel *Tunnel) {
	var channelKeys []string
	tm.channelIndex.Range(func(key, value interface{}) bool {
		entry := value.(*ChannelEntry)
		if entry.TunnelId == tunnel.Data.TunnelId {
			channelKeys = append(channelKeys, key.(string))
		}
		return true
	})

	for _, k := range channelKeys {
		if value, ok := tm.channelIndex.LoadAndDelete(k); ok {
			entry := value.(*ChannelEntry)
			tm.closeChannelInternal(nil, entry.Channel)
			tm.stats.ActiveChannels.Add(-1)
		}
	}

	tunnel.connections.CutMap()
}

func (tm *TunnelManager) WriteToChannel(tunnelId string, channelId int, data []byte) bool {
	entry, ok := tm.GetChannel(tunnelId, channelId)
	if !ok {
		return false
	}

	if entry.Channel.pwTun != nil {
		_, err := entry.Channel.pwTun.Write(data)
		if err == nil {
			tm.stats.TotalBytesRecv.Add(uint64(len(data)))
			return true
		}
	}
	return false
}

func (tm *TunnelManager) WriteToChannelByIdOnly(channelId int, data []byte) bool {
	entry, ok := tm.GetChannelByIdOnly(channelId)
	if !ok {
		return false
	}

	if entry.Channel.pwTun != nil {
		_, err := entry.Channel.pwTun.Write(data)
		if err == nil {
			tm.stats.TotalBytesRecv.Add(uint64(len(data)))
			return true
		}
	}
	return false
}

func (tm *TunnelManager) GetChannelPipes(tunnelId string, channelId int) (*io.PipeReader, *io.PipeWriter, error) {
	entry, ok := tm.GetChannel(tunnelId, channelId)
	if !ok {
		return nil, nil, ErrChannelNotFound
	}
	return entry.Channel.prSrv, entry.Channel.pwTun, nil
}

func (tm *TunnelManager) GetChannelPipesByIdOnly(channelId int) (*io.PipeReader, *io.PipeWriter, error) {
	entry, ok := tm.GetChannelByIdOnly(channelId)
	if !ok {
		return nil, nil, ErrChannelNotFound
	}
	return entry.Channel.prSrv, entry.Channel.pwTun, nil
}

func (tm *TunnelManager) GetStats() TunnelStats {
	return tm.stats
}

func (tm *TunnelManager) ListTunnels() []adaptix.TunnelData {
	var tunnels []adaptix.TunnelData
	tm.tunnels.Range(func(key, value interface{}) bool {
		tunnel := value.(*Tunnel)
		tunnels = append(tunnels, tunnel.Data)
		return true
	})
	return tunnels
}

type SafeTunnelChannel struct {
	*TunnelChannel
	mu     sync.Mutex
	closed atomic.Bool
	ctx    context.Context
	cancel context.CancelFunc
}

func NewSafeTunnelChannel(channelId int, conn net.Conn, wsconn *websocket.Conn, protocol string) *SafeTunnelChannel {
	ctx, cancel := context.WithCancel(context.Background())
	stc := &SafeTunnelChannel{
		TunnelChannel: &TunnelChannel{
			channelId: channelId,
			conn:      conn,
			wsconn:    wsconn,
			protocol:  protocol,
		},
		ctx:    ctx,
		cancel: cancel,
	}
	stc.prSrv, stc.pwSrv = io.Pipe()
	stc.prTun, stc.pwTun = io.Pipe()
	return stc
}

func (stc *SafeTunnelChannel) Close() bool {
	if stc.closed.Swap(true) {
		return false
	}

	stc.cancel()

	stc.mu.Lock()
	defer stc.mu.Unlock()

	if stc.conn != nil {
		_ = stc.conn.Close()
	}
	if stc.wsconn != nil {
		_ = stc.wsconn.Close()
	}
	if stc.pwTun != nil {
		_ = stc.pwTun.Close()
	}
	if stc.prTun != nil {
		_ = stc.prTun.Close()
	}
	if stc.pwSrv != nil {
		_ = stc.pwSrv.Close()
	}
	if stc.prSrv != nil {
		_ = stc.prSrv.Close()
	}

	return true
}

func (stc *SafeTunnelChannel) IsClosed() bool {
	return stc.closed.Load()
}

func (stc *SafeTunnelChannel) Context() context.Context {
	return stc.ctx
}

var ErrChannelNotFound = errorString("tunnel channel not found")
var ErrTunnelNotFound = errorString("tunnel not found")
var ErrAgentNotFound = errorString("agent not found")
var ErrTunnelAlreadyActive = errorString("tunnel already active")
var ErrInvalidTunnelType = errorString("invalid tunnel type")

type errorString string

func (e errorString) Error() string {
	return string(e)
}
