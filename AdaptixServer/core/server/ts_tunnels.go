package server

import (
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/proxy"
	"AdaptixServer/core/utils/safe"
	"context"
	"errors"
	"fmt"
	"github.com/Adaptix-Framework/axc2"
	"io"
	"math/rand"
	"net"
	"strconv"
	"time"
)

func (ts *Teamserver) TsTunnelTaskStartSocks5(agentId string, clientName string, desc string, lhost string, lport int, auth bool, username string, password string) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}

	agent, _ := value.(*Agent)
	if agent.Active == false {
		return fmt.Errorf("agent '%v' not active", agentId)
	}

	taskData, err := ts.Extender.ExAgentTunnelTaskSock5(agent.Data, desc, lhost, lport, auth, username, password)
	if err != nil {
		return err
	}

	taskData.Type = TYPE_TUNNEL
	taskData.Sync = true
	commandline := ""
	if auth {
		commandline = fmt.Sprintf("[from browser] socks5 start %v:%v -auth %v %v", lhost, lport, username, password)
		taskData.Message = fmt.Sprintf("Started socks5 tunnel (with Auth) on %v:%v", lhost, lport)
	} else {
		commandline = fmt.Sprintf("[from browser] socks5 start %v:%v", lhost, lport)
		taskData.Message = fmt.Sprintf("Started socks5 tunnel on %v:%v", lhost, lport)
	}
	taskData.MessageType = CONSOLE_OUT_SUCCESS

	ts.TsTaskCreate(agentId, commandline, clientName, taskData)
	return nil
}

func (ts *Teamserver) TsTunnelTaskStartSocks4(agentId string, clientName string, desc string, lhost string, lport int) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}

	agent, _ := value.(*Agent)
	if agent.Active == false {
		return fmt.Errorf("agent '%v' not active", agentId)
	}

	taskData, err := ts.Extender.ExAgentTunnelTaskSock4(agent.Data, desc, lhost, lport)
	if err != nil {
		return err
	}

	commandline := fmt.Sprintf("[from browser] socks4 start %v:%v", lhost, lport)

	taskData.Type = TYPE_TUNNEL
	taskData.Sync = true
	taskData.Message = fmt.Sprintf("Started socks4 tunnel on %v:%v", lhost, lport)
	taskData.MessageType = CONSOLE_OUT_SUCCESS

	ts.TsTaskCreate(agentId, commandline, clientName, taskData)
	return nil
}

func (ts *Teamserver) TsTunnelTaskStartLpf(agentId string, clientName string, desc string, lhost string, lport int, thost string, tport int) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}

	agent, _ := value.(*Agent)
	if agent.Active == false {
		return fmt.Errorf("agent '%v' not active", agentId)
	}

	taskData, err := ts.Extender.ExAgentTunnelTaskLpf(agent.Data, desc, lhost, lport, thost, tport)
	if err != nil {
		return err
	}

	commandline := fmt.Sprintf("[from browser] local_port_fwd start %v:%v %v:%v", lhost, lport, thost, tport)

	taskData.Type = TYPE_TUNNEL
	taskData.Sync = true
	taskData.Message = fmt.Sprintf("Started local port forwarding on %s:%d to %s:%d", lhost, lport, thost, tport)
	taskData.MessageType = CONSOLE_OUT_SUCCESS

	ts.TsTaskCreate(agentId, commandline, clientName, taskData)
	return nil
}

func (ts *Teamserver) TsTunnelTaskStartRpf(agentId string, clientName string, desc string, port int, thost string, tport int) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}

	agent, _ := value.(*Agent)
	if agent.Active == false {
		return fmt.Errorf("agent '%v' not active", agentId)
	}

	taskData, err := ts.Extender.ExAgentTunnelTaskRpf(agent.Data, desc, port, thost, tport)
	if err != nil {
		return err
	}

	commandline := fmt.Sprintf("[from browser] reverse_port_fwd start %v %v:%v", port, thost, tport)

	taskData.Type = TYPE_TUNNEL
	taskData.Sync = true
	taskData.Message = fmt.Sprintf("Starting reverse port forwarding %d to %s:%d", port, thost, tport)
	taskData.MessageType = CONSOLE_OUT_INFO

	ts.TsTaskCreate(agentId, commandline, clientName, taskData)
	return nil
}

