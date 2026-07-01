package main

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"

	"github.com/goccy/go-yaml"
)

type Config struct {
	Host     string `yaml:"host"`
	Port     int    `yaml:"port"`
	Endpoint string `yaml:"endpoint"`
	Username string `yaml:"username"`
	Password string `yaml:"password"`
}

func configPath() (string, error) {
	home, err := os.UserHomeDir()
	if err != nil {
		return "", fmt.Errorf("cannot find home directory: %w", err)
	}
	dir := filepath.Join(home, ".adaptix")
	if err := os.MkdirAll(dir, 0700); err != nil {
		return "", fmt.Errorf("create config dir: %w", err)
	}
	return filepath.Join(dir, "adaptix-cil.yaml"), nil
}

func loadConfig() (*Config, error) {
	path, err := configPath()
	if err != nil {
		return nil, err
	}
	data, err := os.ReadFile(path)
	if err != nil {
		if errors.Is(err, os.ErrNotExist) {
			return nil, fmt.Errorf("no config found, run 'adaptix-cil config set' first")
		}
		return nil, fmt.Errorf("read config: %w", err)
	}
	var cfg Config
	if err := yaml.Unmarshal(data, &cfg); err != nil {
		return nil, fmt.Errorf("parse config: %w", err)
	}
	if cfg.Port == 0 {
		cfg.Port = 4321
	}
	if cfg.Endpoint == "" {
		cfg.Endpoint = "/endpoint"
	}
	return &cfg, nil
}

func saveConfig(cfg *Config) error {
	path, err := configPath()
	if err != nil {
		return err
	}
	if cfg.Port == 0 {
		cfg.Port = 4321
	}
	if cfg.Endpoint == "" {
		cfg.Endpoint = "/endpoint"
	}
	data, err := yaml.Marshal(cfg)
	if err != nil {
		return fmt.Errorf("marshal config: %w", err)
	}
	if err := os.WriteFile(path, data, 0600); err != nil {
		return fmt.Errorf("write config: %w", err)
	}
	return nil
}

func runConfigSet(args []string) {
	fs := newFlagSet("config set")
	host := fs.String("host", "", "server host")
	port := fs.Int("port", 0, "server port")
	endpoint := fs.String("endpoint", "", "server endpoint")
	username := fs.String("username", "", "operator username")
	password := fs.String("password", "", "operator password")
	_ = fs.Parse(args)

	cfg, _ := loadConfig()
	if cfg == nil {
		cfg = &Config{}
	}
	if *host != "" {
		cfg.Host = *host
	}
	if *port != 0 {
		cfg.Port = *port
	}
	if *endpoint != "" {
		cfg.Endpoint = *endpoint
	}
	if *username != "" {
		cfg.Username = *username
	}
	if *password != "" {
		cfg.Password = *password
	}

	if err := saveConfig(cfg); err != nil {
		fmt.Fprintf(os.Stderr, "error: %v\n", err)
		os.Exit(1)
	}
	fmt.Println("config saved")
}

func runConfigShow(jsonOut bool) {
	cfg, err := loadConfig()
	if err != nil {
		fmt.Fprintf(os.Stderr, "error: %v\n", err)
		os.Exit(1)
	}
	if jsonOut {
		printJSON(cfg)
	} else {
		fmt.Printf("host:      %s\n", cfg.Host)
		fmt.Printf("port:      %d\n", cfg.Port)
		fmt.Printf("endpoint:  %s\n", cfg.Endpoint)
		fmt.Printf("username:  %s\n", cfg.Username)
		fmt.Printf("password:  %s\n", maskPassword(cfg.Password))
	}
}

func maskPassword(p string) string {
	if len(p) <= 4 {
		return "****"
	}
	return p[:2] + "****" + p[len(p)-2:]
}
