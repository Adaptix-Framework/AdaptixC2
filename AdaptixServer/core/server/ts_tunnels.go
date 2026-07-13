package server

import (
	"AdaptixServer/core/eventing"
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/proxy"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"math/rand/v2"
	"net"
	"strconv"
	"sync"
	"time"

	"github.com/Adaptix-Framework/axc2/v2"
)

func (ts *Teamserver) TsTunnelList() (string, error) {
	tunnels := ts.TunnelManager.ListTunnels()
	jsonTunnel, err := json.Marshal(tunnels)
	if err != nil {
		return "", err
	}
	return string(jsonTunnel), nil
}

func (ts *Teamserver) TsTunnelClientStart(AgentId int64, Listen bool, Type int, Info string, Lhost string, Lport int, Client string, Thost string, Tport int, AuthUser string, AuthPass string) (int64, error) {
	var (
		taskId   int64
		tunnelId int64
		err      error
	)

	agent, ok := ts.Agents.Get(AgentId)
	if !ok {
		return 0, fmt.Errorf("agent '%v' does not exist", AgentId)
	}
	if !agent.IsActive() {
		return 0, fmt.Errorf("agent '%v' not active", AgentId)
	}

	commandline := ""
	message := ""
	switch Type {

	case adaptix.TUNNEL_TYPE_SOCKS4:
		if Listen {
			commandline = fmt.Sprintf("[from browser] socks4 start %v:%v", Lhost, Lport)
			message = fmt.Sprintf("SOCKS4 server started on '%v:%v'", Lhost, Lport)
		} else {
			commandline = fmt.Sprintf("[from browser] socks4 (client) start %v:%v", Lhost, Lport)
			message = fmt.Sprintf("SOCKS4 server started on (client '%v') '%v:%v'", Client, Lhost, Lport)
		}

	case adaptix.TUNNEL_TYPE_SOCKS5:
		if Listen {
			commandline = fmt.Sprintf("[from browser] socks5 start %v:%v", Lhost, Lport)
			message = fmt.Sprintf("SOCKS5 server started on '%v:%v'", Lhost, Lport)
		} else {
			commandline = fmt.Sprintf("[from browser] socks5 (client) start %v:%v", Lhost, Lport)
			message = fmt.Sprintf("SOCKS5 server started on (client '%v') '%v:%v'", Client, Lhost, Lport)
		}

	case adaptix.TUNNEL_TYPE_SOCKS5_AUTH:
		if Listen {
			commandline = fmt.Sprintf("[from browser] socks5 start %v:%v -auth %v %v", Lhost, Lport, AuthUser, AuthPass)
			message = fmt.Sprintf("SOCKS5 (with Auth) server started on '%v:%v'", Lhost, Lport)
		} else {
			commandline = fmt.Sprintf("[from browser] socks5 (client) start %v:%v -auth %v %v", Lhost, Lport, AuthUser, AuthPass)
			message = fmt.Sprintf("SOCKS5 (with Auth) server started on (client '%v') '%v:%v'", Client, Lhost, Lport)
		}

	case adaptix.TUNNEL_TYPE_LOCAL_PORT:
		if Listen {
			commandline = fmt.Sprintf("[from browser] local_port_fwd start %v:%v %v:%v", Lhost, Lport, Thost, Tport)
			message = fmt.Sprintf("Started local port forwarding on %v:%v to %v:%v", Lhost, Lport, Thost, Tport)
		} else {
			commandline = fmt.Sprintf("[from browser] local_port_fwd (client) start on %v:%v %v:%v", Lhost, Lport, Thost, Tport)
			message = fmt.Sprintf("Started local port forwarding on (client '%v') %v:%v to %v:%v", Client, Lhost, Lport, Thost, Tport)
		}

	case adaptix.TUNNEL_TYPE_REVERSE:
		if Listen {
			commandline = fmt.Sprintf("[from browser] reverse_port_fwd start %v %v:%v", Lport, Thost, Tport)
			message = fmt.Sprintf("Starting reverse port forwarding %v to %v:%v", Lport, Thost, Tport)
		} else {

		}

	default:
		return 0, errors.New("unknown tunnel type")
	}

	if Listen {
		tunnelId, err = ts.TsTunnelCreate(AgentId, Type, Info, Lhost, Lport, "", Thost, Tport, AuthUser, AuthPass)
		if err != nil {
			return 0, err
		}
		taskId, err = ts.TsTunnelStart(tunnelId)
		if err != nil {
			return 0, err
		}

	} else {
		tunnelId, err = ts.TsTunnelCreate(AgentId, Type, Info, Lhost, Lport, Client, Thost, Tport, AuthUser, AuthPass)
		if err != nil {
			return 0, err
		}

		tunnel, ok := ts.TunnelManager.GetTunnel(tunnelId)
		if !ok {
			return 0, ErrTunnelNotFound
		}

		tunnel.mu.Lock()
		tunnel.Active = true
		tunnel.TaskId = ts.TsTaskGenID()
		taskId = tunnel.TaskId
		tunnelData := tunnel.Data
		tunnel.mu.Unlock()

		packet := CreateSpTunnelCreate(tunnelData, tunnel.BytesSent.Load(), tunnel.BytesRecv.Load())
		ts.TsSyncAllClientsWithCategory(packet, SyncCategoryTunnels)

		ts.TsNotifyTunnelAdd(tunnel)
	}

	taskData := adaptix.TaskData{
		TaskId:      taskId,
		Type:        adaptix.TASK_TYPE_TUNNEL,
		Sync:        true,
		Message:     message,
		MessageType: CONSOLE_OUT_SUCCESS,
		ClearText:   "",
	}
	ts.TsTaskCreate(AgentId, commandline, Client, taskData)

	return tunnelId, nil
}

