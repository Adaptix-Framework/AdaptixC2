package proxy

import (
	"encoding/binary"
	"errors"
	"io"
	"net"
)

func CheckSocks5(conn net.Conn) (string, int, error) {
	var (
		buf     []byte
		err     error
		address string
		port    int
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
	conn.Write([]byte{0x05, 0x00}) // version 5, without auth

	buf = make([]byte, 4)
	_, err = io.ReadFull(conn, buf)
	if err != nil {
		goto RET
	}
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

	default:
		err = errors.New("unsupported address format")
		goto RET
	}

RET:
	if err != nil {
		conn.Write([]byte{0x05, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}) // failed
	} else {
		conn.Write([]byte{0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}) // success
	}

	return address, port, err
}
