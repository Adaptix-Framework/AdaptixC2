package server

import (
	"context"
	"io"
	"net"
	"sync"
	"sync/atomic"
	"time"

	"github.com/Adaptix-Framework/axsafe"

	"github.com/Adaptix-Framework/axc2/v2"
	"github.com/gorilla/websocket"
)

const (
	TunnelBufferSize    = 0x8000
	TunnelBufferPoolCap = 256

	tunnelIngressQueueDepth = 1024
	tunnelIngressHiWM       = 80
	tunnelIngressLoWM       = 20

	tunnelIngressBlockTimeout = 2 * time.Second

	tunnelChannelResumeTimeout = 60 * time.Second
)

type TunnelManager struct {
	ts *Teamserver

	tunnels       axsafe.Map[int64, *Tunnel]       // tunnelId
	channelIndex  axsafe.Map[int64, *ChannelEntry] // channelId
	nextChannelId atomic.Int64

	bufferPool sync.Pool

	stats TunnelStats
}

func (tm *TunnelManager) SendTunnelFlowControl(channelId int64, pause bool) {
	entry, ok := tm.GetChannel(channelId)
	if !ok || entry.Tunnel == nil {
		return
	}
	if tm.ts == nil {
		return
	}

	var task adaptix.TaskData
	if pause {
		if entry.Tunnel.Callbacks.Pause == nil {
			return
		}
		task = entry.Tunnel.Callbacks.Pause(channelId)
	} else {
		if entry.Tunnel.Callbacks.Resume == nil {
			return
		}
		task = entry.Tunnel.Callbacks.Resume(channelId)
	}

	if task.Type == 0 && len(task.Data) == 0 {
		return
	}
	tunnelManageTask(tm.ts, entry.Tunnel.Data.AgentId, task)
}

func (tc *TunnelChannel) writeWsBinary(data []byte) error {
	if tc == nil || tc.wsconn == nil {
		return io.ErrClosedPipe
	}
	tc.wsWriteMu.Lock()
	defer tc.wsWriteMu.Unlock()
	return tc.wsconn.WriteMessage(websocket.BinaryMessage, data)
}

func (tc *TunnelChannel) closeWs() {
	if tc == nil || tc.wsconn == nil {
		return
	}
	tc.wsWriteMu.Lock()
	_ = tc.wsconn.Close()
	tc.wsWriteMu.Unlock()
}

type ChannelEntry struct {
	TunnelId int64
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
		ts:           ts,
		tunnels:      axsafe.NewMap[int64, *Tunnel](),
		channelIndex: axsafe.NewMap[int64, *ChannelEntry](),
		bufferPool: sync.Pool{
			New: func() interface{} {
				buf := make([]byte, TunnelBufferSize)
				return buf
			},
		},
	}
	return tm
}

func (tm *TunnelManager) Start(ctx context.Context) {
	go tm.statsLoop(ctx)
}

func (tm *TunnelManager) statsLoop(ctx context.Context) {
	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()
	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			tm.ForEachTunnel(func(key int64, tunnel *Tunnel) bool {
				packet := CreateSpTunnelCreate(tunnel.Data, tunnel.BytesSent.Load(), tunnel.BytesRecv.Load())
				tm.ts.TsSyncAllClientsWithCategory(packet, SyncCategoryTunnels)
				return true
			})
		}
	}
}

func (tm *TunnelManager) GetBuffer() []byte {
	buf, ok := tm.bufferPool.Get().([]byte)
	if !ok || cap(buf) < TunnelBufferSize {
		return make([]byte, TunnelBufferSize)
	}
	return buf[:TunnelBufferSize]
}

func (tm *TunnelManager) PutBuffer(buf []byte) {
	if cap(buf) >= TunnelBufferSize {
		tm.bufferPool.Put(buf[:TunnelBufferSize])
	}
}