type TunnelChannelData struct {
	TunnelId  int64  `json:"tunnel_id"`
	ChannelId int64  `json:"channel_id"`
	Mode      string `json:"mode,omitempty"`
	Host      string `json:"host,omitempty"`
	Port      string `json:"port,omitempty"`
}

func (ts *Teamserver) TsTunnelClientNewChannel(TunnelData string, wsconn adaptix.WebSocketConn) error {
	var td TunnelChannelData
	if err := json.Unmarshal([]byte(TunnelData), &td); err != nil {
		return errors.New("invalid tunnel data")
	}

	tunnel, ok := ts.TunnelManager.GetTunnel(td.TunnelId)
	if !ok {
		return ErrTunnelNotFound
	}

	var err error
	port := 0
	if tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS4 || tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5 || tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5_AUTH {
		port, err = strconv.Atoi(td.Port)
		if err != nil {
			return errors.New("Invalid port number")
		}
		if port < 1 || port > 65535 {
			return errors.New("Invalid port number")
		}
		if td.Host == "" {
			return errors.New("Invalid host")
		}
	}

	if tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5 || tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5_AUTH {
		if td.Mode != "tcp" && td.Mode != "udp" {
			return errors.New("invalid td.Mode")
		}
	}

	go handleTunChannelCreateClient(ts.TunnelManager, tunnel.Data.AgentId, tunnel, wsconn, td.ChannelId, td.Host, port, td.Mode)

	return nil
}

func (ts *Teamserver) TsTunnelClientSetInfo(TunnelId int64, Info string) error {
	tunnel, ok := ts.TunnelManager.GetTunnel(TunnelId)
	if !ok {
		return ErrTunnelNotFound
	}

	tunnel.mu.Lock()
	tunnel.Data.Info = Info
	tunnelData := tunnel.Data
	tunnel.mu.Unlock()

	packet := CreateSpTunnelEdit(tunnelData)
	ts.TsSyncStateWithCategory(packet, fmt.Sprintf("tunnel:%d", tunnelData.TunnelId), SyncCategoryTunnels)

	return nil
}

func (ts *Teamserver) TsTunnelClientStop(TunnelId int64, Client string) error {
	tunnel, ok := ts.TunnelManager.GetTunnel(TunnelId)
	if !ok {
		return ErrTunnelNotFound
	}

	tunnel.mu.RLock()
	tunnelClient := tunnel.Data.Client
	tunnel.mu.RUnlock()

	if tunnelClient == "" {
		_ = ts.TsTunnelStop(TunnelId)
		return nil
	}

	if tunnelClient == Client {
		tunnel, ok = ts.TunnelManager.DeleteTunnel(TunnelId)
		if !ok {
			return ErrTunnelNotFound
		}

		ts.TunnelManager.CloseAllChannels(tunnel)

		packet := CreateSpTunnelDelete(tunnel.Data)
		ts.TsSyncAllClientsWithCategory(packet, SyncCategoryTunnels)

		taskData := adaptix.TaskData{
			TaskId:     tunnel.TaskId,
			Completed:  true,
			FinishDate: time.Now().Unix(),
		}

		ts.TsTaskUpdate(tunnel.Data.AgentId, taskData)
		return nil
	}

	return errors.New("The tunnel is running on another client's side, you are not allowed to perform this operation.")
}

/// Tunnel Start

func (ts *Teamserver) TsTunnelStart(TunnelId int64) (int64, error) {
	tunnel, ok := ts.TunnelManager.GetTunnel(TunnelId)
	if !ok {
		return 0, ErrTunnelNotFound
	}

	tunnel.mu.RLock()
	tunnelData := tunnel.Data
	tunnelType := tunnel.Type
	tunnel.mu.RUnlock()

	port, err := strconv.Atoi(tunnelData.Port)
	if err != nil {
		return 0, fmt.Errorf("invalid port: %v", err)
	}

	// --- PRE HOOK ---
	preEvent := &eventing.EventDataTunnelStart{
		AgentId:    tunnelData.AgentId,
		TunnelId:   TunnelId,
		TunnelType: tunnelType,
		Port:       port,
		Info:       tunnelData.Info,
	}
	if !ts.EventManager.Emit(eventing.EventTunnelStart, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return 0, preEvent.Error
		}
		return 0, fmt.Errorf("operation cancelled by hook")
	}
	// ----------------

	if tunnelType == adaptix.TUNNEL_TYPE_REVERSE {
		if tunnel.Callbacks.Reverse == nil {
			ts.TunnelManager.DeleteTunnel(TunnelId)
			return 0, errors.New("agent does not support reverse tunnels")
		}
		taskData := tunnel.Callbacks.Reverse(TunnelId, port)
		tunnelManageTask(ts, tunnelData.AgentId, taskData)

	} else {
		address := tunnelData.Interface + ":" + tunnelData.Port
		listener, listenErr := net.Listen("tcp", address)
		if listenErr != nil {
			ts.TunnelManager.DeleteTunnel(TunnelId)
			return 0, listenErr
		}
		tunnel.mu.Lock()
		tunnel.listener = listener
		tunnel.mu.Unlock()

		go func(l net.Listener, tm *TunnelManager) {
			for {
				conn, acceptErr := l.Accept()
				if acceptErr != nil {
					return
				}
				go handleTunChannelCreate(tm, tunnelData.AgentId, tunnel, conn)
			}
		}(listener, ts.TunnelManager)
	}

	tunnel.mu.Lock()
	tunnel.Active = true
	tunnel.TaskId = ts.TsTaskGenID()
	TaskId := tunnel.TaskId
	tunnel.mu.Unlock()

	packet := CreateSpTunnelCreate(tunnelData, tunnel.BytesSent.Load(), tunnel.BytesRecv.Load())
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryTunnels)

	ts.TsNotifyTunnelAdd(tunnel)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataTunnelStart{
		AgentId:    tunnelData.AgentId,
		TunnelId:   TunnelId,
		TunnelType: tunnel.Type,
		Port:       port,
		Info:       tunnelData.Info,
	}
	ts.EventManager.EmitAsync(eventing.EventTunnelStart, postEvent)
	// -----------------

	return TaskId, nil
}

