package proxy

import (
	"bytes"
	"encoding/binary"
	"errors"
	"io"
	"net"

	"github.com/gorilla/websocket"
)

const (
	TUNNEL_TYPE_SOCKS4     = 1
	TUNNEL_TYPE_SOCKS5     = 2
	TUNNEL_TYPE_LOCAL_PORT = 4
	TUNNEL_TYPE_REVERSE    = 5

	ADDRESS_TYPE_IPV4   = 1
	ADDRESS_TYPE_DOMAIN = 3
	ADDRESS_TYPE_IPV6   = 4

	SOCKS5_SUCCESS                 byte = 0
	SOCKS5_SERVER_FAILURE          byte = 1
	SOCKS5_NOT_ALLOWED_RULESET     byte = 2
	SOCKS5_NETWORK_UNREACHABLE     byte = 3
	SOCKS5_HOST_UNREACHABLE        byte = 4
	SOCKS5_CONNECTION_REFUSED      byte = 5
	SOCKS5_TTL_EXPIRED             byte = 6
	SOCKS5_COMMAND_NOT_SUPPORTED   byte = 7
	SOCKS5_ADDR_TYPE_NOT_SUPPORTED byte = 8
)

type ProxySocks struct {
	SocksType    int
	SocksCommand int
	AddressType  int
	Address      string
	Port         int
}

func DetectAddrType(s string) int {
	ip := net.ParseIP(s)
	if ip != nil {
		if ip.To4() != nil {
			return ADDRESS_TYPE_IPV4
		}
		return ADDRESS_TYPE_IPV6
	}
	return ADDRESS_TYPE_DOMAIN
}