func (ts *Teamserver) TsTunnelTaskStop(TunnelId string) error {
	value, ok := ts.tunnels.GetDelete(TunnelId)
	if !ok {
		return errors.New("tunnel Not Found")
	}
	tunnel, _ := value.(*Tunnel)

	if tunnel.listener != nil {
		_ = tunnel.listener.Close()
	}

	tunnel.connections.ForEach(func(key string, valueConn interface{}) bool {
		tunnelConnection, _ := valueConn.(*TunnelConnection)
		if tunnelConnection.conn != nil {
			tunnelConnection.handleCancel()
			_ = tunnelConnection.conn.Close()
		}
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

	message := ""
	if tunnel.Data.Type == "SOCKS5 proxy" {
		message = fmt.Sprintf("SOCKS5 server ':%s' stopped", tunnel.Data.Port)
	} else if tunnel.Data.Type == "SOCKS4 proxy" {
		message = fmt.Sprintf("SOCKS4 server ':%s' stopped", tunnel.Data.Port)
	} else if tunnel.Data.Type == "SOCKS5 Auth proxy" {
		message = fmt.Sprintf("SOCKS5 (with Auth) server ':%s' stopped", tunnel.Data.Port)
	} else if tunnel.Data.Type == "Local port forward" {
		message = fmt.Sprintf("Local port forward on ':%s' stopped", tunnel.Data.Port)
	} else if tunnel.Data.Type == "Remote port forward" {
		message = fmt.Sprintf("Remote port forward to '%s:%s' stopped", tunnel.Data.Fhost, tunnel.Data.Fport)
	}

	packet2 := CreateSpEvent(EVENT_TUNNEL_STOP, message)
	ts.TsSyncAllClients(packet2)
	ts.events.Put(packet2)

	return nil
}

func (ts *Teamserver) TsTunnelSetInfo(TunnelId string, Info string) error {
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

/// Ts

func (ts *Teamserver) TsTunnelAdd(tunnel *Tunnel) {
	message := ""
	if tunnel.Type == TUNNEL_SOCKS5 {
		message = fmt.Sprintf("SOCKS5 server started on '%s:%s'", tunnel.Data.Interface, tunnel.Data.Port)
	} else if tunnel.Type == TUNNEL_SOCKS4 {
		message = fmt.Sprintf("SOCKS4 server started on '%s:%s'", tunnel.Data.Interface, tunnel.Data.Port)
	} else if tunnel.Type == TUNNEL_SOCKS5_AUTH {
		message = fmt.Sprintf("SOCKS5 (with Auth) server started on '%s:%s'", tunnel.Data.Interface, tunnel.Data.Port)
	} else if tunnel.Type == TUNNEL_LPORTFWD {
		message = fmt.Sprintf("Local port forward started on '%s:%s'", tunnel.Data.Interface, tunnel.Data.Port)
	}

	tunnel.TaskId, _ = krypt.GenerateUID(8)

	ts.tunnels.Put(tunnel.Data.TunnelId, tunnel)

	packet := CreateSpTunnelCreate(tunnel.Data)
	ts.TsSyncAllClients(packet)

	packet2 := CreateSpEvent(EVENT_TUNNEL_START, message)
	ts.TsSyncAllClients(packet2)
	ts.events.Put(packet2)
}

func (ts *Teamserver) TsReverseAdd(tunnel *Tunnel) {
	packet := CreateSpTunnelCreate(tunnel.Data)
	ts.TsSyncAllClients(packet)

	message := fmt.Sprintf("Reverse port forward to '%s:%s'", tunnel.Data.Fhost, tunnel.Data.Fport)
	packet2 := CreateSpEvent(EVENT_TUNNEL_START, message)
	ts.TsSyncAllClients(packet2)
	ts.events.Put(packet2)
}

/// Socks5

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
		taskData := tunnel.handleReverse(int(id), port)
		sendTunnelTaskData(agent, taskData)

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
				go handleTunnelConnection(agent, tunnel, conn)
			}
		}()

		time.Sleep(300 * time.Millisecond)

		if err != nil {
			ts.tunnels.Delete(TunnelId)
			return "", err
		}
	}

	ts.TsTunnelAdd(tunnel)

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
		tunnelData.TunnelId = fmt.Sprintf("%08x", krypt.CRC32([]byte(agent.Data.Id+"socks"+lport)))
		tunnelData.Interface = Lhost
		tunnelData.Port = lport

	case TUNNEL_SOCKS5:
		tunnelData.Type = "SOCKS5 proxy"
		tunnelData.TunnelId = fmt.Sprintf("%08x", krypt.CRC32([]byte(agent.Data.Id+"socks"+lport)))
		tunnelData.Interface = Lhost
		tunnelData.Port = lport

	case TUNNEL_SOCKS5_AUTH:
		tunnelData.Type = "SOCKS5 Auth proxy"
		tunnelData.TunnelId = fmt.Sprintf("%08x", krypt.CRC32([]byte(agent.Data.Id+"socks"+lport)))
		tunnelData.Interface = Lhost
		tunnelData.Port = lport
		tunnelData.AuthUser = AuthUser
		tunnelData.AuthPass = AuthPass

	case TUNNEL_LPORTFWD:
		tunnelData.Type = "Local port forward"
		tunnelData.TunnelId = fmt.Sprintf("%08x", krypt.CRC32([]byte(agent.Data.Id+"lportfwd"+lport)))
		tunnelData.Interface = Lhost
		tunnelData.Port = lport
		tunnelData.Fhost = Thost
		tunnelData.Fport = tport

	case TUNNEL_RPORTFWD:
		tunnelData.Type = "Reverse port forward"
		tunnelData.TunnelId = fmt.Sprintf("%08x", krypt.CRC32([]byte(agent.Data.Id+"rportfwd"+lport)))
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
		handleReverse:     fReverse,
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

