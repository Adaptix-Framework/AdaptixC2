package main

type TCPConfig struct {
	Port    int    `json:"port_bind"`
	Prepend string `json:"prepend"`

	Protocol   string `json:"protocol"`
	EncryptKey []byte `json:"encrypt_key"`
}

type TCP struct {
	Config TCPConfig
	Name   string
	Active bool
}