func (ts *Teamserver) TsTunnelCreate(AgentId int64, Type int, Info string, Lhost string, Lport int, Client string, Thost string, Tport int, AuthUser string, AuthPass string) (int64, error) {
	agent, ok := ts.Agents.Get(AgentId)
	if !ok {
		return 0, ErrAgentNotFound
	}

	agentData := agent.GetData()
	if Info == "" && Type == adaptix.TUNNEL_TYPE_SOCKS5_AUTH {
		Info = fmt.Sprintf("Creds %s:%s", AuthUser, AuthPass)
	}

	tunnelData := adaptix.TunnelData{
		AgentId:  agentData.Id,
		Computer: agentData.Computer,
		Username: agentData.Username,
		Process:  agentData.Process,
		Info:     Info,
		Client:   Client,
		Date:     time.Now().Unix(),
	}

	lport := strconv.Itoa(Lport)
	tport := strconv.Itoa(Tport)

	switch Type {

	case adaptix.TUNNEL_TYPE_SOCKS4:
		tunnelData.Type = "SOCKS4 proxy"
		tunnelData.TunnelId = int64(uint32(krypt.CRC32([]byte(Client + strconv.FormatInt(agentData.Id, 10) + "socks" + lport))))
		tunnelData.Interface = Lhost
		tunnelData.Port = lport

	case adaptix.TUNNEL_TYPE_SOCKS5:
		tunnelData.Type = "SOCKS5 proxy"
		tunnelData.TunnelId = int64(uint32(krypt.CRC32([]byte(Client + strconv.FormatInt(agentData.Id, 10) + "socks" + lport))))
		tunnelData.Interface = Lhost
		tunnelData.Port = lport

	case adaptix.TUNNEL_TYPE_SOCKS5_AUTH:
		tunnelData.Type = "SOCKS5 Auth proxy"
		tunnelData.TunnelId = int64(uint32(krypt.CRC32([]byte(Client + strconv.FormatInt(agentData.Id, 10) + "socks" + lport))))
		tunnelData.Interface = Lhost
		tunnelData.Port = lport
		tunnelData.AuthUser = AuthUser
		tunnelData.AuthPass = AuthPass

	case adaptix.TUNNEL_TYPE_LOCAL_PORT:
		tunnelData.Type = "Local port forward"
		tunnelData.TunnelId = int64(uint32(krypt.CRC32([]byte(Client + strconv.FormatInt(agentData.Id, 10) + "lportfwd" + lport))))
		tunnelData.Interface = Lhost
		tunnelData.Port = lport
		tunnelData.Fhost = Thost
		tunnelData.Fport = tport

	case adaptix.TUNNEL_TYPE_REVERSE:
		tunnelData.Type = "Reverse port forward"
		tunnelData.TunnelId = int64(uint32(krypt.CRC32([]byte(Client + strconv.FormatInt(agentData.Id, 10) + "rportfwd" + lport))))
		tunnelData.Port = lport
		tunnelData.Fhost = Thost
		tunnelData.Fport = tport

	default:
		return 0, errors.New("invalid tunnel type")
	}

	existingTunnel, ok := ts.TunnelManager.GetTunnel(tunnelData.TunnelId)
	if ok {
		existingTunnel.mu.RLock()
		active := existingTunnel.Active
		existingTunnel.mu.RUnlock()
		if active {
			return 0, ErrTunnelAlreadyActive
		}
		ts.TunnelManager.DeleteTunnel(tunnelData.TunnelId)
	}

	tunnel := &Tunnel{
		Data:      tunnelData,
		Type:      Type,
		Active:    false,
		Callbacks: agent.Fn.TunnelCB,
	}

	ts.TunnelManager.PutTunnel(tunnel)

	return tunnel.Data.TunnelId, nil
}

func (ts *Teamserver) TsTunnelCreateSocks4(AgentId int64, Info string, Lhost string, Lport int) (int64, error) {
	return ts.TsTunnelCreate(AgentId, adaptix.TUNNEL_TYPE_SOCKS4, Info, Lhost, Lport, "", "", 0, "", "")
}