func (ts *Teamserver) TsTunnelStopSocks(AgentId string, Port int) {
	port := strconv.Itoa(Port)
	id := krypt.CRC32([]byte(AgentId + "socks" + port))
	TunnelId := fmt.Sprintf("%08x", id)

	_ = ts.TsTunnelTaskStop(TunnelId)
}

// no
func (ts *Teamserver) TsTunnelStopLocalPortFwd(AgentId string, Port int) {
	port := strconv.Itoa(Port)
	id := krypt.CRC32([]byte(AgentId + "lportfwd" + port))
	TunnelId := fmt.Sprintf("%08x", id)

	_ = ts.TsTunnelTaskStop(TunnelId)
}

/// Port Forward

// no
func (ts *Teamserver) TsTunnelStateRemotePortFwd(tunnelId int, result bool) (string, string, error) {
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
				Message:     "This port is already in use",
				FinishDate:  time.Now().Unix(),
				Completed:   true,
			}

			ts.TsTaskUpdate(tunnel.Data.AgentId, taskData)

			return tunnel.TaskId, "", errors.New("this port is already in use")
		}
	}
	return "", "", errors.New("tunnel not found")
}

func (ts *Teamserver) TsTunnelStopRemotePortFwd(AgentId string, Port int) {
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
	sendTunnelTaskData(agent, rawTaskData)

	_ = ts.TsTunnelTaskStop(TunnelId)
}

/// Connection

func (ts *Teamserver) TsTunnelConnectionData(channelId int, data []byte) {
	var (
		valueConn        interface{}
		tunnelConnection *TunnelConnection
		ok               bool
	)

	ts.tunnels.ForEach(func(key string, valueTun interface{}) bool {
		tunnel, _ := valueTun.(*Tunnel)
		valueConn, ok = tunnel.connections.Get(strconv.Itoa(channelId))
		if ok {
			tunnelConnection, _ = valueConn.(*TunnelConnection)
			return false
		}
		return true
	})

	if ok {
		go tunnelDataToSocket(tunnelConnection, data)
	}
}

func (ts *Teamserver) TsTunnelConnectionResume(AgentId string, channelId int) {
	var (
		valueConn        interface{}
		tunnel           *Tunnel
		tunnelConnection *TunnelConnection
		ok               bool
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
			tunnelConnection, _ = valueConn.(*TunnelConnection)
			return false
		}
		return true
	})

	if ok {
		go socketToTunnelData(agent, tunnel, tunnelConnection)
	}
}

