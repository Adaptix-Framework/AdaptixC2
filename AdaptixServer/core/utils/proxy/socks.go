package proxy

import (
	"encoding/binary"
	"errors"
	"io"
	"net"
)

func CheckSocks4(conn net.Conn) (string, int, error) {
	var (
		buf     []byte
		err     error
		address string
		port    int
	)

	buf = make([]byte, 2)
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}
	if buf[0] != 0x04 {
		err = errors.New("invalid version of socks proxy")
		goto RET
	}

	if buf[1] != 0x01 { // CONNECT command
		err = errors.New("invalid command code")
		goto RET
	}

	buf = make([]byte, 2) // port
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}
	port = int(binary.BigEndian.Uint16(buf))

	buf = make([]byte, 4) // IPv4
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}
	address = net.IP(buf).String()

RET:
	if err != nil {
		_, _ = conn.Write([]byte{0x00, 0x5b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}) // failed
	} else {
		_, _ = conn.Write([]byte{0x00, 0x5a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}) // success
	}

	return address, port, err
}

func CheckSocks5(conn net.Conn) (string, int, int, error) {
	var (
		buf     []byte
		err     error
		address string
		port    int
		command int
	)

	buf = make([]byte, 3)
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}
	if buf[0] != 0x05 {
		err = errors.New("invalid version of socks proxy")
		goto RET
	}
	if buf[1] != 0x01 && buf[2] != 0x00 {
		err = errors.New("invalid version of socks proxy")
		goto RET
	}
	_, _ = conn.Write([]byte{0x05, 0x00}) // version 5, without auth

	buf = make([]byte, 4)
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}

	command = int(buf[1])

	switch buf[3] {

	case 0x01: // IPv4
		ipBuffer := make([]byte, 4)
		_, err = io.ReadFull(conn, ipBuffer)
		if err != nil {
			goto RET
		}
		portBuf := make([]byte, 2)
		_, err := io.ReadFull(conn, portBuf)
		if err != nil {
			goto RET
		}
		port = int(binary.BigEndian.Uint16(portBuf))
		address = net.IP(ipBuffer).String()

		break

	case 0x03: // dns
		domainLen := make([]byte, 1)
		_, err = io.ReadFull(conn, domainLen)
		if err != nil {
			goto RET
		}
		domain := make([]byte, domainLen[0])
		_, err = io.ReadFull(conn, domain)
		if err != nil {
			goto RET
		}
		portBuf := make([]byte, 2)
		_, err = io.ReadFull(conn, portBuf)
		if err != nil {
			goto RET
		}
		port = int(binary.BigEndian.Uint16(portBuf))
		address = string(domain)

		break

	//case 0x04: // IPv6
	//	ipBuffer := make([]byte, 16)
	//	_, err = io.ReadFull(conn, ipBuffer)
	//	if err != nil {
	//		goto RET
	//	}
	//	portBuf := make([]byte, 2)
	//	_, err = io.ReadFull(conn, portBuf)
	//	if err != nil {
	//		goto RET
	//	}
	//	port = int(binary.BigEndian.Uint16(portBuf))
	//	address = net.IP(ipBuffer).String()
	//
	//	break

	default:
		err = errors.New("unsupported address format")
		goto RET
	}

RET:
	if err != nil {
		_, _ = conn.Write([]byte{0x05, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}) // failed
	} else {
		_, _ = conn.Write([]byte{0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}) // success
	}

	return address, port, command, err
}

func CheckSocks5Auth(conn net.Conn, username string, password string) (string, int, int, error) {
	var (
		buf          []byte
		err          error
		address      string
		port         int
		command      int
		size         int
		authRequired bool
		reqUsername  string
		reqPassword  string
	)

	buf = make([]byte, 2)
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}
	if buf[0] != 0x05 {
		err = errors.New("invalid version of socks proxy")
		goto RET
	}

	size = int(buf[1])
	buf = make([]byte, size) // auth methods
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}
	authRequired = false
	for _, method := range buf {
		if method == 0x02 {
			authRequired = true
			break
		}
	}
	if !authRequired {
		_, _ = conn.Write([]byte{0x05, 0xFF}) // failed method
		err = errors.New("no supported authentication method")
		goto RET
	}
	_, _ = conn.Write([]byte{0x05, 0x02}) // success method

	buf = make([]byte, 2) // username
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}
	if buf[0] != 0x01 {
		err = errors.New("invalid auth request")
		goto RET
	}
	size = int(buf[1])
	buf = make([]byte, size)
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}
	reqUsername = string(buf)

	buf = make([]byte, 1) // password
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}
	size = int(buf[0])
	buf = make([]byte, size)
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}
	reqPassword = string(buf)

	if reqUsername != username || reqPassword != password {
		_, _ = conn.Write([]byte{0x01, 0x01}) // auth failed
		err = errors.New("authentication failed")
		goto RET
	}
	_, _ = conn.Write([]byte{0x01, 0x00}) // auth success

	buf = make([]byte, 4)
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}

	command = int(buf[1])

	switch buf[3] {

	case 0x01: // IPv4
		ipBuffer := make([]byte, 4)
		_, err = io.ReadFull(conn, ipBuffer)
		if err != nil {
			goto RET
		}
		portBuf := make([]byte, 2)
		_, err := io.ReadFull(conn, portBuf)
		if err != nil {
			goto RET
		}
		port = int(binary.BigEndian.Uint16(portBuf))
		address = net.IP(ipBuffer).String()

		break

	case 0x03: // dns
		domainLen := make([]byte, 1)
		_, err = io.ReadFull(conn, domainLen)
		if err != nil {
			goto RET
		}
		domain := make([]byte, domainLen[0])
		_, err = io.ReadFull(conn, domain)
		if err != nil {
			goto RET
		}
		portBuf := make([]byte, 2)
		_, err = io.ReadFull(conn, portBuf)
		if err != nil {
			goto RET
		}
		port = int(binary.BigEndian.Uint16(portBuf))
		address = string(domain)

		break

		//case 0x04: // IPv6
		//	ipBuffer := make([]byte, 16)
		//	_, err = io.ReadFull(conn, ipBuffer)
		//	if err != nil {
		//		goto RET
		//	}
		//	portBuf := make([]byte, 2)
		//	_, err = io.ReadFull(conn, portBuf)
		//	if err != nil {
		//		goto RET
		//	}
		//	port = int(binary.BigEndian.Uint16(portBuf))
		//	address = net.IP(ipBuffer).String()
		//
		//	break

	default:
		err = errors.New("unsupported address format")
		goto RET
	}

RET:
	if err != nil {
		_, _ = conn.Write([]byte{0x05, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}) // failed
	} else {
		_, _ = conn.Write([]byte{0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}) // success
	}

	return address, port, command, err
}