func (tm *TunnelManager) GetTunnel(tunnelId int64) (*Tunnel, bool) {
	return tm.tunnels.Get(tunnelId)
}

func (tm *TunnelManager) PutTunnel(tunnel *Tunnel) {
	tm.tunnels.Put(tunnel.Data.TunnelId, tunnel)
	tm.stats.ActiveTunnels.Add(1)
}

func (tm *TunnelManager) DeleteTunnel(tunnelId int64) (*Tunnel, bool) {
	tunnel, ok := tm.tunnels.GetDelete(tunnelId)
	if !ok {
		return nil, false
	}
	tm.stats.ActiveTunnels.Add(-1)
	return tunnel, true
}

func (tm *TunnelManager) TunnelExists(tunnelId int64) bool {
	return tm.tunnels.Contains(tunnelId)
}

func (tm *TunnelManager) ForEachTunnel(fn func(tunnelId int64, tunnel *Tunnel) bool) {
	tm.tunnels.ForEachFast(fn)
}

func (tm *TunnelManager) RegisterChannel(tunnel *Tunnel, channel *TunnelChannel) {
	entry := &ChannelEntry{
		TunnelId: tunnel.Data.TunnelId,
		Tunnel:   tunnel,
		Channel:  channel,
	}
	tm.channelIndex.Put(channel.channelId, entry)
	tm.stats.ActiveChannels.Add(1)
}

func (tm *TunnelManager) UnregisterChannel(channelId int64) {
	if _, ok := tm.channelIndex.GetDelete(channelId); ok {
		tm.stats.ActiveChannels.Add(-1)
	}
}

func (tm *TunnelManager) GetChannel(channelId int64) (*ChannelEntry, bool) {
	return tm.channelIndex.Get(channelId)
}

func (tm *TunnelManager) ChannelExists(channelId int64) bool {
	return tm.channelIndex.Contains(channelId)
}

func (tm *TunnelManager) CloseChannel(channelId int64, writeOnly bool) {
	entry, ok := tm.GetChannel(channelId)
	if !ok {
		return
	}
	if writeOnly {
		if entry.Channel != nil && entry.Channel.pwTun != nil {
			_ = entry.Channel.pwTun.Close()
		}
	} else {
		tm.closeChannelInternal(entry.Channel)
		if _, deleted := tm.channelIndex.GetDelete(channelId); deleted {
			tm.stats.ActiveChannels.Add(-1)
		}
	}
}

func (tm *TunnelManager) closeChannelInternal(channel *TunnelChannel) {
	if channel == nil {
		return
	}

	channel.CloseIngress()

	if channel.conn != nil {
		_ = channel.conn.Close()
	}
	channel.closeWs()

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
}

