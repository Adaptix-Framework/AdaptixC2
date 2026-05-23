package main

import (
	"encoding/json"
	"net/http"
	"sync"

	adaptix "github.com/Adaptix-Framework/axc2"
)

type Teamserver interface {
	TsExtenderDataSave(extenderName string, key string, value []byte) error
	TsExtenderDataLoad(extenderName string, key string) ([]byte, error)
	TsExtenderDataDelete(extenderName string, key string) error
	TsExtenderDataKeys(extenderName string) ([]string, error)

	TsEndpointRegisterPublicRaw(method string, path string, handler func(w http.ResponseWriter, r *http.Request)) error
	TsEndpointUnregisterPublic(method string, path string) error
	TsEndpointExistsPublic(method string, path string) bool

	TsServiceSendDataAll(service string, data string)
	TsServiceSendDataClient(operator string, service string, data string)
}

const ServiceName = "Phishing"
const ExtenderName = "phishing_service"

type PhishingService struct {
	ts        Teamserver
	moduleDir string
	mu        sync.RWMutex
	stopChans map[string]chan struct{} // campaignID -> stop channel
}

var (
	Ts        Teamserver
	ModuleDir string
	Service   *PhishingService
)

func InitPlugin(ts any, moduleDir string, serviceConfig string) adaptix.PluginService {
	Ts = ts.(Teamserver)
	ModuleDir = moduleDir

	Service = &PhishingService{
		ts:        Ts,
		moduleDir: moduleDir,
		stopChans: make(map[string]chan struct{}),
	}

	Service.RegisterEndpoints()

	return Service
}

func (s *PhishingService) Call(operator string, function string, args string) {
	switch function {

	case "campaign_create":
		s.HandleCampaignCreate(operator, args)

	case "campaign_list":
		s.HandleCampaignList(operator)

	case "campaign_delete":
		s.HandleCampaignDelete(operator, args)

	case "campaign_start":
		s.HandleCampaignStart(operator, args)

	case "campaign_stop":
		s.HandleCampaignStop(operator, args)

	case "targets_import":
		s.HandleTargetsImport(operator, args)

	case "targets_list":
		s.HandleTargetsList(operator, args)

	case "targets_delete":
		s.HandleTargetsDelete(operator, args)

	case "results_list":
		s.HandleResultsList(operator, args)

	case "results_export":
		s.HandleResultsExport(operator, args)

	case "templates_list":
		s.HandleTemplatesList(operator)

	case "landers_list":
		s.HandleLandersList(operator)

	case "template_preview":
		s.HandleTemplatePreview(operator, args)
	}
}

// sendResponse sends a JSON response back to all connected clients via the service data channel.
func (s *PhishingService) sendResponseAll(msgType string, data interface{}) {
	resp := map[string]interface{}{
		"type": msgType,
		"data": data,
	}
	jsonData, err := json.Marshal(resp)
	if err != nil {
		return
	}
	s.ts.TsServiceSendDataAll(ServiceName, string(jsonData))
}

// sendResponseClient sends a JSON response to a specific operator.
func (s *PhishingService) sendResponseClient(operator string, msgType string, data interface{}) {
	resp := map[string]interface{}{
		"type": msgType,
		"data": data,
	}
	jsonData, err := json.Marshal(resp)
	if err != nil {
		return
	}
	s.ts.TsServiceSendDataClient(operator, ServiceName, string(jsonData))
}

// sendEvent sends a real-time event notification to all clients.
func (s *PhishingService) sendEvent(eventType string, data interface{}) {
	resp := map[string]interface{}{
		"type":  "event",
		"event": eventType,
		"data":  data,
	}
	jsonData, err := json.Marshal(resp)
	if err != nil {
		return
	}
	s.ts.TsServiceSendDataAll(ServiceName, string(jsonData))
}

// sendError sends an error message to a specific operator.
func (s *PhishingService) sendError(operator string, message string) {
	resp := map[string]interface{}{
		"type":    "error",
		"message": message,
	}
	jsonData, err := json.Marshal(resp)
	if err != nil {
		return
	}
	s.ts.TsServiceSendDataClient(operator, ServiceName, string(jsonData))
}