func (ts *Teamserver) TsTunnelCreateSocks5(AgentId int64, Info string, Lhost string, Lport int, UseAuth bool, Username string, Password string) (int64, error) {
	if UseAuth {
		return ts.TsTunnelCreate(AgentId, adaptix.TUNNEL_TYPE_SOCKS5_AUTH, Info, Lhost, Lport, "", "", 0, Username, Password)
	}
	return ts.TsTunnelCreate(AgentId, adaptix.TUNNEL_TYPE_SOCKS5, Info, Lhost, Lport, "", "", 0, "", "")
}

func (ts *Teamserver) TsTunnelCreateLportfwd(AgentId int64, Info string, Lhost string, Lport int, Thost string, Tport int) (int64, error) {
	return ts.TsTunnelCreate(AgentId, adaptix.TUNNEL_TYPE_LOCAL_PORT, Info, Lhost, Lport, "", Thost, Tport, "", "")
}

func (ts *Teamserver) TsTunnelCreateRportfwd(AgentId int64, Info string, Lport int, Thost string, Tport int) (int64, error) {
	return ts.TsTunnelCreate(AgentId, adaptix.TUNNEL_TYPE_REVERSE, Info, "", Lport, "", Thost, Tport, "", "")
}

func (ts *Teamserver) TsTunnelUpdateRportfwd(tunnelId int64, result bool) (int64, string, error) {
	tunId := int64(uint32(tunnelId))

	if result {
		tunnel, ok := ts.TunnelManager.GetTunnel(tunId)
		if ok {
			tunnel.mu.RLock()
			tunnelData := tunnel.Data
			taskId := tunnel.TaskId
			tunnel.mu.RUnlock()

			packet := CreateSpTunnelCreate(tunnelData, tunnel.BytesSent.Load(), tunnel.BytesRecv.Load())
			ts.TsSyncAllClientsWithCategory(packet, SyncCategoryTunnels)

			ts.TsNotifyTunnelAdd(tunnel)

			message := fmt.Sprintf("Reverse port forward '%s' to '%s:%s'", tunnelData.Port, tunnelData.Fhost, tunnelData.Fport)

			return taskId, message, nil
		}
	} else {
		tunnel, ok := ts.TunnelManager.DeleteTunnel(tunId)
		if ok {

			taskData := adaptix.TaskData{
				TaskId:      tunnel.TaskId,
				MessageType: CONSOLE_OUT_ERROR,
				Message:     "Reverse port forward failed",
				FinishDate:  time.Now().Unix(),
				Completed:   true,
			}

			ts.TsTaskUpdate(tunnel.Data.AgentId, taskData)

			return tunnel.TaskId, "", errors.New("reverse port forward failed")
		}
	}
	return 0, "", errors.New("tunnel not found")
}

/// Tunnel Stop

func (ts *Teamserver) TsTunnelStop(TunnelId int64) error {
	tunnel, ok := ts.TunnelManager.GetTunnel(TunnelId)
	if !ok {
		return ErrTunnelNotFound
	}

	tunnel.mu.RLock()
	port, _ := strconv.Atoi(tunnel.Data.Port)
	agentId := tunnel.Data.AgentId
	tunnelType := tunnel.Type
	tunnel.mu.RUnlock()

	// --- PRE HOOK ---
	preEvent := &eventing.EventDataTunnelStop{
		AgentId:    agentId,
		TunnelId:   TunnelId,
		TunnelType: tunnelType,
		Port:       port,
	}
	if !ts.EventManager.Emit(eventing.EventTunnelStop, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	// ----------------

	tunnel, ok = ts.TunnelManager.DeleteTunnel(TunnelId)
	if !ok {
		return nil
	}

	if tunnel.listener != nil {
		_ = tunnel.listener.Close()
	}

	ts.TunnelManager.CloseAllChannels(tunnel)

	packet := CreateSpTunnelDelete(tunnel.Data)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryTunnels)

	taskData := adaptix.TaskData{
		TaskId:     tunnel.TaskId,
		Completed:  true,
		FinishDate: time.Now().Unix(),
	}

	ts.TsTaskUpdate(tunnel.Data.AgentId, taskData)

	ts.TsNotifyTunnelRemove(tunnel)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataTunnelStop{
		AgentId:    tunnel.Data.AgentId,
		TunnelId:   TunnelId,
		TunnelType: tunnel.Type,
		Port:       port,
	}
	ts.EventManager.EmitAsync(eventing.EventTunnelStop, postEvent)
	// -----------------

	return nil
}

func (ts *Teamserver) stopTunnelsByAgentPort(agentId int64, port int, types ...int) {
	portStr := strconv.Itoa(port)
	typeSet := make(map[int]struct{}, len(types))
	for _, t := range types {
		typeSet[t] = struct{}{}
	}

	var ids []int64
	ts.TunnelManager.ForEachTunnel(func(id int64, tunnel *Tunnel) bool {
		if tunnel == nil {
			return true
		}
		if tunnel.Data.AgentId != agentId || tunnel.Data.Port != portStr {
			return true
		}
		if _, ok := typeSet[tunnel.Type]; ok {
			ids = append(ids, id)
		}
		return true
	})
	for _, id := range ids {
		_ = ts.TsTunnelStop(id)
	}
}

