package advisor

import "time"

type AdvisorConfig struct {
	Enabled      bool   `yaml:"enabled"`
	Provider     string `yaml:"provider"`
	ApiKey       string `yaml:"api_key"`
	Model        string `yaml:"model"`
	Scope        string `yaml:"scope"`
	Endpoint     string `yaml:"endpoint"`
	MaxHistory   int    `yaml:"max_history"`
	RedactCreds  bool   `yaml:"redact_creds"`
	MaxOutputLen int    `yaml:"max_output_len"`
}

type HistoryEntry struct {
	Timestamp   time.Time
	CommandLine string
	Output      string
	Success     bool
}

type AgentContext struct {
	AgentID    string
	Hostname   string
	Username   string
	Domain     string
	OS         string
	OsDesc     string
	Arch       string
	Elevated   bool
	PID        string
	Process    string
	InternalIP string
	ExternalIP string
}

type Recommendation struct {
	AgentID      string   `json:"agent_id"`
	Command      string   `json:"command"`
	Rationale    string   `json:"rationale"`
	LookFor      string   `json:"look_for"`
	Priority     string   `json:"priority"`
	Category     string   `json:"category"`
	OpsecNote    string   `json:"opsec_note"`
	Alternatives []string `json:"alternatives"`
}

type SyncPacketAdvisor struct {
	SpType int `json:"type"`

	AgentID      string   `json:"adv_agent_id"`
	Command      string   `json:"adv_command"`
	Rationale    string   `json:"adv_rationale"`
	LookFor      string   `json:"adv_look_for"`
	Priority     string   `json:"adv_priority"`
	Category     string   `json:"adv_category"`
	OpsecNote    string   `json:"adv_opsec_note"`
	Alternatives []string `json:"adv_alternatives"`
	Date         int64    `json:"adv_date"`
}
