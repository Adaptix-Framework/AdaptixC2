package server

import (
	"AdaptixServer/core/eventing"
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/proxy"
	"AdaptixServer/core/utils/safe"
	"context"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"math/rand/v2"
	"net"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/Adaptix-Framework/axc2"
	"github.com/gorilla/websocket"
)

func (ts *Teamserver) TsTunnelList() (string, error) {
	tunnels := ts.TunnelManager.ListTunnels()
	jsonTunnel, err := json.Marshal(tunnels)
	if err != nil {
		return "", err
	}
	return string(jsonTunnel), nil
}

func (ts *Teamserver) TsTunnelClientStart(AgentId string, Listen bool, Type int, Info string, Lhost string, Lport int, Client string, Thost string, Tport int, AuthUser string, AuthPass string) (string, error) {
	var (
		taskId   string
		tunnelId string
		err      error
	)

	value, ok := ts.Agents.Get(AgentId)
	if !ok {
		return "", fmt.Errorf("agent '%v' does not exist", AgentId)
	}
	agent, ok := value.(*Agent)
	if !ok {
		return "", fmt.Errorf("invalid agent type for '%v'", AgentId)
	}
	if agent.Active == false {
		return "", fmt.Errorf("agent '%v' not active", AgentId)
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
		return "", errors.New("unknown tunnel type")
	}

	if Listen {
		tunnelId, err = ts.TsTunnelCreate(AgentId, Type, Info, Lhost, Lport, "", Thost, Tport, AuthUser, AuthPass)
		if err != nil {
			return "", err
		}
		taskId, err = ts.TsTunnelStart(tunnelId)
		if err != nil {
			return "", err
		}

	} else {
		tunnelId, err = ts.TsTunnelCreate(AgentId, Type, Info, Lhost, Lport, Client, Thost, Tport, AuthUser, AuthPass)
		if err != nil {
			return "", err
		}

		tunnel, ok := ts.TunnelManager.GetTunnel(tunnelId)
		if !ok {
			return "", ErrTunnelNotFound
		}
		tunnel.Active = true
		tunnel.TaskId, _ = krypt.GenerateUID(8)
		taskId = tunnel.TaskId

		packet := CreateSpTunnelCreate(tunnel.Data)
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

func (ts *Teamserver) TsTunnelClientNewChannel(TunnelData string, wsconn *websocket.Conn) error {
	data, err := base64.StdEncoding.DecodeString(TunnelData)
	if err != nil {
		return errors.New("invalid tunnel data")
	}

	d := strings.Split(string(data), "|")
	if len(d) != 5 {
		return errors.New("invalid tunnel data")
	}

	tunnelId := d[0]
	channelId := d[1]
	mode := d[2]
	host := d[3]
	tPort := d[4]

	tunnel, ok := ts.TunnelManager.GetTunnel(tunnelId)
	if !ok {
		return ErrTunnelNotFound
	}

	agent, err := ts.getAgent(tunnel.Data.AgentId)
	if err != nil {
		return ErrAgentNotFound
	}

	cid, err := strconv.ParseInt(channelId, 16, 64)
	if err != nil {
		return errors.New("channelId not supported")
	}

	port := 0
	if tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS4 || tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5 || tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5_AUTH {
		port, err = strconv.Atoi(tPort)
		if err != nil {
			return errors.New("Invalid port number")
		}
		if port < 1 || port > 65535 {
			return errors.New("Invalid port number")
		}
		if host == "" {
			return errors.New("Invalid host")
		}
	}

	if tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5 || tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5_AUTH {
		if mode != "tcp" && mode != "udp" {
			return errors.New("invalid mode")
		}
	}

	go handleTunChannelCreateClient(ts.TunnelManager, agent, tunnel, wsconn, int(cid), host, port, mode)

	return nil
}

func (ts *Teamserver) TsTunnelClientSetInfo(TunnelId string, Info string) error {
	tunnel, ok := ts.TunnelManager.GetTunnel(TunnelId)
	if !ok {
		return ErrTunnelNotFound
	}

	tunnel.Data.Info = Info

	packet := CreateSpTunnelEdit(tunnel.Data)
	ts.TsSyncStateWithCategory(packet, "tunnel:"+tunnel.Data.TunnelId, SyncCategoryTunnels)

	return nil
}

func (ts *Teamserver) TsTunnelClientStop(TunnelId string, Client string) error {
	tunnel, ok := ts.TunnelManager.GetTunnel(TunnelId)
	if !ok {
		return ErrTunnelNotFound
	}

	if tunnel.Data.Client == "" {
		_ = ts.TsTunnelStop(TunnelId)
		return nil
	}

	if tunnel.Data.Client == Client {
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

func (ts *Teamserver) TsTunnelStart(TunnelId string) (string, error) {
	tunnel, ok := ts.TunnelManager.GetTunnel(TunnelId)
	if !ok {
		return "", ErrTunnelNotFound
	}

	agent, err := ts.getAgent(tunnel.Data.AgentId)
	if err != nil {
		return "", ErrAgentNotFound
	}

	port, _ := strconv.Atoi(tunnel.Data.Port)
	// --- PRE HOOK ---
	preEvent := &eventing.EventDataTunnelStart{
		AgentId:    tunnel.Data.AgentId,
		TunnelId:   TunnelId,
		TunnelType: tunnel.Type,
		Port:       port,
		Info:       tunnel.Data.Info,
	}
	if !ts.EventManager.Emit(eventing.EventTunnelStart, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return "", preEvent.Error
		}
		return "", fmt.Errorf("operation cancelled by hook")
	}
	// ----------------

	if tunnel.Type == adaptix.TUNNEL_TYPE_REVERSE {
		id, _ := strconv.ParseInt(TunnelId, 16, 64)
		port, err := strconv.Atoi(tunnel.Data.Port)
		if err != nil {
			return "", fmt.Errorf("invalid port: %v", err)
		}
		taskData := tunnel.Callbacks.Reverse(int(id), port)
		tunnelManageTask(agent, taskData)

	} else {
		address := tunnel.Data.Interface + ":" + tunnel.Data.Port
		listener, listenErr := net.Listen("tcp", address)
		if listenErr != nil {
			ts.TunnelManager.DeleteTunnel(TunnelId)
			return "", listenErr
		}
		tunnel.listener = listener

		go func(l net.Listener, tm *TunnelManager) {
			for {
				conn, acceptErr := l.Accept()
				if acceptErr != nil {
					return
				}
				go handleTunChannelCreate(tm, agent, tunnel, conn)
			}
		}(tunnel.listener, ts.TunnelManager)
	}

	tunnel.Active = true
	tunnel.TaskId, _ = krypt.GenerateUID(8)

	packet := CreateSpTunnelCreate(tunnel.Data)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryTunnels)

	ts.TsNotifyTunnelAdd(tunnel)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataTunnelStart{
		AgentId:    tunnel.Data.AgentId,
		TunnelId:   TunnelId,
		TunnelType: tunnel.Type,
		Port:       port,
		Info:       tunnel.Data.Info,
	}
	ts.EventManager.EmitAsync(eventing.EventTunnelStart, postEvent)
	// -----------------

	return tunnel.TaskId, nil
}

func (ts *Teamserver) TsTunnelCreate(AgentId string, Type int, Info string, Lhost string, Lport int, Client string, Thost string, Tport int, AuthUser string, AuthPass string) (string, error) {
	agent, err := ts.getAgent(AgentId)
	if err != nil {
		return "", ErrAgentNotFound
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
	}

	lport := strconv.Itoa(Lport)
	tport := strconv.Itoa(Tport)

	switch Type {

	case adaptix.TUNNEL_TYPE_SOCKS4:
		tunnelData.Type = "SOCKS4 proxy"
		tunnelData.TunnelId = fmt.Sprintf("%08x", krypt.CRC32([]byte(Client+agentData.Id+"socks"+lport)))
		tunnelData.Interface = Lhost
		tunnelData.Port = lport

	case adaptix.TUNNEL_TYPE_SOCKS5:
		tunnelData.Type = "SOCKS5 proxy"
		tunnelData.TunnelId = fmt.Sprintf("%08x", krypt.CRC32([]byte(Client+agentData.Id+"socks"+lport)))
		tunnelData.Interface = Lhost
		tunnelData.Port = lport

	case adaptix.TUNNEL_TYPE_SOCKS5_AUTH:
		tunnelData.Type = "SOCKS5 Auth proxy"
		tunnelData.TunnelId = fmt.Sprintf("%08x", krypt.CRC32([]byte(Client+agentData.Id+"socks"+lport)))
		tunnelData.Interface = Lhost
		tunnelData.Port = lport
		tunnelData.AuthUser = AuthUser
		tunnelData.AuthPass = AuthPass

	case adaptix.TUNNEL_TYPE_LOCAL_PORT:
		tunnelData.Type = "Local port forward"
		tunnelData.TunnelId = fmt.Sprintf("%08x", krypt.CRC32([]byte(Client+agentData.Id+"lportfwd"+lport)))
		tunnelData.Interface = Lhost
		tunnelData.Port = lport
		tunnelData.Fhost = Thost
		tunnelData.Fport = tport

	case adaptix.TUNNEL_TYPE_REVERSE:
		tunnelData.Type = "Reverse port forward"
		tunnelData.TunnelId = fmt.Sprintf("%08x", krypt.CRC32([]byte(Client+agentData.Id+"rportfwd"+lport)))
		tunnelData.Port = lport
		tunnelData.Fhost = Thost
		tunnelData.Fport = tport

	default:
		return "", errors.New("invalid tunnel type")
	}

	if existingTunnel, ok := ts.TunnelManager.GetTunnel(tunnelData.TunnelId); ok {
		if existingTunnel.Active {
			return "", ErrTunnelAlreadyActive
		} else {
			ts.TunnelManager.DeleteTunnel(tunnelData.TunnelId)
		}
	}

	tunnel := &Tunnel{
		connections: safe.NewMap(),
		Data:        tunnelData,
		Type:        Type,
		Active:      false,
		Callbacks:   agent.TunnelCallbacks(),
	}

	ts.TunnelManager.PutTunnel(tunnel)

	return tunnel.Data.TunnelId, nil
}

func (ts *Teamserver) TsTunnelCreateSocks4(AgentId string, Info string, Lhost string, Lport int) (string, error) {
	return ts.TsTunnelCreate(AgentId, adaptix.TUNNEL_TYPE_SOCKS4, Info, Lhost, Lport, "", "", 0, "", "")
}

func (ts *Teamserver) TsTunnelCreateSocks5(AgentId string, Info string, Lhost string, Lport int, UseAuth bool, Username string, Password string) (string, error) {
	if UseAuth {
		return ts.TsTunnelCreate(AgentId, adaptix.TUNNEL_TYPE_SOCKS5_AUTH, Info, Lhost, Lport, "", "", 0, Username, Password)
	} else {
		return ts.TsTunnelCreate(AgentId, adaptix.TUNNEL_TYPE_SOCKS5, Info, Lhost, Lport, "", "", 0, "", "")
	}
}

func (ts *Teamserver) TsTunnelCreateLportfwd(AgentId string, Info string, Lhost string, Lport int, Thost string, Tport int) (string, error) {
	return ts.TsTunnelCreate(AgentId, adaptix.TUNNEL_TYPE_LOCAL_PORT, Info, Lhost, Lport, "", Thost, Tport, "", "")
}

func (ts *Teamserver) TsTunnelCreateRportfwd(AgentId string, Info string, Lport int, Thost string, Tport int) (string, error) {
	return ts.TsTunnelCreate(AgentId, adaptix.TUNNEL_TYPE_REVERSE, Info, "", Lport, "", Thost, Tport, "", "")
}

func (ts *Teamserver) TsTunnelUpdateRportfwd(tunnelId int, result bool) (string, string, error) {
	tunId := fmt.Sprintf("%08x", tunnelId)

	if result {
		tunnel, ok := ts.TunnelManager.GetTunnel(tunId)
		if ok {
			packet := CreateSpTunnelCreate(tunnel.Data)
			ts.TsSyncAllClientsWithCategory(packet, SyncCategoryTunnels)

			ts.TsNotifyTunnelAdd(tunnel)

			message := fmt.Sprintf("Reverse port forward '%s' to '%s:%s'", tunnel.Data.Port, tunnel.Data.Fhost, tunnel.Data.Fport)

			return tunnel.TaskId, message, nil
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
	return "", "", errors.New("tunnel not found")
}

/// Tunnel Stop

func (ts *Teamserver) TsTunnelStop(TunnelId string) error {
	tunnel, ok := ts.TunnelManager.GetTunnel(TunnelId)
	if !ok {
		return ErrTunnelNotFound
	}

	port, _ := strconv.Atoi(tunnel.Data.Port)
	// --- PRE HOOK ---
	preEvent := &eventing.EventDataTunnelStop{
		AgentId:    tunnel.Data.AgentId,
		TunnelId:   TunnelId,
		TunnelType: tunnel.Type,
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
		return ErrTunnelNotFound
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

func (ts *Teamserver) TsTunnelStopSocks(AgentId string, Port int) {
	port := strconv.Itoa(Port)
	id := krypt.CRC32([]byte(AgentId + "socks" + port))
	TunnelId := fmt.Sprintf("%08x", id)

	_ = ts.TsTunnelStop(TunnelId)
}

func (ts *Teamserver) TsTunnelStopLportfwd(AgentId string, Port int) {
	port := strconv.Itoa(Port)
	id := krypt.CRC32([]byte(AgentId + "lportfwd" + port))
	TunnelId := fmt.Sprintf("%08x", id)

	_ = ts.TsTunnelStop(TunnelId)
}

func (ts *Teamserver) TsTunnelStopRportfwd(AgentId string, Port int) {
	port := strconv.Itoa(Port)
	id := krypt.CRC32([]byte(AgentId + "rportfwd" + port))
	TunnelId := fmt.Sprintf("%08x", id)

	tunnel, ok := ts.TunnelManager.GetTunnel(TunnelId)
	if !ok {
		return
	}

	agent, err := ts.getAgent(tunnel.Data.AgentId)
	if err != nil {
		return
	}

	rawTaskData := tunnel.Callbacks.Close(int(id))
	tunnelManageTask(agent, rawTaskData)

	_ = ts.TsTunnelStop(TunnelId)
}

/// Connection

func (ts *Teamserver) TsTunnelChannelExists(channelId int) bool {
	_, ok := ts.TunnelManager.GetChannelByIdOnly(channelId)
	return ok
}

func (ts *Teamserver) TsTunnelGetPipe(AgentId string, channelId int) (*io.PipeReader, *io.PipeWriter, error) {
	return ts.TunnelManager.GetChannelPipesByIdOnly(channelId)
}

func (ts *Teamserver) TsTunnelConnectionData(channelId int, data []byte) {
	ts.TunnelManager.WriteToChannelByIdOnly(channelId, data)
}

func (ts *Teamserver) TsTunnelConnectionResume(AgentId string, channelId int, ioDirect bool) {
	agent, err := ts.getAgent(AgentId)
	if err != nil {
		return
	}

	entry, ok := ts.TunnelManager.GetChannelByIdOnly(channelId)
	if !ok {
		return
	}

	tunnel := entry.Tunnel
	tunChannel := entry.Channel

	if tunnel.Data.Client == "" {
		if tunChannel.conn != nil {
			if tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5 || tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5_AUTH {
				proxy.ReplySocks5StatusConn(tunChannel.conn, adaptix.SOCKS5_SUCCESS)
			} else if tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS4 {
				proxy.ReplySocks4StatusConn(tunChannel.conn, true)
			}
			relaySocketToTunnel(ts.TunnelManager, agent, tunnel, tunChannel, ioDirect)
		} else {
			logs.Debug("", "[ERROR] tunChannel.conn is nil in relaySocketToTunnel")
		}
	} else {
		if tunChannel.wsconn != nil {
			if tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5 || tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5_AUTH {
				proxy.ReplySocks5StatusWs(tunChannel.wsconn, adaptix.SOCKS5_SUCCESS)
			} else if tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS4 {
				proxy.ReplySocks4StatusWs(tunChannel.wsconn, true)
			}
			relayWebsocketToTunnel(ts.TunnelManager, agent, tunnel, tunChannel, ioDirect)
		} else {
			logs.Debug("", "[ERROR] tunChannel.wsconn is nil in relayWebsocketToTunnel")
		}
	}
}

func (ts *Teamserver) TsTunnelConnectionClose(channelId int, writeOnly bool) {
	ts.TunnelManager.CloseChannelByIdOnly(channelId, writeOnly)
}

func (ts *Teamserver) TsTunnelPause(channelId int) {
	ts.TunnelManager.PauseChannel(channelId)
}

func (ts *Teamserver) TsTunnelResume(channelId int) {
	ts.TunnelManager.ResumeChannel(channelId)
}

func (ts *Teamserver) TsTunnelConnectionHalt(channelId int, errorCode byte) {
	entry, ok := ts.TunnelManager.GetChannelByIdOnly(channelId)
	if !ok {
		return
	}

	tunnel := entry.Tunnel
	tunChannel := entry.Channel

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
	} else {
		if tunChannel.wsconn != nil {
			if tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5 || tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS5_AUTH {
				if errorCode < 1 || errorCode > 8 {
					errorCode = adaptix.SOCKS5_CONNECTION_REFUSED
				}
				proxy.ReplySocks5StatusWs(tunChannel.wsconn, errorCode)
			} else if tunnel.Type == adaptix.TUNNEL_TYPE_SOCKS4 {
				proxy.ReplySocks4StatusWs(tunChannel.wsconn, false)
			}
		}
	}
	ts.TunnelManager.CloseChannelByIdOnly(channelId, false)
}

func (ts *Teamserver) TsTunnelConnectionAccept(tunnelId int, channelId int) {
	tunId := fmt.Sprintf("%08x", tunnelId)
	tunnel, ok := ts.TunnelManager.GetTunnel(tunId)
	if !ok {
		return
	}

	agent, err := ts.getAgent(tunnel.Data.AgentId)
	if err != nil {
		return
	}

	if tunnel.Data.Client == "" {
		handlerReverseAccept(ts.TunnelManager, agent, tunnel, channelId)
	} else {
		// TODO: reverse proxy to client
	}
}

/// handlers

func handleTunChannelCreate(tm *TunnelManager, agent *Agent, tunnel *Tunnel, conn net.Conn) {
	channelId := 0
	for i := 0; i < 32; i++ {
		cid := int(rand.Uint32())
		if cid == 0 {
			continue
		}
		if !tm.ChannelExistsInTunnel(tunnel, cid) {
			channelId = cid
			break
		}
	}
	if channelId == 0 {
		_ = conn.Close()
		return
	}

	stc := NewSafeTunnelChannel(tm, channelId, conn, nil, "TCP")
	tunChannel := stc.TunnelChannel

	var taskData adaptix.TaskData
	switch tunnel.Type {

	case adaptix.TUNNEL_TYPE_SOCKS4:
		proxySock, err := proxy.CheckSocks4(conn)
		if err != nil {
			logs.Debug("", "[ERROR] Socks4 proxy error: %v", err)
			tm.closeChannelInternal(tunnel, tunChannel)
			return
		}
		taskData = tunnel.Callbacks.ConnectTCP(tunChannel.channelId, proxySock.SocksType, proxySock.AddressType, proxySock.Address, proxySock.Port)

	case adaptix.TUNNEL_TYPE_SOCKS5:
		proxySock, err := proxy.CheckSocks5(conn, false, "", "")
		if err != nil {
			logs.Debug("", "[ERROR] Socks5 proxy error: %v", err)
			tm.closeChannelInternal(tunnel, tunChannel)
			return
		}
		if proxySock.SocksCommand == 3 {
			taskData = tunnel.Callbacks.ConnectUDP(tunChannel.channelId, proxySock.SocksType, proxySock.AddressType, proxySock.Address, proxySock.Port)
			tunChannel.protocol = "UDP"
		} else {
			taskData = tunnel.Callbacks.ConnectTCP(tunChannel.channelId, proxySock.SocksType, proxySock.AddressType, proxySock.Address, proxySock.Port)
		}

	case adaptix.TUNNEL_TYPE_SOCKS5_AUTH:
		proxySock, err := proxy.CheckSocks5(conn, true, tunnel.Data.AuthUser, tunnel.Data.AuthPass)
		if err != nil {
			logs.Debug("", "Socks5 proxy error: %v", err)
			tm.closeChannelInternal(tunnel, tunChannel)
			return
		}
		if proxySock.SocksCommand == 3 {
			taskData = tunnel.Callbacks.ConnectUDP(tunChannel.channelId, proxySock.SocksType, proxySock.AddressType, proxySock.Address, proxySock.Port)
			tunChannel.protocol = "UDP"
		} else {
			taskData = tunnel.Callbacks.ConnectTCP(tunChannel.channelId, proxySock.SocksType, proxySock.AddressType, proxySock.Address, proxySock.Port)
		}

	case adaptix.TUNNEL_TYPE_LOCAL_PORT:
		tport, _ := strconv.Atoi(tunnel.Data.Fport)
		taskData = tunnel.Callbacks.ConnectTCP(tunChannel.channelId, adaptix.TUNNEL_TYPE_LOCAL_PORT, adaptix.ADDRESS_TYPE_IPV4, tunnel.Data.Fhost, tport)

	default:
		tm.closeChannelInternal(tunnel, tunChannel)
		return
	}

	tunnelManageTask(agent, taskData)

	tm.RegisterChannel(tunnel.Data.TunnelId, tunnel, tunChannel)
}

func handleTunChannelCreateClient(tm *TunnelManager, agent *Agent, tunnel *Tunnel, wsconn *websocket.Conn, channelId int, targetAddress string, targetPort int, protocol string) {
	stc := NewSafeTunnelChannel(tm, channelId, nil, wsconn, "TCP")
	tunChannel := stc.TunnelChannel

	addressType := proxy.DetectAddrType(targetAddress)

	var taskData adaptix.TaskData
	switch tunnel.Type {

	case adaptix.TUNNEL_TYPE_SOCKS4:
		taskData = tunnel.Callbacks.ConnectTCP(tunChannel.channelId, adaptix.TUNNEL_TYPE_SOCKS4, addressType, targetAddress, targetPort)

	case adaptix.TUNNEL_TYPE_SOCKS5:
		if protocol == "udp" {
			taskData = tunnel.Callbacks.ConnectUDP(tunChannel.channelId, adaptix.TUNNEL_TYPE_SOCKS5, addressType, targetAddress, targetPort)
			tunChannel.protocol = "UDP"
		} else {
			taskData = tunnel.Callbacks.ConnectTCP(tunChannel.channelId, adaptix.TUNNEL_TYPE_SOCKS5, addressType, targetAddress, targetPort)
		}

	case adaptix.TUNNEL_TYPE_SOCKS5_AUTH:
		if protocol == "udp" {
			taskData = tunnel.Callbacks.ConnectUDP(tunChannel.channelId, adaptix.TUNNEL_TYPE_SOCKS5, addressType, targetAddress, targetPort)
			tunChannel.protocol = "UDP"
		} else {
			taskData = tunnel.Callbacks.ConnectTCP(tunChannel.channelId, adaptix.TUNNEL_TYPE_SOCKS5, addressType, targetAddress, targetPort)
		}

	case adaptix.TUNNEL_TYPE_LOCAL_PORT:
		tport, _ := strconv.Atoi(tunnel.Data.Fport)
		taskData = tunnel.Callbacks.ConnectTCP(tunChannel.channelId, adaptix.TUNNEL_TYPE_LOCAL_PORT, addressType, tunnel.Data.Fhost, tport)

	default:
		tm.closeChannelInternal(tunnel, tunChannel)
		return
	}

	tunnelManageTask(agent, taskData)

	tm.RegisterChannel(tunnel.Data.TunnelId, tunnel, tunChannel)
}

func handlerReverseAccept(tm *TunnelManager, agent *Agent, tunnel *Tunnel, channelId int) {
	target := tunnel.Data.Fhost + ":" + tunnel.Data.Fport
	fwdConn, err := net.Dial("tcp", target)
	if err != nil {
		rawTaskData := tunnel.Callbacks.Close(channelId)
		tunnelManageTask(agent, rawTaskData)
		return
	}

	stc := NewSafeTunnelChannel(tm, channelId, fwdConn, nil, "TCP")
	tunChannel := stc.TunnelChannel

	tm.RegisterChannel(tunnel.Data.TunnelId, tunnel, tunChannel)

	relaySocketToTunnel(tm, agent, tunnel, tunChannel, false)
}

/// process socket

func tunnelManageTask(agent *Agent, taskData adaptix.TaskData) {
	taskData.AgentId = agent.GetData().Id
	if taskData.TaskId == "" {
		taskData.TaskId, _ = krypt.GenerateUID(8)
	}

	agent.HostedTunnelTasks.Push(taskData)
}

func relayPipeToTaskData(agent *Agent, channelId int, taskData adaptix.TaskData) {
	if taskData.TaskId == "" {
		taskData.TaskId, _ = krypt.GenerateUID(8)
	}
	taskData.AgentId = agent.GetData().Id

	agent.HostedTunnelTasks.Push(taskData)
}

func relaySocketToTunnel(tm *TunnelManager, agent *Agent, tunnel *Tunnel, tunChannel *TunnelChannel, direct bool) {
	var taskData adaptix.TaskData
	ctx, cancel := context.WithCancel(context.Background())
	tunnelId := tunnel.Data.TunnelId
	var once sync.Once
	finish := func() {
		once.Do(func() {
			cancel()
			tm.CloseChannel(tunnelId, tunChannel.channelId)
			taskData = tunnel.Callbacks.Close(tunChannel.channelId)
			tunnelManageTask(agent, taskData)
		})
	}

	go func() {
		if direct {
			defer finish()
		}
		if tunChannel.pwSrv == nil || tunChannel.conn == nil {
			logs.Debug("", "[ERROR relaySocketToTunnel] pwSrv or conn == nil — copy (pwSrv <- conn)")
			return
		}
		buf := tm.GetBuffer()
		defer tm.PutBuffer(buf)
		io.CopyBuffer(tunChannel.pwSrv, tunChannel.conn, buf)
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
					if agent.HostedTunnelTasks != nil && agent.HostedTunnelTasks.Len() > 128 {
						time.Sleep(backoff)
						if backoff < maxBackoff {
							backoff *= 2
						}
						continue
					}

					n, err := tunChannel.prSrv.Read(buf)
					if n > 0 {
						backoff = minBackoff
						var td adaptix.TaskData
						if tunChannel.protocol == "UDP" {
							td = tunnel.Callbacks.WriteUDP(tunChannel.channelId, buf[:n])
						} else {
							td = tunnel.Callbacks.WriteTCP(tunChannel.channelId, buf[:n])
						}
						relayPipeToTaskData(agent, tunChannel.channelId, td)
					}
					if err != nil {
						return
					}
				}
			}
		}()
	}
}

func relayWebsocketToTunnel(tm *TunnelManager, agent *Agent, tunnel *Tunnel, tunChannel *TunnelChannel, direct bool) {
	ctx, cancel := context.WithCancel(context.Background())
	tunnelId := tunnel.Data.TunnelId
	var once sync.Once
	finish := func() {
		once.Do(func() {
			cancel()
			tm.CloseChannel(tunnelId, tunChannel.channelId)
			taskData := tunnel.Callbacks.Close(tunChannel.channelId)
			tunnelManageTask(agent, taskData)
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
					if agent.HostedTunnelTasks != nil && agent.HostedTunnelTasks.Len() > 128 {
						time.Sleep(backoff)
						if backoff < maxBackoff {
							backoff *= 2
						}
						continue
					}

					n, err := tunChannel.prSrv.Read(buf)
					if n > 0 {
						backoff = minBackoff
						var td adaptix.TaskData
						if tunChannel.protocol == "UDP" {
							td = tunnel.Callbacks.WriteUDP(tunChannel.channelId, buf[:n])
						} else {
							td = tunnel.Callbacks.WriteTCP(tunChannel.channelId, buf[:n])
						}
						relayPipeToTaskData(agent, tunChannel.channelId, td)
					}
					if err != nil {
						return
					}
				}
			}
		}()
	}
}
