package main

type TCPConfig struct {
	Port       int    `json:"port_bind"`
	Prepend    string `json:"prepend_data"`
	EncryptKey string `json:"encrypt_key"`

	Protocol string `json:"protocol"`
}

type TCP struct {
	Config TCPConfig
	Name   string
	Active bool
}