func (tm *TunnelManager) ArmChannelResumeWatchdog(channelId int64, agentId int64, tunnel *Tunnel) {
	if tm == nil || tunnel == nil {
		return
	}
	go func() {
		timer := time.NewTimer(tunnelChannelResumeTimeout)
		defer timer.Stop()
		<-timer.C

		entry, ok := tm.GetChannel(channelId)
		if !ok || entry.Channel == nil {
			return
		}
		ch := entry.Channel
		if ch.resumed.Load() {
			return
		}
		if !ch.resumed.CompareAndSwap(false, true) {
			return
		}

		if tunnel.Data.Client == "" {
			if ch.conn != nil {
				if tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5 || tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5_AUTH {
					_, _ = ch.conn.Write([]byte{0x05, adaptix.SOCKS5_CONNECTION_REFUSED, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
				} else if tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS4 {
					_, _ = ch.conn.Write([]byte{0x00, 0x5b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
				}
			}
		} else if ch.wsconn != nil {
			if tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5 || tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5_AUTH {
				_ = ch.writeWsBinary([]byte{0x05, adaptix.SOCKS5_CONNECTION_REFUSED, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
			} else if tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS4 {
				_ = ch.writeWsBinary([]byte{0x00, 0x5b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
			}
		}

		if tunnel.Callbacks.Close != nil {
			tunnelManageTask(tm.ts, agentId, tunnel.Callbacks.Close(channelId))
		}
		ch.skipAgentClose.Store(true)
		tm.CloseChannel(channelId, false)
	}()
}

func (tm *TunnelManager) CloseAllChannels(tunnel *Tunnel) {
	var channelIds []int64
	tm.channelIndex.ForEachFast(func(channelId int64, entry *ChannelEntry) bool {
		if entry.Tunnel == tunnel {
			channelIds = append(channelIds, channelId)
		}
		return true
	})
	for _, id := range channelIds {
		if entry, ok := tm.channelIndex.GetDelete(id); ok {
			tm.closeChannelInternal(entry.Channel)
			tm.stats.ActiveChannels.Add(-1)
		}
	}
}

func (tm *TunnelManager) WriteToChannel(channelId int64, data []byte) bool {
	if len(data) == 0 {
		return true
	}

	entry, ok := tm.GetChannel(channelId)
	if !ok || entry.Channel == nil {
		return false
	}
	ch := entry.Channel

	if ch.ingressChan != nil {
		if ch.ingressClosed.Load() {
			return false
		}

		curLen := len(ch.ingressChan)
		capLen := cap(ch.ingressChan)
		if capLen > 0 && curLen > (capLen*tunnelIngressHiWM)/100 {
			if ch.flowPaused.CompareAndSwap(false, true) {
				tm.SendTunnelFlowControl(channelId, true)
			}
		}

		payload := make([]byte, len(data))
		copy(payload, data)

		record := func() {
			tm.stats.TotalBytesRecv.Add(uint64(len(payload)))
			if entry.Tunnel != nil {
				entry.Tunnel.BytesRecv.Add(int64(len(payload)))
			}
		}

		sendIngress := func(block <-chan time.Time) (ok bool) {
			defer func() {
				if recover() != nil {
					ok = false
				}
			}()
			if ch.ingressClosed.Load() {
				return false
			}
			if block == nil {
				select {
				case ch.ingressChan <- payload:
					return true
				default:
					return false
				}
			}
			select {
			case ch.ingressChan <- payload:
				return true
			case <-block:
				return false
			}
		}

		if sendIngress(nil) {
			record()
			return true
		}

		if ch.flowPaused.CompareAndSwap(false, true) {
			tm.SendTunnelFlowControl(channelId, true)
		}

		timer := time.NewTimer(tunnelIngressBlockTimeout)
		ok := sendIngress(timer.C)
		if !ok {
			if !timer.Stop() {
				select {
				case <-timer.C:
				default:
				}
			}
			return false
		}
		if !timer.Stop() {
			select {
			case <-timer.C:
			default:
			}
		}
		record()
		return true
	}

	if ch.pwTun != nil {
		_, err := ch.pwTun.Write(data)
		if err == nil {
			tm.stats.TotalBytesRecv.Add(uint64(len(data)))
			if entry.Tunnel != nil {
				entry.Tunnel.BytesRecv.Add(int64(len(data)))
			}
			return true
		}
	}
	return false
}

func (tm *TunnelManager) GetChannelPipes(channelId int64) (*io.PipeReader, *io.PipeWriter, error) {
	entry, ok := tm.GetChannel(channelId)
	if !ok || entry.Channel == nil {
		return nil, nil, ErrChannelNotFound
	}
	return entry.Channel.prSrv, entry.Channel.pwTun, nil
}

func (tm *TunnelManager) GetStats() *TunnelStats {
	return &tm.stats
}

func (tm *TunnelManager) PauseChannel(channelId int64) {
	entry, ok := tm.GetChannel(channelId)
	if ok && entry.Channel != nil {
		entry.Channel.paused.Store(true)
	}
}

func (tm *TunnelManager) ResumeChannel(channelId int64) {
	entry, ok := tm.GetChannel(channelId)
	if ok && entry.Channel != nil {
		entry.Channel.paused.Store(false)
	}
}

func (tm *TunnelManager) ListTunnels() []adaptix.TunnelData {
	var tunnels []adaptix.TunnelData
	tm.tunnels.ForEachFast(func(key int64, tunnel *Tunnel) bool {
		tunnel.mu.RLock()
		tunnels = append(tunnels, tunnel.Data)
		tunnel.mu.RUnlock()
		return true
	})
	return tunnels
}

/// SAFE TUNNEL CHANNEL

type SafeTunnelChannel struct {
	*TunnelChannel
	tm      *TunnelManager
	mu      sync.Mutex
	closed  atomic.Bool
	closing atomic.Bool
	ctx     context.Context
	cancel  context.CancelFunc
}

func NewSafeTunnelChannel(tm *TunnelManager, channelId int64, conn net.Conn, wsconn adaptix.WebSocketConn, protocol string) *SafeTunnelChannel {
	ctx, cancel := context.WithCancel(context.Background())
	stc := &SafeTunnelChannel{
		TunnelChannel: &TunnelChannel{
			channelId: channelId,
			conn:      conn,
			wsconn:    wsconn,
			protocol:  protocol,
		},
		tm:     tm,
		ctx:    ctx,
		cancel: cancel,
	}
	stc.prSrv, stc.pwSrv = io.Pipe()
	stc.prTun, stc.pwTun = io.Pipe()
	stc.ingressChan = make(chan []byte, tunnelIngressQueueDepth)
	go stc.ingressPump()
	return stc
}

func (stc *SafeTunnelChannel) ingressPump() {
	defer func() {
		if stc.pwTun != nil {
			_ = stc.pwTun.Close()
		}
	}()

	resumeTicker := time.NewTicker(500 * time.Millisecond)
	defer resumeTicker.Stop()

	for {
		select {
		case data, ok := <-stc.ingressChan:
			if !ok {
				return
			}
			if stc.flowPaused.Load() {
				capLen := cap(stc.ingressChan)
				if capLen > 0 && len(stc.ingressChan) < (capLen*tunnelIngressLoWM)/100 {
					if stc.flowPaused.CompareAndSwap(true, false) {
						if stc.tm != nil {
							stc.tm.SendTunnelFlowControl(stc.channelId, false)
						}
					}
				}
			}

			if stc.conn != nil {
				if _, err := stc.conn.Write(data); err != nil {
					return
				}
			} else if stc.wsconn != nil {
				if err := stc.writeWsBinary(data); err != nil {
					return
				}
			} else if stc.pwTun != nil {
				if _, err := stc.pwTun.Write(data); err != nil {
					return
				}
			}

		case <-resumeTicker.C:
			if stc.flowPaused.Load() {
				capLen := cap(stc.ingressChan)
				if capLen > 0 && len(stc.ingressChan) < (capLen*tunnelIngressLoWM)/100 {
					if stc.flowPaused.CompareAndSwap(true, false) {
						if stc.tm != nil {
							stc.tm.SendTunnelFlowControl(stc.channelId, false)
						}
					}
				}
			}
		}
	}
}

func (stc *SafeTunnelChannel) Close() bool {
	if stc.closed.Swap(true) {
		return false
	}

	stc.closing.Store(true)
	stc.CloseIngress()

	stc.cancel()

	stc.mu.Lock()
	defer stc.mu.Unlock()

	if stc.conn != nil {
		_ = stc.conn.Close()
	}
	stc.closeWs()
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

/// UTILS

var ErrChannelNotFound = errorString("tunnel channel not found")
var ErrTunnelNotFound = errorString("tunnel not found")
var ErrAgentNotFound = errorString("agent not found")
var ErrTunnelAlreadyActive = errorString("tunnel already active")

type errorString string

func (e errorString) Error() string {
	return string(e)
}
