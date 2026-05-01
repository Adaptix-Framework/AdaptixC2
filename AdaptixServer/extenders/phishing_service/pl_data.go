package main

import (
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"strings"
	"time"
)

// ============================================================================
// Data Models
// ============================================================================

type Campaign struct {
	ID          string `json:"id"`
	Name        string `json:"name"`
	Status      string `json:"status"` // draft, running, paused, completed
	SmtpHost    string `json:"smtp_host"`
	SmtpPort    int    `json:"smtp_port"`
	SmtpUser    string `json:"smtp_user"`
	SmtpPass    string `json:"smtp_pass"`
	SmtpTLS     bool   `json:"smtp_tls"`
	SenderEmail string `json:"sender_email"`
	SenderName  string `json:"sender_name"`
	Subject     string `json:"subject"`
	Template    string `json:"template"`
	Lander      string `json:"lander"`
	TrackOpens  bool   `json:"track_opens"`
	TrackClicks bool   `json:"track_clicks"`
	BaseURL     string `json:"base_url"`
	RedirectURL string `json:"redirect_url"`
	SendDelay   int    `json:"send_delay"` // seconds between emails
	CreatedAt   int64  `json:"created_at"`
	CreatedBy   string `json:"created_by"`
}

type Target struct {
	ID         string `json:"id"`
	CampaignID string `json:"campaign_id"`
	Email      string `json:"email"`
	FirstName  string `json:"first_name"`
	LastName   string `json:"last_name"`
	Position   string `json:"position"`
	Company    string `json:"company"`
	Custom     string `json:"custom"`
}

type Result struct {
	ID         string `json:"id"`
	CampaignID string `json:"campaign_id"`
	TargetID   string `json:"target_id"`
	Email      string `json:"email"`
	FirstName  string `json:"first_name"`
	LastName   string `json:"last_name"`
	Status     string `json:"status"` // sent, delivered, opened, clicked, submitted, error
	SentAt     int64  `json:"sent_at"`
	OpenedAt   int64  `json:"opened_at"`
	ClickedAt  int64  `json:"clicked_at"`
	SubmitAt   int64  `json:"submit_at"`
	UserAgent  string `json:"user_agent"`
	RemoteIP   string `json:"remote_ip"`
	SubmitData string `json:"submit_data"`
	Error      string `json:"error"`
}

// ============================================================================
// ID Generation
// ============================================================================

func generateID() string {
	b := make([]byte, 16)
	rand.Read(b)
	return hex.EncodeToString(b)
}

func generateTrackingID() string {
	b := make([]byte, 12)
	rand.Read(b)
	return hex.EncodeToString(b)
}

// ============================================================================
// Campaign CRUD
// ============================================================================

func (s *PhishingService) SaveCampaign(c Campaign) error {
	data, err := json.Marshal(c)
	if err != nil {
		return err
	}
	return s.ts.TsExtenderDataSave(ExtenderName, "campaign:"+c.ID, data)
}

func (s *PhishingService) LoadCampaign(id string) (Campaign, error) {
	var c Campaign
	data, err := s.ts.TsExtenderDataLoad(ExtenderName, "campaign:"+id)
	if err != nil {
		return c, err
	}
	err = json.Unmarshal(data, &c)
	return c, err
}

func (s *PhishingService) DeleteCampaign(id string) error {
	_ = s.ts.TsExtenderDataDelete(ExtenderName, "campaign:"+id)
	_ = s.ts.TsExtenderDataDelete(ExtenderName, "targets:"+id)
	_ = s.ts.TsExtenderDataDelete(ExtenderName, "results:"+id)
	return nil
}

func (s *PhishingService) ListCampaigns() []Campaign {
	var campaigns []Campaign
	keys, err := s.ts.TsExtenderDataKeys(ExtenderName)
	if err != nil {
		return campaigns
	}

	for _, key := range keys {
		if strings.HasPrefix(key, "campaign:") {
			data, err := s.ts.TsExtenderDataLoad(ExtenderName, key)
			if err != nil {
				continue
			}
			var c Campaign
			if json.Unmarshal(data, &c) == nil {
				campaigns = append(campaigns, c)
			}
		}
	}
	return campaigns
}

// ============================================================================
// Target CRUD
// ============================================================================

func (s *PhishingService) SaveTargets(campaignID string, targets []Target) error {
	data, err := json.Marshal(targets)
	if err != nil {
		return err
	}
	return s.ts.TsExtenderDataSave(ExtenderName, "targets:"+campaignID, data)
}

func (s *PhishingService) LoadTargets(campaignID string) []Target {
	var targets []Target
	data, err := s.ts.TsExtenderDataLoad(ExtenderName, "targets:"+campaignID)
	if err != nil {
		return targets
	}
	json.Unmarshal(data, &targets)
	return targets
}

// ============================================================================
// Result CRUD
// ============================================================================

func (s *PhishingService) SaveResults(campaignID string, results []Result) error {
	data, err := json.Marshal(results)
	if err != nil {
		return err
	}
	return s.ts.TsExtenderDataSave(ExtenderName, "results:"+campaignID, data)
}

func (s *PhishingService) LoadResults(campaignID string) []Result {
	var results []Result
	data, err := s.ts.TsExtenderDataLoad(ExtenderName, "results:"+campaignID)
	if err != nil {
		return results
	}
	json.Unmarshal(data, &results)
	return results
}

// LoadResultByTrackingID searches all campaign results for a specific tracking ID.
func (s *PhishingService) LoadResultByTrackingID(trackingID string) (*Result, string) {
	keys, err := s.ts.TsExtenderDataKeys(ExtenderName)
	if err != nil {
		return nil, ""
	}

	for _, key := range keys {
		if !strings.HasPrefix(key, "results:") {
			continue
		}
		campaignID := strings.TrimPrefix(key, "results:")
		results := s.LoadResults(campaignID)
		for i := range results {
			if results[i].ID == trackingID {
				return &results[i], campaignID
			}
		}
	}
	return nil, ""
}

// UpdateResult updates a specific result within its campaign's results.
func (s *PhishingService) UpdateResult(campaignID string, result *Result) error {
	results := s.LoadResults(campaignID)
	for i := range results {
		if results[i].ID == result.ID {
			results[i] = *result
			return s.SaveResults(campaignID, results)
		}
	}
	return fmt.Errorf("result not found")
}

// ============================================================================
// Campaign Stats
// ============================================================================

type CampaignStats struct {
	TotalTargets int `json:"total_targets"`
	Sent         int `json:"sent"`
	Opened       int `json:"opened"`
	Clicked      int `json:"clicked"`
	Submitted    int `json:"submitted"`
	Errors       int `json:"errors"`
}

func (s *PhishingService) GetCampaignStats(campaignID string) CampaignStats {
	results := s.LoadResults(campaignID)
	targets := s.LoadTargets(campaignID)
	stats := CampaignStats{
		TotalTargets: len(targets),
	}
	for _, r := range results {
		switch r.Status {
		case "error":
			stats.Errors++
		case "submitted":
			stats.Submitted++
			stats.Clicked++
			stats.Opened++
			stats.Sent++
		case "clicked":
			stats.Clicked++
			stats.Opened++
			stats.Sent++
		case "opened":
			stats.Opened++
			stats.Sent++
		case "sent":
			stats.Sent++
		}
	}
	return stats
}

// ============================================================================
// Helpers
// ============================================================================

func nowUnix() int64 {
	return time.Now().Unix()
}
