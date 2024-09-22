package main

import (
	"crypto/rand"
	"crypto/rsa"
	"crypto/x509"
	"encoding/pem"
	"errors"
	"fmt"
	"github.com/gin-gonic/gin"
	"math/big"
	"net/http"
	"net/url"
	"os"
	"time"
)

type HTTPConfig struct {
	Ssl  bool   `json:"ssl"`
	Host string `json:"host"`
	Port string `json:"port"`
	Uri  string `json:"uri"`

	SslCert     string `json:"ssl_cert"`
	SslKey      string `json:"ssl_key"`
	SslCertPath string `json:"ssl_cert_path"`
	SslKeyPath  string `json:"ssl_key_path"`
}

type HTTP struct {
	Config    HTTPConfig
	GinEngine *gin.Engine
	Server    *http.Server

	Active bool
}

func NewConfigHttp() *HTTP {
	return &HTTP{
		GinEngine: gin.New(),
	}
}

func (h *HTTP) Start() {
	fmt.Println("Setup HTTP/S Server...")

	gin.SetMode(gin.ReleaseMode)
	router := gin.Default()
	router.NoRoute(h.pageFake)
	router.POST("/*endpoint", h.processRequest)
	h.Active = true

	h.Server = &http.Server{
		Addr:    fmt.Sprintf("%s:%s", h.Config.Host, h.Config.Port),
		Handler: router,
	}

	if h.Config.Ssl {
		fmt.Println("Started listener: https://" + h.Config.Host + ":" + h.Config.Port)

		/// TODO: Generate cert path
		h.Config.SslCertPath = "listener.crt"
		h.Config.SslKeyPath = "listener.key"

		if h.Config.SslCert == "" || h.Config.SslKey == "" {
			err := h.generateSelfSignedCert(h.Config.SslCertPath, h.Config.SslKeyPath)
			if err != nil {
				h.Active = false
				fmt.Println("Error generating self-signed certificate:", err)
				return
			}
		} else {
			os.WriteFile(h.Config.SslCertPath, []byte(h.Config.SslCert), 0600)
			os.WriteFile(h.Config.SslKeyPath, []byte(h.Config.SslKey), 0600)
		}

		go func() {
			err := h.Server.ListenAndServeTLS(h.Config.SslCertPath, h.Config.SslKeyPath)
			if err != nil && !errors.Is(err, http.ErrServerClosed) {
				fmt.Printf("Error starting HTTP server: %v\n", err)
				return
			}
			h.Active = true
		}()

	} else {
		fmt.Println("Started listener: http://" + h.Config.Host + ":" + h.Config.Port)

		go func() {
			err := h.Server.ListenAndServe()
			if err != nil && !errors.Is(err, http.ErrServerClosed) {
				fmt.Printf("Error starting HTTP server: %v\n", err)
				return
			}
			h.Active = true
		}()
	}
}

func (h *HTTP) Stop() error {
	return nil
}

func (h *HTTP) processRequest(ctx *gin.Context) {

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

	ctx.AbortWithStatus(http.StatusOK)
	return
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

	template.DNSNames = []string{h.Config.Host}

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