func (ts *Teamserver) TsTunnelStopSocks(AgentId int64, Port int) {
	ts.stopTunnelsByAgentPort(AgentId, Port,
		adaptix.TUNNEL_TYPE_SOCKS4,
		adaptix.TUNNEL_TYPE_SOCKS5,
		adaptix.TUNNEL_TYPE_SOCKS5_AUTH,
	)
}

func (ts *Teamserver) TsTunnelStopLportfwd(AgentId int64, Port int) {
	ts.stopTunnelsByAgentPort(AgentId, Port, adaptix.TUNNEL_TYPE_LOCAL_PORT)
}

func (ts *Teamserver) TsTunnelStopRportfwd(AgentId int64, Port int) {
	portStr := strconv.Itoa(Port)
	var ids []int64
	ts.TunnelManager.ForEachTunnel(func(id int64, tunnel *Tunnel) bool {
		if tunnel != nil && tunnel.Data.AgentId == AgentId && tunnel.Data.Port == portStr && tunnel.Type == adaptix.TUNNEL_TYPE_REVERSE {
			ids = append(ids, id)
		}
		return true
	})
	for _, id := range ids {
		tunnel, ok := ts.TunnelManager.GetTunnel(id)
		if !ok {
			continue
		}
		if tunnel.Callbacks.Close != nil {
			rawTaskData := tunnel.Callbacks.Close(id)
			tunnelManageTask(ts, AgentId, rawTaskData)
		}
		_ = ts.TsTunnelStop(id)
	}
}

/// Connection

func (ts *Teamserver) TsTunnelChannelExists(channelId int64) bool {
	_, ok := ts.TunnelManager.GetChannel(channelId)
	return ok
}

func (ts *Teamserver) TsTunnelGetPipe(AgentId int64, channelId int64) (*io.PipeReader, *io.PipeWriter, error) {
	return ts.TunnelManager.GetChannelPipes(channelId)
}

func (ts *Teamserver) TsTunnelConnectionData(channelId int64, data []byte) {
	ts.TunnelManager.WriteToChannel(channelId, data)
}

