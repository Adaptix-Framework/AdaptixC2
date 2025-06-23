package server

import (
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/proxy"
	"AdaptixServer/core/utils/safe"
	"context"
	"encoding/base64"
	"errors"
	"fmt"
	"github.com/Adaptix-Framework/axc2"
	"github.com/gorilla/websocket"
	"io"
	"math/rand"
	"net"
	"strconv"
	"strings"
	"sync"
	"time"
)

func (ts *Teamserver) TsTunnelClientStart(AgentId string, Listen bool, Type int, Info string, Lhost string, Lport int, Client string, Thost string, Tport int, AuthUser string, AuthPass string) (string, error) {
	var (
		taskId   string
		tunnelId string
		err      error
	)

	value, ok := ts.agents.Get(AgentId)
	if !ok {
		return "", fmt.Errorf("agent '%v' does not exist", AgentId)
	}
	agent, _ := value.(*Agent)
	if agent.Active == false {
		return "", fmt.Errorf("agent '%v' not active", AgentId)
	}

	commandline := ""
	message := ""
	switch Type {

	case TUNNEL_SOCKS4:
		if Listen {
			commandline = fmt.Sprintf("[from browser] socks4 start %v:%v", Lhost, Lport)
			message = fmt.Sprintf("SOCKS4 server started on '%v:%v'", Lhost, Lport)
		} else {
			commandline = fmt.Sprintf("[from browser] socks4 (client) start %v:%v", Lhost, Lport)
			message = fmt.Sprintf("SOCKS4 server started on (client '%v') '%v:%v'", Client, Lhost, Lport)
		}

	case TUNNEL_SOCKS5:
		if Listen {
			commandline = fmt.Sprintf("[from browser] socks5 start %v:%v", Lhost, Lport)
			message = fmt.Sprintf("SOCKS5 server started on '%v:%v'", Lhost, Lport)
		} else {
			commandline = fmt.Sprintf("[from browser] socks5 (client) start %v:%v", Lhost, Lport)
			message = fmt.Sprintf("SOCKS5 server started on (client '%v') '%v:%v'", Client, Lhost, Lport)
		}

	case TUNNEL_SOCKS5_AUTH:
		if Listen {
			commandline = fmt.Sprintf("[from browser] socks5 start %v:%v -auth %v %v", Lhost, Lport, AuthUser, AuthPass)
			message = fmt.Sprintf("SOCKS5 (with Auth) server started on '%v:%v'", Lhost, Lport)
		} else {
			commandline = fmt.Sprintf("[from browser] socks5 (client) start %v:%v -auth %v %v", Lhost, Lport, AuthUser, AuthPass)
			message = fmt.Sprintf("SOCKS5 (with Auth) server started on (client '%v') '%v:%v'", Client, Lhost, Lport)
		}

	case TUNNEL_LPORTFWD:
		if Listen {
			commandline = fmt.Sprintf("[from browser] local_port_fwd start %v:%v %v:%v", Lhost, Lport, Thost, Tport)
			message = fmt.Sprintf("Started local port forwarding on %v:%v to %v:%v", Lhost, Lport, Thost, Tport)
		} else {
			commandline = fmt.Sprintf("[from browser] local_port_fwd (client) start on %v:%v %v:%v", Lhost, Lport, Thost, Tport)
			message = fmt.Sprintf("Started local port forwarding on (client '%v') %v:%v to %v:%v", Client, Lhost, Lport, Thost, Tport)
		}

	case TUNNEL_RPORTFWD:
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

		value, ok := ts.tunnels.Get(tunnelId)
		if !ok {
			return "", errors.New("tunnel not found")
		}
		tunnel, _ := value.(*Tunnel)
		tunnel.Active = true

		ts.TsEventTunnelAdd(tunnel)
	}

	taskData := adaptix.TaskData{
		TaskId:      taskId,
		Type:        TYPE_TUNNEL,
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

	value, ok := ts.tunnels.Get(tunnelId)
	if !ok {
		return errors.New("tunnel not found")
	}
	tunnel, _ := value.(*Tunnel)

	value, ok = ts.agents.Get(tunnel.Data.AgentId)
	if !ok {
		return errors.New("agent not found")
	}
	agent, _ := value.(*Agent)

	cid, err := strconv.ParseInt(channelId, 16, 64)
	if err != nil {
		return errors.New("channelId not supported")
	}

	port := 0
	if tunnel.Type == TUNNEL_SOCKS4 || tunnel.Type == TUNNEL_SOCKS5 || tunnel.Type == TUNNEL_SOCKS5_AUTH {
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

	if tunnel.Type == TUNNEL_SOCKS5 || tunnel.Type == TUNNEL_SOCKS5_AUTH {
		if mode != "tcp" && mode != "udp" {
			return errors.New("invalid mode")
		}
	}

	go handleTunChannelCreateClient(agent, tunnel, wsconn, int(cid), host, port, mode)

	return nil
}

func (ts *Teamserver) TsTunnelClientSetInfo(TunnelId string, Info string) error {
	value, ok := ts.tunnels.Get(TunnelId)
	if !ok {
		return errors.New("tunnel not found")
	}
	tunnel, _ := value.(*Tunnel)

	tunnel.Data.Info = Info

	packet := CreateSpTunnelEdit(tunnel.Data)
	ts.TsSyncAllClients(packet)

	return nil
}

func (ts *Teamserver) TsTunnelClientStop(TunnelId string, Client string) error {
	value, ok := ts.tunnels.Get(TunnelId)
	if !ok {
		return errors.New("tunnel Not Found")
	}
	tunnel, _ := value.(*Tunnel)

	if tunnel.Data.Client == "" {
		_ = ts.TsTunnelStop(TunnelId)
		return nil
	}

	if tunnel.Data.Client == Client {
		value, ok = ts.tunnels.GetDelete(TunnelId)
		if !ok {
			return errors.New("tunnel Not Found")
		}
		tunnel, _ = value.(*Tunnel)

		tunnel.connections.ForEach(func(key string, valueConn interface{}) bool {
			tunChannel, _ := valueConn.(*TunnelChannel)
			if tunChannel.wsconn != nil {
				_ = tunChannel.wsconn.Close()
			}

			tunChannel.pwTun.Close()
			tunChannel.prTun.Close()
			tunChannel.pwSrv.Close()
			tunChannel.prSrv.Close()

			return true
		})

		packet := CreateSpTunnelDelete(tunnel.Data)
		ts.TsSyncAllClients(packet)

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

	value, ok := ts.tunnels.Get(TunnelId)
	if !ok {
		return "", errors.New("tunnel not found")
	}
	tunnel, _ := value.(*Tunnel)

	value, ok = ts.agents.Get(tunnel.Data.AgentId)
	if !ok {
		return "", errors.New("agent not found")
	}
	agent, _ := value.(*Agent)

	if tunnel.Type == TUNNEL_RPORTFWD {

		id, _ := strconv.ParseInt(TunnelId, 16, 64)
		port, _ := strconv.Atoi(tunnel.Data.Port)
		taskData := tunnel.handlerReverse(int(id), port)
		tunnelManageTask(agent, taskData)

	} else {

		address := tunnel.Data.Interface + ":" + tunnel.Data.Port
		listener, err := net.Listen("tcp", address)
		if err != nil {
			ts.tunnels.Delete(TunnelId)
			return "", err
		}
		tunnel.listener = listener

		go func() {
			for {
				var conn net.Conn
				conn, err = tunnel.listener.Accept()
				if err != nil {
					return
				}
				go handleTunChannelCreate(agent, tunnel, conn)
			}
		}()

		time.Sleep(300 * time.Millisecond)

		if err != nil {
			ts.tunnels.Delete(TunnelId)
			return "", err
		}
	}

	tunnel.Active = true

	ts.TsEventTunnelAdd(tunnel)

	return tunnel.TaskId, nil
}

func (ts *Teamserver) TsTunnelCreate(AgentId string, Type int, Info string, Lhost string, Lport int, Client string, Thost string, Tport int, AuthUser string, AuthPass string) (string, error) {

	value, ok := ts.agents.Get(AgentId)
	if !ok {
		return "", errors.New("agent not found")
	}
	agent, _ := value.(*Agent)

	tunnelData := adaptix.TunnelData{
		AgentId:  agent.Data.Id,
		Computer: agent.Data.Computer,
		Username: agent.Data.Username,
		Process:  agent.Data.Process,
		Info:     Info,
		Client:   Client,
	}

	lport := strconv.Itoa(Lport)
	tport := strconv.Itoa(Tport)

	switch Type {

	case TUNNEL_SOCKS4:
		tunnelData.Type = "SOCKS4 proxy"
		tunnelData.TunnelId = fmt.Sprintf("%08x", krypt.CRC32([]byte(Client+agent.Data.Id+"socks"+lport)))
		tunnelData.Interface = Lhost
		tunnelData.Port = lport

	case TUNNEL_SOCKS5:
		tunnelData.Type = "SOCKS5 proxy"
		tunnelData.TunnelId = fmt.Sprintf("%08x", krypt.CRC32([]byte(Client+agent.Data.Id+"socks"+lport)))
		tunnelData.Interface = Lhost
		tunnelData.Port = lport

	case TUNNEL_SOCKS5_AUTH:
		tunnelData.Type = "SOCKS5 Auth proxy"
		tunnelData.TunnelId = fmt.Sprintf("%08x", krypt.CRC32([]byte(Client+agent.Data.Id+"socks"+lport)))
		tunnelData.Interface = Lhost
		tunnelData.Port = lport
		tunnelData.AuthUser = AuthUser
		tunnelData.AuthPass = AuthPass

	case TUNNEL_LPORTFWD:
		tunnelData.Type = "Local port forward"
		tunnelData.TunnelId = fmt.Sprintf("%08x", krypt.CRC32([]byte(Client+agent.Data.Id+"lportfwd"+lport)))
		tunnelData.Interface = Lhost
		tunnelData.Port = lport
		tunnelData.Fhost = Thost
		tunnelData.Fport = tport

	case TUNNEL_RPORTFWD:
		tunnelData.Type = "Reverse port forward"
		tunnelData.TunnelId = fmt.Sprintf("%08x", krypt.CRC32([]byte(Client+agent.Data.Id+"rportfwd"+lport)))
		tunnelData.Port = lport
		tunnelData.Fhost = Thost
		tunnelData.Fport = tport

	default:
		return "", errors.New("invalid tunnel type")
	}

	fConnTCP, fConnUDP, fWriteTCP, fWriteUDP, fClose, fReverse, err := ts.Extender.ExAgentTunnelCallbacks(agent.Data, Type)
	if err != nil {
		return "", err
	}

	tunnel := &Tunnel{
		connections: safe.NewMap(),
		Data:        tunnelData,
		Type:        Type,
		Active:      false,

		handlerConnectTCP: fConnTCP,
		handlerConnectUDP: fConnUDP,
		handlerWriteTCP:   fWriteTCP,
		handlerWriteUDP:   fWriteUDP,
		handlerClose:      fClose,
		handlerReverse:    fReverse,
	}

	value, ok = ts.tunnels.Get(tunnel.Data.TunnelId)
	if ok {
		t, _ := value.(*Tunnel)
		if t.Active {
			return "", errors.New("Tunnel already active")
		} else {
			ts.tunnels.Delete(tunnel.Data.TunnelId)
		}
	}

	ts.tunnels.Put(tunnel.Data.TunnelId, tunnel)

	return tunnel.Data.TunnelId, nil
}

func (ts *Teamserver) TsTunnelCreateSocks4(AgentId string, Info string, Lhost string, Lport int) (string, error) {
	return ts.TsTunnelCreate(AgentId, TUNNEL_SOCKS4, Info, Lhost, Lport, "", "", 0, "", "")
}

func (ts *Teamserver) TsTunnelCreateSocks5(AgentId string, Info string, Lhost string, Lport int, UseAuth bool, Username string, Password string) (string, error) {
	if UseAuth {
		return ts.TsTunnelCreate(AgentId, TUNNEL_SOCKS5_AUTH, Info, Lhost, Lport, "", "", 0, Username, Password)
	} else {
		return ts.TsTunnelCreate(AgentId, TUNNEL_SOCKS5, Info, Lhost, Lport, "", "", 0, "", "")
	}
}

func (ts *Teamserver) TsTunnelCreateLportfwd(AgentId string, Info string, Lhost string, Lport int, Thost string, Tport int) (string, error) {
	return ts.TsTunnelCreate(AgentId, TUNNEL_LPORTFWD, Info, Lhost, Lport, "", Thost, Tport, "", "")
}

func (ts *Teamserver) TsTunnelCreateRportfwd(AgentId string, Info string, Lport int, Thost string, Tport int) (string, error) {
	return ts.TsTunnelCreate(AgentId, TUNNEL_RPORTFWD, Info, "", Lport, "", Thost, Tport, "", "")
}

func (ts *Teamserver) TsTunnelUpdateRportfwd(tunnelId int, result bool) (string, string, error) {
	var (
		tunnel *Tunnel
		value  interface{}
		ok     bool
	)
	tunId := fmt.Sprintf("%08x", tunnelId)

	if result == true {
		value, ok = ts.tunnels.Get(tunId)
		if ok {
			tunnel, _ = value.(*Tunnel)

			message := fmt.Sprintf("Reverse port forward '%s' to '%s:%s'", tunnel.Data.Port, tunnel.Data.Fhost, tunnel.Data.Fport)

			packet := CreateSpTunnelCreate(tunnel.Data)
			ts.TsSyncAllClients(packet)

			packet2 := CreateSpEvent(EVENT_TUNNEL_START, message)
			ts.TsSyncAllClients(packet2)
			ts.events.Put(packet2)

			return tunnel.TaskId, message, nil
		}
	} else {
		value, ok = ts.tunnels.GetDelete(tunId)
		if ok {
			tunnel, _ = value.(*Tunnel)

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
	value, ok := ts.tunnels.GetDelete(TunnelId)
	if !ok {
		return errors.New("tunnel Not Found")
	}
	tunnel, _ := value.(*Tunnel)

	if tunnel.listener != nil {
		_ = tunnel.listener.Close()
	}

	tunnel.connections.ForEach(func(key string, valueConn interface{}) bool {
		tunChannel, _ := valueConn.(*TunnelChannel)
		if tunnel.Data.Client == "" {
			if tunChannel.conn != nil {
				_ = tunChannel.conn.Close()
			}
		} else {
			if tunChannel.wsconn != nil {
				_ = tunChannel.wsconn.Close()
			}
		}

		tunChannel.pwTun.Close()
		tunChannel.prTun.Close()
		tunChannel.pwSrv.Close()
		tunChannel.prSrv.Close()

		return true
	})

	packet := CreateSpTunnelDelete(tunnel.Data)
	ts.TsSyncAllClients(packet)

	taskData := adaptix.TaskData{
		TaskId:     tunnel.TaskId,
		Completed:  true,
		FinishDate: time.Now().Unix(),
	}

	ts.TsTaskUpdate(tunnel.Data.AgentId, taskData)

	if tunnel.Data.Client == "" {
		message := ""
		switch tunnel.Type {
		case TUNNEL_SOCKS4:
			message = fmt.Sprintf("SOCKS4 server ':%s' stopped", tunnel.Data.Port)
		case TUNNEL_SOCKS5:
			message = fmt.Sprintf("SOCKS5 server ':%s' stopped", tunnel.Data.Port)
		case TUNNEL_SOCKS5_AUTH:
			message = fmt.Sprintf("SOCKS5 (with Auth) server ':%s' stopped", tunnel.Data.Port)
		case TUNNEL_LPORTFWD:
			message = fmt.Sprintf("Local port forward on ':%s' stopped", tunnel.Data.Port)
		case TUNNEL_RPORTFWD:
			message = fmt.Sprintf("Remote port forward to '%s:%s' stopped", tunnel.Data.Fhost, tunnel.Data.Fport)
		default:
			return errors.New("tunnel type not supported")
		}

		packet2 := CreateSpEvent(EVENT_TUNNEL_STOP, message)
		ts.TsSyncAllClients(packet2)
		ts.events.Put(packet2)
	}

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

	value, ok := ts.tunnels.Get(TunnelId)
	if !ok {
		return
	}
	tunnel, _ := value.(*Tunnel)

	value, ok = ts.agents.Get(tunnel.Data.AgentId)
	if !ok {
		return
	}
	agent, _ := value.(*Agent)

	rawTaskData := tunnel.handlerClose(int(id))
	tunnelManageTask(agent, rawTaskData)

	_ = ts.TsTunnelStop(TunnelId)
}

/// Connection

func (ts *Teamserver) TsTunnelChannelExists(channelId int) bool {
	var (
		tunnel *Tunnel
		ok     bool
	)

	cid := strconv.Itoa(channelId)
	ts.tunnels.ForEach(func(key string, valueTun interface{}) bool {
		tunnel, _ = valueTun.(*Tunnel)
		ok = tunnel.connections.Contains(cid)
		if ok {
			return false
		}
		return true
	})

	return ok
}

func (ts *Teamserver) TsTunnelGetPipe(AgentId string, channelId int) (*io.PipeReader, *io.PipeWriter, error) {
	var (
		valueConn  interface{}
		tunnel     *Tunnel
		tunChannel *TunnelChannel
		ok         bool
	)

	ts.tunnels.ForEach(func(key string, valueTun interface{}) bool {
		tunnel, _ = valueTun.(*Tunnel)
		valueConn, ok = tunnel.connections.Get(strconv.Itoa(channelId))
		if ok {
			tunChannel, _ = valueConn.(*TunnelChannel)
			return false
		}
		return true
	})

	if !ok {
		return nil, nil, errors.New("tunnel connection not found")
	}

	return tunChannel.prSrv, tunChannel.pwTun, nil
}

func (ts *Teamserver) TsTunnelConnectionData(channelId int, data []byte) {
	var (
		tunnel     *Tunnel
		valueConn  interface{}
		tunChannel *TunnelChannel
		ok         bool
	)

	ts.tunnels.ForEach(func(key string, valueTun interface{}) bool {
		tunnel, _ = valueTun.(*Tunnel)
		valueConn, ok = tunnel.connections.Get(strconv.Itoa(channelId))
		if ok {
			tunChannel, _ = valueConn.(*TunnelChannel)
			return false
		}
		return true
	})

	if ok {
		go tunChannel.pwTun.Write(data)
	}
}

func (ts *Teamserver) TsTunnelConnectionResume(AgentId string, channelId int, ioDirect bool) {
	var (
		valueConn  interface{}
		tunnel     *Tunnel
		tunChannel *TunnelChannel
		ok         bool
	)

	value, ok := ts.agents.Get(AgentId)
	if !ok {
		return
	}
	agent, _ := value.(*Agent)

	ts.tunnels.ForEach(func(key string, valueTun interface{}) bool {
		tunnel, _ = valueTun.(*Tunnel)
		valueConn, ok = tunnel.connections.Get(strconv.Itoa(channelId))
		if ok {
			tunChannel, _ = valueConn.(*TunnelChannel)
			return false
		}
		return true
	})

	if ok {
		if tunnel.Data.Client == "" {
			relaySocketToTunnel(agent, tunnel, tunChannel, ioDirect)
		} else {
			relayWebsocketToTunnel(agent, tunnel, tunChannel, ioDirect)
		}
	}
}

func (ts *Teamserver) TsTunnelConnectionClose(channelId int) {
	var (
		valueConn  interface{}
		tunnel     *Tunnel
		tunChannel *TunnelChannel
		ok         bool
	)

	ts.tunnels.ForEach(func(key string, valueTun interface{}) bool {
		tunnel, _ = valueTun.(*Tunnel)
		valueConn, ok = tunnel.connections.Get(strconv.Itoa(channelId))
		if ok {
			tunChannel, _ = valueConn.(*TunnelChannel)
			return false
		}
		return true
	})

	if ok {
		if tunnel.Data.Client == "" {
			if tunChannel.conn != nil {
				_ = tunChannel.conn.Close()
			}
		} else {
			if tunChannel.wsconn != nil {
				_ = tunChannel.wsconn.Close()
			}
		}

		tunChannel.pwTun.Close()
		tunChannel.prTun.Close()
		tunChannel.pwSrv.Close()
		tunChannel.prSrv.Close()

		tunnel.connections.Delete(strconv.Itoa(channelId))
	}
}

func (ts *Teamserver) TsTunnelConnectionAccept(tunnelId int, channelId int) {
	tunId := fmt.Sprintf("%08x", tunnelId)
	value, ok := ts.tunnels.Get(tunId)
	if !ok {
		return
	}
	tunnel, _ := value.(*Tunnel)

	value, ok = ts.agents.Get(tunnel.Data.AgentId)
	if !ok {
		return
	}
	agent, _ := value.(*Agent)

	if tunnel.Data.Client == "" {
		handlerReverseAccept(agent, tunnel, channelId)
	} else {
		// TODO: reverse proxy to client
	}
}

/// handlers

func handleTunChannelCreate(agent *Agent, tunnel *Tunnel, conn net.Conn) {

	tunChannel := &TunnelChannel{
		channelId: int(rand.Uint32()),
		conn:      conn,
		protocol:  "TCP",
	}

	tunChannel.prSrv, tunChannel.pwSrv = io.Pipe()
	tunChannel.prTun, tunChannel.pwTun = io.Pipe()

	var taskData adaptix.TaskData
	switch tunnel.Type {

	case TUNNEL_SOCKS4:
		targetAddress, targetPort, err := proxy.CheckSocks4(conn)
		if err != nil {
			//fmt.Println("Socks4 proxy error: ", err)
			return
		}
		taskData = tunnel.handlerConnectTCP(tunChannel.channelId, targetAddress, targetPort)

	case TUNNEL_SOCKS5:
		targetAddress, targetPort, socksCommand, err := proxy.CheckSocks5(conn)
		if err != nil {
			//fmt.Println("Socks5 proxy error: ", err)
			return
		}
		if socksCommand == 3 {
			taskData = tunnel.handlerConnectUDP(tunChannel.channelId, targetAddress, targetPort)
			tunChannel.protocol = "UDP"
		} else {
			taskData = tunnel.handlerConnectTCP(tunChannel.channelId, targetAddress, targetPort)
		}

	case TUNNEL_SOCKS5_AUTH:
		targetAddress, targetPort, socksCommand, err := proxy.CheckSocks5Auth(conn, tunnel.Data.AuthUser, tunnel.Data.AuthPass)
		if err != nil {
			//fmt.Println("Socks5 proxy error: ", err)
			return
		}
		if socksCommand == 3 {
			taskData = tunnel.handlerConnectUDP(tunChannel.channelId, targetAddress, targetPort)
			tunChannel.protocol = "UDP"
		} else {
			taskData = tunnel.handlerConnectTCP(tunChannel.channelId, targetAddress, targetPort)
		}

	case TUNNEL_LPORTFWD:
		tport, _ := strconv.Atoi(tunnel.Data.Fport)
		taskData = tunnel.handlerConnectTCP(tunChannel.channelId, tunnel.Data.Fhost, tport)

	default:
		return
	}

	tunnelManageTask(agent, taskData)

	tunnel.connections.Put(strconv.Itoa(tunChannel.channelId), tunChannel)
}

func handleTunChannelCreateClient(agent *Agent, tunnel *Tunnel, wsconn *websocket.Conn, channelId int, targetAddress string, targetPort int, protocol string) {
	tunChannel := &TunnelChannel{
		channelId: channelId,
		conn:      nil,
		wsconn:    wsconn,
		protocol:  "TCP",
	}

	tunChannel.prSrv, tunChannel.pwSrv = io.Pipe()
	tunChannel.prTun, tunChannel.pwTun = io.Pipe()

	var taskData adaptix.TaskData
	switch tunnel.Type {

	case TUNNEL_SOCKS4:
		taskData = tunnel.handlerConnectTCP(tunChannel.channelId, targetAddress, targetPort)

	case TUNNEL_SOCKS5:
		if protocol == "udp" {
			taskData = tunnel.handlerConnectUDP(tunChannel.channelId, targetAddress, targetPort)
			tunChannel.protocol = "UDP"
		} else {
			taskData = tunnel.handlerConnectTCP(tunChannel.channelId, targetAddress, targetPort)
		}

	case TUNNEL_SOCKS5_AUTH:
		if protocol == "udp" {
			taskData = tunnel.handlerConnectUDP(tunChannel.channelId, targetAddress, targetPort)
			tunChannel.protocol = "UDP"
		} else {
			taskData = tunnel.handlerConnectTCP(tunChannel.channelId, targetAddress, targetPort)
		}

	case TUNNEL_LPORTFWD:
		tport, _ := strconv.Atoi(tunnel.Data.Fport)
		taskData = tunnel.handlerConnectTCP(tunChannel.channelId, tunnel.Data.Fhost, tport)

	default:
		return
	}

	tunnelManageTask(agent, taskData)

	tunnel.connections.Put(strconv.Itoa(tunChannel.channelId), tunChannel)
}

func handlerReverseAccept(agent *Agent, tunnel *Tunnel, channelId int) {
	target := tunnel.Data.Fhost + ":" + tunnel.Data.Fport
	fwdConn, err := net.Dial("tcp", target)
	if err != nil {
		rawTaskData := tunnel.handlerClose(channelId)
		tunnelManageTask(agent, rawTaskData)
		return
	}

	tunChannel := &TunnelChannel{
		channelId: channelId,
		conn:      fwdConn,
		protocol:  "TCP",
	}

	tunChannel.prSrv, tunChannel.pwSrv = io.Pipe()
	tunChannel.prTun, tunChannel.pwTun = io.Pipe()

	tunnel.connections.Put(strconv.Itoa(channelId), tunChannel)

	relaySocketToTunnel(agent, tunnel, tunChannel, false)
}

/// process socket

func tunnelManageTask(agent *Agent, taskData adaptix.TaskData) {
	taskData.AgentId = agent.Data.Id
	if taskData.TaskId == "" {
		taskData.TaskId, _ = krypt.GenerateUID(8)
	}

	agent.TunnelConnectTasks.Put(taskData)
}

func relayPipeToTaskData(agent *Agent, channelId int, taskData adaptix.TaskData) {
	if taskData.TaskId == "" {
		taskData.TaskId, _ = krypt.GenerateUID(8)
	}
	taskData.AgentId = agent.Data.Id

	taskTunnel := adaptix.TaskDataTunnel{
		ChannelId: channelId,
		Data:      taskData,
	}

	agent.TunnelQueue.Put(taskTunnel)
}

func relaySocketToTunnel(agent *Agent, tunnel *Tunnel, tunChannel *TunnelChannel, direct bool) {
	var taskData adaptix.TaskData
	ctx, cancel := context.WithCancel(context.Background())

	var closeOnce sync.Once
	closeChannel := func() {
		closeOnce.Do(func() {
			cancel()
			taskData := tunnel.handlerClose(tunChannel.channelId)
			tunnelManageTask(agent, taskData)
			return
		})
	}

	go func() {
		io.Copy(tunChannel.pwSrv, tunChannel.conn)
		closeChannel()
	}()

	go func() {
		io.Copy(tunChannel.conn, tunChannel.prTun)
		closeChannel()
	}()

	if !direct {
		go func() {
			buf := make([]byte, 0x8000)
			for {
				select {
				case <-ctx.Done():
					closeChannel()
					return

				default:
					n, err := tunChannel.prSrv.Read(buf)
					if err != nil {
						closeChannel()
						return
					}
					if n > 0 {
						if tunChannel.protocol == "UDP" {
							taskData = tunnel.handlerWriteUDP(tunChannel.channelId, buf[:n])
						} else {
							taskData = tunnel.handlerWriteTCP(tunChannel.channelId, buf[:n])
						}
						relayPipeToTaskData(agent, tunChannel.channelId, taskData)
					}
				}
			}
		}()
	}
}

func relayWebsocketToTunnel(agent *Agent, tunnel *Tunnel, tunChannel *TunnelChannel, direct bool) {
	var taskData adaptix.TaskData
	ctx, cancel := context.WithCancel(context.Background())

	var closeOnce sync.Once
	closeChannel := func() {
		closeOnce.Do(func() {
			cancel()
			_ = tunChannel.wsconn.Close()
			taskData = tunnel.handlerClose(tunChannel.channelId)
			tunnelManageTask(agent, taskData)
		})
	}

	go func() {
		for {
			_, msg, err := tunChannel.wsconn.ReadMessage()
			if err != nil {
				break
			}
			_, err = tunChannel.pwSrv.Write(msg)
			if err != nil {
				break
			}
		}
		closeChannel()
	}()

	go func() {
		buf := make([]byte, 0x8000)
		for {
			n, err := tunChannel.prTun.Read(buf)
			if err != nil {
				break
			}
			if err := tunChannel.wsconn.WriteMessage(websocket.BinaryMessage, buf[:n]); err != nil {
				break
			}
		}
		closeChannel()
	}()

	if !direct {
		go func() {
			buf := make([]byte, 0x8000)
			for {
				select {
				case <-ctx.Done():
					closeChannel()
					return
				default:
					n, err := tunChannel.prSrv.Read(buf)
					if err != nil {
						closeChannel()
						return
					}
					if n > 0 {
						if tunChannel.protocol == "UDP" {
							taskData = tunnel.handlerWriteUDP(tunChannel.channelId, buf[:n])
						} else {
							taskData = tunnel.handlerWriteTCP(tunChannel.channelId, buf[:n])
						}
						relayPipeToTaskData(agent, tunChannel.channelId, taskData)
					}
				}
			}
		}()
	}
}
