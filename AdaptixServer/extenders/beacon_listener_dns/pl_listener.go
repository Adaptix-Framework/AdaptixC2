package main

import (
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"strconv"
	"strings"

	"github.com/Adaptix-Framework/axc2"
)

// =============================================================================
// Configuration
// =============================================================================

type DNSConfig struct {
	HostBind   string   `json:"host_bind"`
	PortBind   int      `json:"port_bind"`
	Domain     string   `json:"domain"`
	Domains    []string `json:"-"`
	PktSize    int      `json:"pkt_size"`
	TTL        int      `json:"ttl"`
	EncryptKey string   `json:"encrypt_key"`
	Protocol   string   `json:"protocol"`
}

// =============================================================================
// Constructors
// =============================================================================

func NewDNSListener(name string, conf DNSConfig) *DNSListener {
	listener := new(DNSListener)
	listener.Name = name
	listener.Config = conf
	return listener
}

func buildListenerData(listener *DNSListener) adaptix.ListenerData {
	data := adaptix.ListenerData{
		BindHost:  listener.Config.HostBind,
		BindPort:  strconv.Itoa(listener.Config.PortBind),
		AgentAddr: fmt.Sprintf("%s:%d", listener.Config.HostBind, listener.Config.PortBind),
		Protocol:  "dns",
		Status:    "Listen",
	}
	if !listener.Active {
		data.Status = "Closed"
	}
	return data
}

func encodeConfig(conf DNSConfig) []byte {
	var buf bytes.Buffer
	_ = json.NewEncoder(&buf).Encode(conf)
	return buf.Bytes()
}

// =============================================================================
// Utility Functions
// =============================================================================

func parseDomains(domain string) []string {
	var domains []string
	for _, d := range strings.Split(domain, ",") {
		d = strings.TrimSpace(d)
		d = strings.ToLower(d)
		d = strings.TrimSuffix(d, ".")
		if d != "" {
			domains = append(domains, d)
		}
	}
	return domains
}

func isHex32(s string) bool {
	if len(s) != 32 {
		return false
	}
	for i := 0; i < 32; i++ {
		c := s[i]
		if !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) {
			return false
		}
	}
	return true
}

func normalizeEncryptKey(key string) string {
	if len(key) == 32 && isHex32(key) {
		return key
	}
	hash := sha256.Sum256([]byte(key))
	return hex.EncodeToString(hash[:16])
}

// =============================================================================
// Handler Methods
// =============================================================================

func (m *ModuleExtender) HandlerListenerValid(data string) error {
	conf := new(DNSConfig)
	if err := json.Unmarshal([]byte(data), conf); err != nil {
		return err
	}

	if conf.HostBind == "" {
		return errors.New("host_bind is required")
	}
	if conf.PortBind < 1 || conf.PortBind > 65535 {
		return errors.New("port_bind must be 1-65535")
	}
	if conf.Domain == "" {
		return errors.New("domain is required")
	}

	keyLen := len(conf.EncryptKey)
	if keyLen < 6 || keyLen > 32 {
		return errors.New("encrypt_key must be 6-32 characters")
	}

	return nil
}

func (m *ModuleExtender) HandlerCreateListenerDataAndStart(name string, configData string, listenerCustomData []byte) (adaptix.ListenerData, []byte, any, error) {
	var listenerData adaptix.ListenerData
	var customData []byte

	conf := new(DNSConfig)

	if listenerCustomData == nil {
		if err := json.Unmarshal([]byte(configData), conf); err != nil {
			return listenerData, customData, nil, err
		}
		conf.EncryptKey = normalizeEncryptKey(conf.EncryptKey)
	} else {
		if err := json.Unmarshal(listenerCustomData, conf); err != nil {
			return listenerData, customData, nil, err
		}
	}

	if conf.Protocol == "" {
		conf.Protocol = "dns"
	}
	conf.Domains = parseDomains(conf.Domain)

	listener := NewDNSListener(name, *conf)
	if err := listener.Start(m.ts); err != nil {
		return listenerData, customData, nil, err
	}

	listenerData = buildListenerData(listener)
	customData = encodeConfig(listener.Config)

	return listenerData, customData, listener, nil
}

func (m *ModuleExtender) HandlerEditListenerData(name string, listenerObject any, configData string) (adaptix.ListenerData, []byte, bool) {
	listener, ok := listenerObject.(*DNSListener)
	if !ok || listener.Name != name {
		return adaptix.ListenerData{}, nil, false
	}

	conf := new(DNSConfig)
	if err := json.Unmarshal([]byte(configData), conf); err != nil {
		return adaptix.ListenerData{}, nil, false
	}

	if conf.Domain != "" {
		listener.Config.Domain = conf.Domain
		listener.Config.Domains = parseDomains(conf.Domain)
	}
	if conf.TTL != 0 {
		listener.Config.TTL = conf.TTL
	}
	if conf.PktSize != 0 {
		listener.Config.PktSize = conf.PktSize
	}

	listenerData := buildListenerData(listener)
	customData := encodeConfig(listener.Config)

	return listenerData, customData, true
}

func (m *ModuleExtender) HandlerListenerStop(name string, listenerObject any) (bool, error) {
	listener, ok := listenerObject.(*DNSListener)
	if !ok || listener.Name != name {
		return false, nil
	}
	return true, listener.Stop()
}

func (m *ModuleExtender) HandlerListenerGetProfile(name string, listenerObject any) ([]byte, bool) {
	listener, ok := listenerObject.(*DNSListener)
	if !ok || listener.Name != name {
		return nil, false
	}

	if listener.Config.Protocol == "" {
		listener.Config.Protocol = "dns"
	}

	return encodeConfig(listener.Config), true
}