func (ts *Teamserver) TsTunnelConnectionClose(channelId int) {
	var (
		valueConn        interface{}
		tunnel           *Tunnel
		tunnelConnection *TunnelConnection
		ok               bool
	)

	ts.tunnels.ForEach(func(key string, valueTun interface{}) bool {
		tunnel, _ = valueTun.(*Tunnel)
		valueConn, ok = tunnel.connections.Get(strconv.Itoa(channelId))
		if ok {
			tunnelConnection, _ = valueConn.(*TunnelConnection)
			return false
		}
		return true
	})

	if ok {
		tunnelConnection.handleCancel()
		_ = tunnelConnection.conn.Close()
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

	handlerReverseAccept(agent, tunnel, channelId)
}

/// handlers

func handleTunnelConnection(agent *Agent, tunnel *Tunnel, conn net.Conn) {

	tunnelConnection := &TunnelConnection{
		channelId: int(rand.Uint32()),
		conn:      conn,
		protocol:  "TCP",
	}
	tunnelConnection.ctx, tunnelConnection.handleCancel = context.WithCancel(context.Background())

	var taskData adaptix.TaskData
	switch tunnel.Type {

	case TUNNEL_SOCKS4:
		targetAddress, targetPort, err := proxy.CheckSocks4(conn)
		if err != nil {
			fmt.Println("Socks4 proxy error: ", err)
			return
		}
		taskData = tunnel.handlerConnectTCP(tunnelConnection.channelId, targetAddress, targetPort)

	case TUNNEL_SOCKS5:
		targetAddress, targetPort, socksCommand, err := proxy.CheckSocks5(conn)
		if err != nil {
			fmt.Println("Socks5 proxy error: ", err)
			return
		}
		if socksCommand == 3 {
			taskData = tunnel.handlerConnectUDP(tunnelConnection.channelId, targetAddress, targetPort)
			tunnelConnection.protocol = "UDP"
		} else {
			taskData = tunnel.handlerConnectTCP(tunnelConnection.channelId, targetAddress, targetPort)
		}

	case TUNNEL_SOCKS5_AUTH:
		targetAddress, targetPort, socksCommand, err := proxy.CheckSocks5Auth(conn, tunnel.Data.AuthUser, tunnel.Data.AuthPass)
		if err != nil {
			fmt.Println("Socks5 proxy error: ", err)
			return
		}
		if socksCommand == 3 {
			taskData = tunnel.handlerConnectUDP(tunnelConnection.channelId, targetAddress, targetPort)
			tunnelConnection.protocol = "UDP"
		} else {
			taskData = tunnel.handlerConnectTCP(tunnelConnection.channelId, targetAddress, targetPort)
		}

	case TUNNEL_LPORTFWD:
		tport, _ := strconv.Atoi(tunnel.Data.Fport)
		taskData = tunnel.handlerConnectTCP(tunnelConnection.channelId, tunnel.Data.Fhost, tport)

	default:
		return
	}

	sendTunnelTaskData(agent, taskData)
	tunnel.connections.Put(strconv.Itoa(tunnelConnection.channelId), tunnelConnection)
}

func handlerReverseAccept(agent *Agent, tunnel *Tunnel, channelId int) {
	target := tunnel.Data.Fhost + ":" + tunnel.Data.Fport
	fwdConn, err := net.Dial("tcp", target)
	if err != nil {
		rawTaskData := tunnel.handlerClose(channelId)
		sendTunnelTaskData(agent, rawTaskData)
		return
	}

	tunnelConnection := &TunnelConnection{
		channelId: channelId,
		conn:      fwdConn,
		protocol:  "TCP",
	}
	tunnelConnection.ctx, tunnelConnection.handleCancel = context.WithCancel(context.Background())

	tunnel.connections.Put(strconv.Itoa(channelId), tunnelConnection)

	go socketToTunnelData(agent, tunnel, tunnelConnection)
}

/// process socket

func sendTunnelTaskData(agent *Agent, taskData adaptix.TaskData) {
	if taskData.TaskId == "" {
		taskData.TaskId, _ = krypt.GenerateUID(8)
	}
	taskData.AgentId = agent.Data.Id
	agent.TunnelQueue.Put(taskData)
}

func socketToTunnelData(agent *Agent, tunnel *Tunnel, tunnelConnection *TunnelConnection) {
	var taskData adaptix.TaskData

	buffer := make([]byte, 0x10000)
	for {
		select {
		case <-tunnelConnection.ctx.Done():
			return
		default:
			n, err := tunnelConnection.conn.Read(buffer)
			if err != nil {
				if err == io.EOF {
					taskData = tunnel.handlerClose(tunnelConnection.channelId)
				} else {
					fmt.Printf("Error read data: %v\n", err)
					continue
				}
				break
			} else {
				if tunnelConnection.protocol == "UDP" {
					taskData = tunnel.handlerWriteUDP(tunnelConnection.channelId, buffer[:n])
				} else {
					taskData = tunnel.handlerWriteTCP(tunnelConnection.channelId, buffer[:n])
				}
			}

			sendTunnelTaskData(agent, taskData)
		}
	}
}

func tunnelDataToSocket(tunnelConnection *TunnelConnection, data []byte) {
	_, _ = tunnelConnection.conn.Write(data)
}