func (ts *Teamserver) TsTunnelConnectionResume(AgentId int64, channelId int64, ioDirect bool) {
	entry, ok := ts.TunnelManager.GetChannel(channelId)
	if !ok {
		return
	}

	tunnel := entry.Tunnel
	tunChannel := entry.Channel
	if tunnel == nil || tunChannel == nil {
		return
	}

	if tunChannel.resumed.Load() {
		return
	}

	agentId := tunnel.Data.AgentId
	if agentId == 0 {
		agentId = AgentId
	}

	if tunnel.Data.Client == "" {
		if tunChannel.conn == nil {
			ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server:tunnels_manager", "[ERROR] tunChannel.conn is nil in relaySocketToTunnel")
			return
		}
		if !tunChannel.resumed.CompareAndSwap(false, true) {
			return
		}
		if tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5 || tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5_AUTH {
			proxy.ReplySocks5StatusConn(tunChannel.conn, adaptix.SOCKS5_SUCCESS)
		} else if tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS4 {
			proxy.ReplySocks4StatusConn(tunChannel.conn, true)
		}
		relaySocketToTunnel(ts.TunnelManager, agentId, tunnel, tunChannel, ioDirect)
		return
	}

	if tunChannel.wsconn == nil {
		ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server:tunnels_manager", "[ERROR] tunChannel.wsconn is nil in relayWebsocketToTunnel")
		return
	}
	if !tunChannel.resumed.CompareAndSwap(false, true) {
		return
	}
	if tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5 || tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5_AUTH {
		_ = tunChannel.writeWsBinary([]byte{0x05, adaptix.SOCKS5_SUCCESS, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
	} else if tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS4 {
		_ = tunChannel.writeWsBinary([]byte{0x00, 0x5a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
	}
	relayWebsocketToTunnel(ts.TunnelManager, agentId, tunnel, tunChannel, ioDirect)
}

func (ts *Teamserver) TsTunnelConnectionClose(channelId int64, writeOnly bool) {
	if entry, ok := ts.TunnelManager.GetChannel(channelId); ok && entry.Channel != nil {
		entry.Channel.skipAgentClose.Store(true)
	}
	ts.TunnelManager.CloseChannel(channelId, writeOnly)
}

func (ts *Teamserver) TsTunnelPause(channelId int64) {
	ts.TunnelManager.PauseChannel(channelId)
}

func (ts *Teamserver) TsTunnelResume(channelId int64) {
	ts.TunnelManager.ResumeChannel(channelId)
}

func (ts *Teamserver) TsTunnelConnectionHalt(channelId int64, errorCode byte) {
	entry, ok := ts.TunnelManager.GetChannel(channelId)
	if !ok {
		return
	}

	tunnel := entry.Tunnel
	tunChannel := entry.Channel
	if tunnel == nil || tunChannel == nil {
		return
	}

	tunChannel.skipAgentClose.Store(true)

	if tunnel.Data.Client == "" {
		if tunChannel.conn != nil {
			if tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5 || tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5_AUTH {
				if errorCode < 1 || errorCode > 8 {
					errorCode = adaptix.SOCKS5_CONNECTION_REFUSED
				}
				proxy.ReplySocks5StatusConn(tunChannel.conn, errorCode)
			} else if tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS4 {
				proxy.ReplySocks4StatusConn(tunChannel.conn, false)
			}
		}
	} else if tunChannel.wsconn != nil {
		if tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5 || tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5_AUTH {
			if errorCode < 1 || errorCode > 8 {
				errorCode = adaptix.SOCKS5_CONNECTION_REFUSED
			}
			_ = tunChannel.writeWsBinary([]byte{0x05, errorCode, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
		} else if tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS4 {
			_ = tunChannel.writeWsBinary([]byte{0x00, 0x5b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
		}
	}
	ts.TunnelManager.CloseChannel(channelId, false)
}

func (ts *Teamserver) TsTunnelConnectionAccept(tunnelId int64, channelId int64) {
	tunId := int64(uint32(tunnelId))
	tunnel, ok := ts.TunnelManager.GetTunnel(tunId)
	if !ok {
		return
	}

	if tunnel.Data.Client == "" {
		handlerReverseAccept(ts.TunnelManager, tunnel.Data.AgentId, tunnel, channelId)
	} else {
		// TODO: reverse proxy to client
	}
}

/// handlers

func handleTunChannelCreate(tm *TunnelManager, agentId int64, tunnel *Tunnel, conn net.Conn) {
	channelId := tm.nextChannelId.Add(1)

	stc := NewSafeTunnelChannel(tm, channelId, conn, nil, "TCP")
	tunChannel := stc.TunnelChannel

	var taskData adaptix.TaskData
	switch tunnel.Type {

	case adaptix.TUNNEL_TYPE_SOCKS4:
		proxySock, err := proxy.CheckSocks4(conn)
		if err != nil {
			tm.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server:tunnels_manager", "[ERROR] Socks4 proxy error: %v", err)
			tm.closeChannelInternal(tunChannel)
			return
		}
		if tunnel.Callbacks.ConnectTCP == nil {
			tm.closeChannelInternal(tunChannel)
			return
		}
		taskData = tunnel.Callbacks.ConnectTCP(tunChannel.channelId, proxySock.SocksType, proxySock.AddressType, proxySock.Address, proxySock.Port)

	case adaptix.TUNNEL_TYPE_SOCKS5:
		proxySock, err := proxy.CheckSocks5(conn, false, "", "")
		if err != nil {
			tm.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server:tunnels_manager", "[ERROR] Socks5 proxy error: %v", err)
			tm.closeChannelInternal(tunChannel)
			return
		}
		if proxySock.SocksCommand == 3 {
			if tunnel.Callbacks.ConnectUDP == nil {
				tm.closeChannelInternal(tunChannel)
				return
			}
			taskData = tunnel.Callbacks.ConnectUDP(tunChannel.channelId, proxySock.SocksType, proxySock.AddressType, proxySock.Address, proxySock.Port)
			tunChannel.protocol = "UDP"
		} else {
			if tunnel.Callbacks.ConnectTCP == nil {
				tm.closeChannelInternal(tunChannel)
				return
			}
			taskData = tunnel.Callbacks.ConnectTCP(tunChannel.channelId, proxySock.SocksType, proxySock.AddressType, proxySock.Address, proxySock.Port)
		}

	case adaptix.TUNNEL_TYPE_SOCKS5_AUTH:
		proxySock, err := proxy.CheckSocks5(conn, true, tunnel.Data.AuthUser, tunnel.Data.AuthPass)
		if err != nil {
			tm.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server:tunnels_manager", "Socks5 proxy error: %v", err)
			tm.closeChannelInternal(tunChannel)
			return
		}
		if proxySock.SocksCommand == 3 {
			if tunnel.Callbacks.ConnectUDP == nil {
				tm.closeChannelInternal(tunChannel)
				return
			}
			taskData = tunnel.Callbacks.ConnectUDP(tunChannel.channelId, proxySock.SocksType, proxySock.AddressType, proxySock.Address, proxySock.Port)
			tunChannel.protocol = "UDP"
		} else {
			if tunnel.Callbacks.ConnectTCP == nil {
				tm.closeChannelInternal(tunChannel)
				return
			}
			taskData = tunnel.Callbacks.ConnectTCP(tunChannel.channelId, proxySock.SocksType, proxySock.AddressType, proxySock.Address, proxySock.Port)
		}

	case adaptix.TUNNEL_TYPE_LOCAL_PORT:
		if tunnel.Callbacks.ConnectTCP == nil {
			tm.closeChannelInternal(tunChannel)
			return
		}
		tport, _ := strconv.Atoi(tunnel.Data.Fport)
		taskData = tunnel.Callbacks.ConnectTCP(tunChannel.channelId, adaptix.TUNNEL_TYPE_LOCAL_PORT, adaptix.ADDRESS_TYPE_IPV4, tunnel.Data.Fhost, tport)

	default:
		tm.closeChannelInternal(tunChannel)
		return
	}

	tm.RegisterChannel(tunnel, tunChannel)
	tm.ArmChannelResumeWatchdog(tunChannel.channelId, agentId, tunnel)
	tunnelManageTask(tm.ts, agentId, taskData)
}

func handleTunChannelCreateClient(tm *TunnelManager, agentId int64, tunnel *Tunnel, wsconn adaptix.WebSocketConn, channelId int64, targetAddress string, targetPort int, protocol string) {
	if channelId == 0 {
		_ = wsconn.Close()
		return
	}
	if tm.ChannelExists(channelId) {
		tm.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server:tunnels_manager", "[ERROR] tunnel channel %d already exists", channelId)
		_ = wsconn.Close()
		return
	}

	stc := NewSafeTunnelChannel(tm, channelId, nil, wsconn, "TCP")
	tunChannel := stc.TunnelChannel

	addressType := proxy.DetectAddrType(targetAddress)

	var taskData adaptix.TaskData
	switch tunnel.Type {

	case adaptix.TUNNEL_TYPE_SOCKS4:
		if tunnel.Callbacks.ConnectTCP == nil {
			tm.closeChannelInternal(tunChannel)
			return
		}
		taskData = tunnel.Callbacks.ConnectTCP(tunChannel.channelId, adaptix.TUNNEL_TYPE_SOCKS4, addressType, targetAddress, targetPort)

	case adaptix.TUNNEL_TYPE_SOCKS5:
		if protocol == "udp" {
			if tunnel.Callbacks.ConnectUDP == nil {
				tm.closeChannelInternal(tunChannel)
				return
			}
			taskData = tunnel.Callbacks.ConnectUDP(tunChannel.channelId, adaptix.TUNNEL_TYPE_SOCKS5, addressType, targetAddress, targetPort)
			tunChannel.protocol = "UDP"
		} else {
			if tunnel.Callbacks.ConnectTCP == nil {
				tm.closeChannelInternal(tunChannel)
				return
			}
			taskData = tunnel.Callbacks.ConnectTCP(tunChannel.channelId, adaptix.TUNNEL_TYPE_SOCKS5, addressType, targetAddress, targetPort)
		}

	case adaptix.TUNNEL_TYPE_SOCKS5_AUTH:
		if protocol == "udp" {
			if tunnel.Callbacks.ConnectUDP == nil {
				tm.closeChannelInternal(tunChannel)
				return
			}
			taskData = tunnel.Callbacks.ConnectUDP(tunChannel.channelId, adaptix.TUNNEL_TYPE_SOCKS5, addressType, targetAddress, targetPort)
			tunChannel.protocol = "UDP"
		} else {
			if tunnel.Callbacks.ConnectTCP == nil {
				tm.closeChannelInternal(tunChannel)
				return
			}
			taskData = tunnel.Callbacks.ConnectTCP(tunChannel.channelId, adaptix.TUNNEL_TYPE_SOCKS5, addressType, targetAddress, targetPort)
		}

	case adaptix.TUNNEL_TYPE_LOCAL_PORT:
		if tunnel.Callbacks.ConnectTCP == nil {
			tm.closeChannelInternal(tunChannel)
			return
		}
		tport, _ := strconv.Atoi(tunnel.Data.Fport)
		taskData = tunnel.Callbacks.ConnectTCP(tunChannel.channelId, adaptix.TUNNEL_TYPE_LOCAL_PORT, addressType, tunnel.Data.Fhost, tport)

	default:
		tm.closeChannelInternal(tunChannel)
		return
	}

	tm.RegisterChannel(tunnel, tunChannel)
	tm.ArmChannelResumeWatchdog(tunChannel.channelId, agentId, tunnel)
	tunnelManageTask(tm.ts, agentId, taskData)
}

func handlerReverseAccept(tm *TunnelManager, agentId int64, tunnel *Tunnel, channelId int64) {
	target := tunnel.Data.Fhost + ":" + tunnel.Data.Fport
	fwdConn, err := net.Dial("tcp", target)
	if err != nil {
		if tunnel.Callbacks.Close != nil {
			rawTaskData := tunnel.Callbacks.Close(channelId)
			tunnelManageTask(tm.ts, agentId, rawTaskData)
		}
		return
	}

	if tm.ChannelExists(channelId) {
		_ = fwdConn.Close()
		return
	}

	stc := NewSafeTunnelChannel(tm, channelId, fwdConn, nil, "TCP")
	tunChannel := stc.TunnelChannel
	tunChannel.resumed.Store(true)

	tm.RegisterChannel(tunnel, tunChannel)
	relaySocketToTunnel(tm, agentId, tunnel, tunChannel, false)
}

/// process socket

func tunnelManageTask(ts *Teamserver, agentId int64, taskData adaptix.TaskData) {
	agent, _ := ts.Agents.Get(agentId)
	taskData.AgentId = agentId
	if taskData.TaskId == 0 {
		if taskData.Sync {
			taskData.TaskId = ts.TsTaskGenID()
		} else {
			taskData.TaskId = int64(rand.Uint32())
		}
	}

	if agent != nil {
		agent.HostedQueue.Push(taskData.Priority, taskData)
	}
}

func relayPipeToTaskData(ts *Teamserver, agentId int64, channelId int64, taskData adaptix.TaskData) {
	if taskData.TaskId == 0 {
		if taskData.Sync {
			taskData.TaskId = ts.TsTaskGenID()
		} else {
			taskData.TaskId = int64(rand.Uint32())
		}
	}
	taskData.AgentId = agentId

	agent, _ := ts.Agents.Get(agentId) // agent may be nil if disconnected
	if agent != nil {
		agent.HostedQueue.Push(taskData.Priority, taskData)
	}
}

func relaySocketToTunnel(tm *TunnelManager, agentId int64, tunnel *Tunnel, tunChannel *TunnelChannel, direct bool) {
	agent, _ := tm.ts.Agents.Get(agentId)
	ctx, cancel := context.WithCancel(context.Background())
	var once sync.Once
	finish := func() {
		once.Do(func() {
			cancel()
			tm.CloseChannel(tunChannel.channelId, false)
			if tunChannel.skipAgentClose.Load() {
				return
			}
			if tunnel.Callbacks.Close == nil {
				return
			}
			taskData := tunnel.Callbacks.Close(tunChannel.channelId)
			tunnelManageTask(tm.ts, agentId, taskData)
		})
	}

	go func() {
		if direct {
			defer finish()
		}
		if tunChannel.pwSrv == nil || tunChannel.conn == nil {
			tm.ts.TsLogAdd(adaptix.LogStatusDebug, 0, "server:tunnels_manager", "[ERROR relaySocketToTunnel] pwSrv or conn == nil — copy (pwSrv <- conn)")
			return
		}
		buf := tm.GetBuffer()
		defer tm.PutBuffer(buf)
		for {
			n, err := tunChannel.conn.Read(buf)
			if n > 0 {
				tunnel.BytesSent.Add(int64(n))
				_, _ = tunChannel.pwSrv.Write(buf[:n])
			}
			if err != nil {
				break
			}
		}
		_ = tunChannel.pwSrv.Close()
	}()

	if !direct {
		go func() {
			defer finish()
			buf := tm.GetBuffer()
			defer tm.PutBuffer(buf)

			backoff := time.Duration(1) * time.Millisecond
			const maxBackoff = 50 * time.Millisecond
			const minBackoff = 1 * time.Millisecond
			for {
				select {
				case <-ctx.Done():
					return
				default:
					if tunChannel.paused.Load() {
						time.Sleep(backoff)
						if backoff < maxBackoff {
							backoff *= 2
						}
						continue
					}
					if agent != nil && agent.HostedQueue != nil && agent.HostedQueue.Len() > 128 {
						time.Sleep(backoff)
						if backoff < maxBackoff {
							backoff *= 2
						}
						continue
					}

					n, err := tunChannel.prSrv.Read(buf)
					if n > 0 {
						backoff = minBackoff
						payload := make([]byte, n)
						copy(payload, buf[:n])
						var td adaptix.TaskData
						if tunChannel.protocol == "UDP" {
							if tunnel.Callbacks.WriteUDP == nil {
								return
							}
							td = tunnel.Callbacks.WriteUDP(tunChannel.channelId, payload)
						} else {
							if tunnel.Callbacks.WriteTCP == nil {
								return
							}
							td = tunnel.Callbacks.WriteTCP(tunChannel.channelId, payload)
						}
						relayPipeToTaskData(tm.ts, agentId, tunChannel.channelId, td)
					}
					if err != nil {
						return
					}
				}
			}
		}()
	}
}

func relayWebsocketToTunnel(tm *TunnelManager, agentId int64, tunnel *Tunnel, tunChannel *TunnelChannel, direct bool) {
	agent, _ := tm.ts.Agents.Get(agentId)
	ctx, cancel := context.WithCancel(context.Background())
	var once sync.Once
	finish := func() {
		once.Do(func() {
			cancel()
			tm.CloseChannel(tunChannel.channelId, false)
			if tunChannel.skipAgentClose.Load() {
				return
			}
			if tunnel.Callbacks.Close == nil {
				return
			}
			taskData := tunnel.Callbacks.Close(tunChannel.channelId)
			tunnelManageTask(tm.ts, agentId, taskData)
		})
	}

	go func() {
		if direct {
			defer finish()
		}
		if tunChannel.wsconn == nil || tunChannel.pwSrv == nil {
			return
		}
		for {
			_, msg, err := tunChannel.wsconn.ReadMessage()
			if err != nil {
				break
			}
			tunnel.BytesSent.Add(int64(len(msg)))
			if _, err := tunChannel.pwSrv.Write(msg); err != nil {
				break
			}
		}
		_ = tunChannel.pwSrv.Close()
	}()

	if !direct {
		go func() {
			defer finish()
			buf := tm.GetBuffer()
			defer tm.PutBuffer(buf)

			backoff := time.Duration(1) * time.Millisecond
			const maxBackoff = 50 * time.Millisecond
			const minBackoff = 1 * time.Millisecond
			for {
				select {
				case <-ctx.Done():
					return
				default:
					if tunChannel.paused.Load() {
						time.Sleep(backoff)
						if backoff < maxBackoff {
							backoff *= 2
						}
						continue
					}
					if agent != nil && agent.HostedQueue != nil && agent.HostedQueue.Len() > 128 {
						time.Sleep(backoff)
						if backoff < maxBackoff {
							backoff *= 2
						}
						continue
					}

					n, err := tunChannel.prSrv.Read(buf)
					if n > 0 {
						backoff = minBackoff
						payload := make([]byte, n)
						copy(payload, buf[:n])
						var td adaptix.TaskData
						if tunChannel.protocol == "UDP" {
							if tunnel.Callbacks.WriteUDP == nil {
								return
							}
							td = tunnel.Callbacks.WriteUDP(tunChannel.channelId, payload)
						} else {
							if tunnel.Callbacks.WriteTCP == nil {
								return
							}
							td = tunnel.Callbacks.WriteTCP(tunChannel.channelId, payload)
						}
						relayPipeToTaskData(tm.ts, agentId, tunChannel.channelId, td)
					}
					if err != nil {
						return
					}
				}
			}
		}()
	}
}