func ReplySocks4StatusConn(conn net.Conn, success bool) {
	if success {
		_, _ = conn.Write([]byte{0x00, 0x5a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
	} else {
		_, _ = conn.Write([]byte{0x00, 0x5b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
	}
}

func ReplySocks4StatusWs(wsconn *websocket.Conn, success bool) {
	if success {
		_ = wsconn.WriteMessage(websocket.BinaryMessage, []byte{0x00, 0x5a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
	} else {
		_ = wsconn.WriteMessage(websocket.BinaryMessage, []byte{0x00, 0x5b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
	}
}

func ReplySocks5StatusConn(conn net.Conn, rep byte) {
	_, _ = conn.Write([]byte{0x05, rep, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
}

func ReplySocks5StatusWs(wsconn *websocket.Conn, rep byte) {
	_ = wsconn.WriteMessage(websocket.BinaryMessage, []byte{0x05, rep, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00})
}

func CheckSocks4(conn net.Conn) (ProxySocks, error) {

	proxySocks := ProxySocks{
		SocksType:   TUNNEL_TYPE_SOCKS4,
		AddressType: ADDRESS_TYPE_IPV4,
	}

	buf := make([]byte, 8)
	_, err := io.ReadFull(conn, buf)
	if err != nil || buf[0] != 0x04 || buf[1] != 0x01 { /// VER=4, CMD=1 (CONNECT)
		ReplySocks4StatusConn(conn, false)
		return proxySocks, errors.New("invalid version of socks proxy")
	}

	proxySocks.Port = int(binary.BigEndian.Uint16(buf[2:4]))
	proxySocks.Address = net.IP(buf[4:8]).String()

	return proxySocks, err
}

func CheckSocks5(conn net.Conn, auth bool, username string, password string) (ProxySocks, error) {

	recvError := func(conn net.Conn, err string, data []byte) error {
		_, _ = conn.Write(data)
		return errors.New(err)
	}

	proxySocks := ProxySocks{
		SocksCommand: 1,
		SocksType:    TUNNEL_TYPE_SOCKS5,
	}

	buf := make([]byte, 2)
	_, err := io.ReadFull(conn, buf)
	if err != nil {
		_, _ = conn.Write([]byte{0x00, 0x01, 0x00})
		return proxySocks, err
	}
	socksVersion := buf[0]
	socksAuthCount := buf[1]

	/// Check version and auth methods

	if socksVersion != 0x05 {
		return proxySocks, recvError(conn, "invalid version of socks proxy", []byte{0x00, 0x01, 0x00})
	}

	buf = make([]byte, socksAuthCount) // auth methods
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		return proxySocks, recvError(conn, "no supported authentication method", []byte{0x05, 0xFF, 0x00})
	}

	/// Check Auth

	if auth {
		if bytes.IndexByte(buf, 0x02) == -1 {
			return proxySocks, recvError(conn, "no supported authentication method", []byte{0x05, 0xFF, 0x00})
		}
		_, _ = conn.Write([]byte{0x05, 0x02}) // version 5, auth user:pass

		/// get username

		buf = make([]byte, 2)
		_, err = io.ReadFull(conn, buf)
		if err != nil || buf[0] != 0x01 {
			return proxySocks, recvError(conn, "authentication failed", []byte{0x01, 0x01})
		}
		usernameLen := int(buf[1])
		buf = make([]byte, usernameLen)
		_, err = io.ReadFull(conn, buf)
		if err != nil {
			return proxySocks, recvError(conn, "authentication failed", []byte{0x01, 0x01})
		}
		reqUsername := string(buf)

		/// get password

		buf = make([]byte, 1) // password
		_, err = io.ReadFull(conn, buf)
		if err != nil {
			return proxySocks, recvError(conn, "authentication failed", []byte{0x01, 0x01})
		}
		passwordLen := int(buf[0])
		buf = make([]byte, passwordLen)
		_, err = io.ReadFull(conn, buf)
		if err != nil {
			return proxySocks, recvError(conn, "authentication failed", []byte{0x01, 0x01})
		}
		reqPassword := string(buf)

		/// check creds

		if reqUsername != username || reqPassword != password {
			return proxySocks, recvError(conn, "authentication failed", []byte{0x01, 0x01})
		}

		_, _ = conn.Write([]byte{0x01, 0x00}) // auth success

	} else {

		if bytes.IndexByte(buf, 0x00) == -1 {
			return proxySocks, recvError(conn, "no supported authentication method", []byte{0x05, 0xFF, 0x00})
		}

		_, _ = conn.Write([]byte{0x05, 0x00}) // version 5, without auth
	}

	/// Check command: CONNECT (1)

	buf = make([]byte, 4)
	_, err = io.ReadFull(conn, buf)
	if err != nil || buf[0] != 0x05 || buf[1] != 0x01 {
		ReplySocks5StatusConn(conn, SOCKS5_COMMAND_NOT_SUPPORTED)
		err = errors.New("no supported command")
		return proxySocks, err
	}

	//errByte = SOCKS5_ADDR_TYPE_NOT_SUPPORTED

	/// Check address

	addressType := buf[3]
	switch addressType {

	case ADDRESS_TYPE_IPV4:
		ipBuffer := make([]byte, 4)
		_, err = io.ReadFull(conn, ipBuffer)
		if err != nil {
			ReplySocks5StatusConn(conn, SOCKS5_COMMAND_NOT_SUPPORTED)
			return proxySocks, err
		}
		proxySocks.Address = net.IP(ipBuffer).String()

	case ADDRESS_TYPE_DOMAIN:
		domainLen := make([]byte, 1)
		_, err = io.ReadFull(conn, domainLen)
		if err != nil {
			ReplySocks5StatusConn(conn, SOCKS5_COMMAND_NOT_SUPPORTED)
			return proxySocks, err
		}
		domain := make([]byte, domainLen[0])
		_, err = io.ReadFull(conn, domain)
		if err != nil {
			ReplySocks5StatusConn(conn, SOCKS5_COMMAND_NOT_SUPPORTED)
			return proxySocks, err
		}
		proxySocks.Address = string(domain)

	case ADDRESS_TYPE_IPV6:
		ipBuffer := make([]byte, 16)
		_, err = io.ReadFull(conn, ipBuffer)
		if err != nil {
			ReplySocks5StatusConn(conn, SOCKS5_COMMAND_NOT_SUPPORTED)
			return proxySocks, err
		}
		proxySocks.Address = net.IP(ipBuffer).String()

	default:
		err = errors.New("unsupported address format")
		ReplySocks5StatusConn(conn, SOCKS5_COMMAND_NOT_SUPPORTED)
		return proxySocks, err
	}

	portBuf := make([]byte, 2)
	_, err = io.ReadFull(conn, portBuf)
	if err != nil {
		ReplySocks5StatusConn(conn, SOCKS5_COMMAND_NOT_SUPPORTED)
		return proxySocks, err
	}
	proxySocks.Port = int(binary.BigEndian.Uint16(portBuf))

	return proxySocks, err
}
