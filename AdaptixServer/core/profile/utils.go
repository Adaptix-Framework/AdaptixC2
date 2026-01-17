package profile

type AdaptixProfile struct {
	Server         *TsProfile  `yaml:"Teamserver"`
	ServerResponse *TsResponse `yaml:"ServerResponse"`
	Callbacks      *TsCallback `yaml:"EventCallback"`
}

type TsProfile struct {
	Interface    string            `yaml:"interface"`
	Port         int               `yaml:"port"`
	Endpoint     string            `yaml:"endpoint"`
	Password     string            `yaml:"password"`
	OnlyPassword bool              `yaml:"only_password"`
	Operators    map[string]string `yaml:"operators"`
	Cert         string            `yaml:"cert"`
	Key          string            `yaml:"key"`
	Extenders    []string          `yaml:"extenders"`
	ATokenLive   int               `yaml:"access_token_live_hours"`
	RTokenLive   int               `yaml:"refresh_token_live_hours"`
}

type TsResponse struct {
	Status      int               `yaml:"status"`
	Headers     map[string]string `yaml:"headers"`
	PagePath    string            `yaml:"page"`
	PageContent string
}

type WebhookConfig struct {
	URL     string            `yaml:"url"`
	Method  string            `yaml:"method"`
	Headers map[string]string `yaml:"headers"`
	Data    string            `yaml:"data"`
}

type TsCallback struct {
	Telegram struct {
		Token   string   `yaml:"token"`
		ChatsId []string `yaml:"chats_id"`
	} `yaml:"Telegram"`

	Slack struct {
		WebhookURL string `yaml:"webhook_url"`
	} `yaml:"Slack"`

	Webhooks []WebhookConfig `yaml:"Webhooks"`

	NewAgentMessage    string `yaml:"new_agent_message"`
	NewCredMessage     string `yaml:"new_cred_message"`
	NewDownloadMessage string `yaml:"new_download_message"`
}
