package advisor

import (
	"context"
	"encoding/json"
	"fmt"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/Adaptix-Framework/axc2"
)

// --- mock LLM ---

type mockLLM struct {
	response string
	err      error
	mu       sync.Mutex
	calls    []string
}

func (m *mockLLM) Complete(ctx context.Context, systemPrompt string, userPrompt string) (string, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.calls = append(m.calls, userPrompt)
	return m.response, m.err
}

func (m *mockLLM) callCount() int {
	m.mu.Lock()
	defer m.mu.Unlock()
	return len(m.calls)
}

func (m *mockLLM) lastCall() string {
	m.mu.Lock()
	defer m.mu.Unlock()
	if len(m.calls) == 0 {
		return ""
	}
	return m.calls[len(m.calls)-1]
}

// --- mock teamserver ---

type mockTeamserver struct {
	mu      sync.Mutex
	agents  map[string]adaptix.AgentData
	packets []interface{}
}

func newMockTS() *mockTeamserver {
	return &mockTeamserver{
		agents:  make(map[string]adaptix.AgentData),
		packets: make([]interface{}, 0),
	}
}

func (m *mockTeamserver) GetAgent(agentId string) (adaptix.AgentData, bool) {
	m.mu.Lock()
	defer m.mu.Unlock()
	a, ok := m.agents[agentId]
	return a, ok
}

func (m *mockTeamserver) GetAllAgents() []adaptix.AgentData {
	m.mu.Lock()
	defer m.mu.Unlock()
	out := make([]adaptix.AgentData, 0, len(m.agents))
	for _, a := range m.agents {
		out = append(out, a)
	}
	return out
}

func (m *mockTeamserver) SyncAdvisor(packet interface{}) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.packets = append(m.packets, packet)
}

func (m *mockTeamserver) getPackets() []interface{} {
	m.mu.Lock()
	defer m.mu.Unlock()
	out := make([]interface{}, len(m.packets))
	copy(out, m.packets)
	return out
}

// --- tests ---

func TestParseRecommendation_Valid(t *testing.T) {
	input := `{"command":"whoami /all","rationale":"Check current privileges","look_for":"group memberships","priority":"high","category":"recon","alternatives":["net user %username%"]}`
	rec, err := parseRecommendation(input)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if rec.Command != "whoami /all" {
		t.Errorf("command = %q, want %q", rec.Command, "whoami /all")
	}
	if rec.Priority != "high" {
		t.Errorf("priority = %q, want %q", rec.Priority, "high")
	}
	if rec.Category != "recon" {
		t.Errorf("category = %q, want %q", rec.Category, "recon")
	}
	if len(rec.Alternatives) != 1 || rec.Alternatives[0] != "net user %username%" {
		t.Errorf("alternatives = %v, unexpected", rec.Alternatives)
	}
}

func TestParseRecommendation_WithMarkdown(t *testing.T) {
	input := "Here is my suggestion:\n```json\n{\"command\":\"ipconfig /all\",\"rationale\":\"enumerate NICs\",\"look_for\":\"subnets\",\"priority\":\"medium\",\"category\":\"recon\",\"alternatives\":[]}\n```"
	rec, err := parseRecommendation(input)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if rec.Command != "ipconfig /all" {
		t.Errorf("command = %q, want %q", rec.Command, "ipconfig /all")
	}
}

func TestParseRecommendation_EmptyCommand(t *testing.T) {
	input := `{"command":"","rationale":"nothing"}`
	_, err := parseRecommendation(input)
	if err == nil {
		t.Fatal("expected error for empty command, got nil")
	}
}

func TestParseRecommendation_InvalidJSON(t *testing.T) {
	_, err := parseRecommendation("not json at all")
	if err == nil {
		t.Fatal("expected error for invalid JSON, got nil")
	}
}

func TestParseRecommendation_DefaultPriority(t *testing.T) {
	input := `{"command":"net user","rationale":"list users","look_for":"admins","priority":"critical","category":"recon"}`
	rec, err := parseRecommendation(input)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if rec.Priority != "medium" {
		t.Errorf("expected default priority 'medium', got %q", rec.Priority)
	}
}

func TestParseRecommendation_DefaultCategory(t *testing.T) {
	input := `{"command":"net user","rationale":"list users","look_for":"admins","priority":"high","category":"unknown"}`
	rec, err := parseRecommendation(input)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if rec.Category != "recon" {
		t.Errorf("expected default category 'recon', got %q", rec.Category)
	}
}

func TestBuildSystemPrompt_WithScope(t *testing.T) {
	prompt := BuildSystemPrompt("10.0.0.0/8, CORP.LOCAL")
	if !strings.Contains(prompt, "10.0.0.0/8") {
		t.Error("scope not included in system prompt")
	}
	if !strings.Contains(prompt, "authorized penetration testing") {
		t.Error("authorization context missing from system prompt")
	}
}

func TestBuildSystemPrompt_NoScope(t *testing.T) {
	prompt := BuildSystemPrompt("")
	if strings.Contains(prompt, "scope:") {
		t.Error("scope clause should not appear when scope is empty")
	}
}

func TestBuildUserPrompt(t *testing.T) {
	ctx := AgentContext{
		Hostname:   "WORKSTATION1",
		Username:   "admin",
		Domain:     "CORP",
		OS:         "1",
		OsDesc:     "Windows 10 Pro",
		Arch:       "x64",
		Elevated:   true,
		PID:        "1234",
		Process:    "explorer.exe",
		InternalIP: "10.0.0.5",
		ExternalIP: "1.2.3.4",
	}

	history := []HistoryEntry{
		{Timestamp: time.Now(), CommandLine: "whoami", Output: "CORP\\admin", Success: true},
	}

	prompt := BuildUserPrompt(ctx, history, "net user", "admin\nGuest\nkrbtgt", 4000, "", "")

	checks := []string{
		"WORKSTATION1",
		"admin",
		"CORP",
		"Windows",
		"Elevated: Yes",
		"explorer.exe",
		"whoami",
		"net user",
		"krbtgt",
	}
	for _, check := range checks {
		if !strings.Contains(prompt, check) {
			t.Errorf("prompt missing expected content %q", check)
		}
	}
}

func TestBuildUserPrompt_TruncatesLongOutput(t *testing.T) {
	ctx := AgentContext{Hostname: "HOST"}
	longOutput := strings.Repeat("A", 5000)
	prompt := BuildUserPrompt(ctx, nil, "cmd", longOutput, 100, "", "")
	if !strings.Contains(prompt, "(truncated)") {
		t.Error("long output should be truncated")
	}
	if strings.Contains(prompt, strings.Repeat("A", 5000)) {
		t.Error("full long output should not be present")
	}
}

func TestBuildUserPrompt_EmptyOutput(t *testing.T) {
	ctx := AgentContext{Hostname: "HOST"}
	prompt := BuildUserPrompt(ctx, nil, "cmd", "", 4000, "", "")
	if !strings.Contains(prompt, "(empty output)") {
		t.Error("empty output should be indicated")
	}
}

func TestAgentState_HistoryRolling(t *testing.T) {
	state := NewAgentState(AgentContext{}, 3)
	for i := 0; i < 5; i++ {
		state.AddHistory(HistoryEntry{CommandLine: fmt.Sprintf("cmd%d", i)})
	}
	h := state.GetHistory()
	if len(h) != 3 {
		t.Fatalf("history len = %d, want 3", len(h))
	}
	if h[0].CommandLine != "cmd2" {
		t.Errorf("oldest entry = %q, want cmd2", h[0].CommandLine)
	}
	if h[2].CommandLine != "cmd4" {
		t.Errorf("newest entry = %q, want cmd4", h[2].CommandLine)
	}
}

func TestAgentState_Concurrency(t *testing.T) {
	state := NewAgentState(AgentContext{}, 100)
	var wg sync.WaitGroup
	for i := 0; i < 50; i++ {
		wg.Add(1)
		go func(n int) {
			defer wg.Done()
			state.AddHistory(HistoryEntry{CommandLine: fmt.Sprintf("cmd%d", n)})
			_ = state.GetHistory()
			_ = state.GetContext()
		}(i)
	}
	wg.Wait()
	h := state.GetHistory()
	if len(h) != 50 {
		t.Errorf("expected 50 entries, got %d", len(h))
	}
}

func TestAdvisor_HandleTaskComplete(t *testing.T) {
	mock := &mockLLM{
		response: `{"command":"whoami /all","rationale":"check privs","look_for":"groups","priority":"high","category":"recon","alternatives":[]}`,
	}
	ts := newMockTS()
	ts.agents["agent-1"] = adaptix.AgentData{
		Id:       "agent-1",
		Computer: "WORKSTATION",
		Username: "admin",
		Domain:   "CORP",
		Os:       1,
		Elevated: true,
	}

	a := &Advisor{
		config: AdvisorConfig{Enabled: true, MaxHistory: 10, MaxOutputLen: 4000},
		llm:    mock,
		states: make(map[string]*AgentState),
		ts:     ts,
	}

	task := adaptix.TaskData{
		AgentId:     "agent-1",
		CommandLine: "hostname",
		ClearText:   "WORKSTATION",
		MessageType: 7,
		Completed:   true,
	}

	a.handleTaskComplete("agent-1", task)

	if mock.callCount() != 1 {
		t.Fatalf("expected 1 LLM call, got %d", mock.callCount())
	}

	packets := ts.getPackets()
	if len(packets) != 1 {
		t.Fatalf("expected 1 sync packet, got %d", len(packets))
	}

	pkt, ok := packets[0].(SyncPacketAdvisor)
	if !ok {
		t.Fatalf("packet type = %T, want SyncPacketAdvisor", packets[0])
	}
	if pkt.SpType != TYPE_ADVISOR_RECOMMENDATION {
		t.Errorf("SpType = 0x%x, want 0x%x", pkt.SpType, TYPE_ADVISOR_RECOMMENDATION)
	}
	if pkt.Command != "whoami /all" {
		t.Errorf("command = %q, want %q", pkt.Command, "whoami /all")
	}
	if pkt.AgentID != "agent-1" {
		t.Errorf("agent = %q, want %q", pkt.AgentID, "agent-1")
	}
}

func TestAdvisor_SkipsEmptyCommand(t *testing.T) {
	mock := &mockLLM{response: `{"command":"test","rationale":"test","look_for":"test","priority":"low","category":"recon"}`}
	ts := newMockTS()
	a := &Advisor{
		config: AdvisorConfig{Enabled: true, MaxHistory: 10},
		llm:    mock,
		states: make(map[string]*AgentState),
		ts:     ts,
	}

	a.handleTaskComplete("agent-1", adaptix.TaskData{
		AgentId:   "agent-1",
		Completed: true,
	})

	if mock.callCount() != 0 {
		t.Errorf("expected 0 LLM calls for empty command, got %d", mock.callCount())
	}
}

func TestAdvisor_LLMError(t *testing.T) {
	mock := &mockLLM{err: fmt.Errorf("API down")}
	ts := newMockTS()
	a := &Advisor{
		config: AdvisorConfig{Enabled: true, MaxHistory: 10, MaxOutputLen: 4000},
		llm:    mock,
		states: make(map[string]*AgentState),
		ts:     ts,
	}

	a.handleTaskComplete("agent-1", adaptix.TaskData{
		AgentId:     "agent-1",
		CommandLine: "whoami",
		ClearText:   "admin",
		Completed:   true,
	})

	packets := ts.getPackets()
	if len(packets) != 0 {
		t.Errorf("expected 0 packets on LLM error, got %d", len(packets))
	}
}

func TestSyncPacketAdvisor_JSON(t *testing.T) {
	pkt := SyncPacketAdvisor{
		SpType:       TYPE_ADVISOR_RECOMMENDATION,
		AgentID:      "a1",
		Command:      "whoami /all",
		Rationale:    "check privs",
		LookFor:      "groups",
		Priority:     "high",
		Category:     "recon",
		Alternatives: []string{"net user"},
		Date:         1234567890,
	}

	data, err := json.Marshal(pkt)
	if err != nil {
		t.Fatalf("marshal error: %v", err)
	}

	var m map[string]interface{}
	if err := json.Unmarshal(data, &m); err != nil {
		t.Fatalf("unmarshal error: %v", err)
	}

	if int(m["type"].(float64)) != TYPE_ADVISOR_RECOMMENDATION {
		t.Errorf("type = %v, want 0xA1", m["type"])
	}
	if m["adv_command"] != "whoami /all" {
		t.Errorf("adv_command = %v", m["adv_command"])
	}
}

func TestAgentDataToContext(t *testing.T) {
	agent := adaptix.AgentData{
		Id:         "test-id",
		Computer:   "PC1",
		Username:   "user1",
		Domain:     "DOMAIN",
		Os:         1,
		OsDesc:     "Windows 10",
		Arch:       "x64",
		Elevated:   true,
		Pid:        "1000",
		Process:    "svchost.exe",
		InternalIP: "192.168.1.10",
		ExternalIP: "8.8.8.8",
	}

	ctx := agentDataToContext(agent)
	if ctx.AgentID != "test-id" {
		t.Errorf("AgentID = %q", ctx.AgentID)
	}
	if ctx.Hostname != "PC1" {
		t.Errorf("Hostname = %q", ctx.Hostname)
	}
	if !ctx.Elevated {
		t.Error("expected Elevated = true")
	}
}

func TestClaudeClient_NoAPIKey(t *testing.T) {
	client := &ClaudeClient{
		apiKey: "",
		model:  "claude-opus-4-6",
	}
	_, err := client.Complete(context.Background(), "sys", "user")
	if err == nil {
		t.Fatal("expected error when API key is empty")
	}
	if !strings.Contains(err.Error(), "ANTHROPIC_API_KEY") {
		t.Errorf("error = %q, should mention ANTHROPIC_API_KEY", err.Error())
	}
}

// --- Recon tracking tests ---

func TestReconState_MarkAndQuery(t *testing.T) {
	rs := NewReconState()
	if rs.IsCompleted(ReconIdentity) {
		t.Error("identity should not be completed initially")
	}

	rs.MarkCompleted(ReconIdentity)
	rs.MarkCompleted(ReconNetwork)

	if !rs.IsCompleted(ReconIdentity) {
		t.Error("identity should be completed after mark")
	}
	if !rs.IsCompleted(ReconNetwork) {
		t.Error("network should be completed after mark")
	}

	completed := rs.CompletedList()
	if len(completed) != 2 {
		t.Errorf("expected 2 completed, got %d", len(completed))
	}

	pending := rs.PendingList()
	if len(pending) != len(allCategories)-2 {
		t.Errorf("expected %d pending, got %d", len(allCategories)-2, len(pending))
	}
}

func TestReconState_Discoveries(t *testing.T) {
	rs := NewReconState()
	rs.AddDiscovery("users", "CORP\\admin", "CORP\\user1")
	rs.AddDiscovery("users", "CORP\\admin", "CORP\\user2") // admin is a dupe

	users := rs.GetDiscoveries("users")
	if len(users) != 3 {
		t.Errorf("expected 3 users, got %d: %v", len(users), users)
	}
}

func TestReconState_Summary(t *testing.T) {
	rs := NewReconState()
	rs.MarkCompleted(ReconIdentity)
	rs.MarkCompleted(ReconProcesses)

	summary := rs.Summary()
	if !strings.Contains(summary, "identity") {
		t.Error("summary should contain 'identity'")
	}
	if !strings.Contains(summary, "Pending:") {
		t.Error("summary should contain 'Pending:'")
	}
}

func TestReconState_Concurrency(t *testing.T) {
	rs := NewReconState()
	var wg sync.WaitGroup
	for i := 0; i < 50; i++ {
		wg.Add(1)
		go func(n int) {
			defer wg.Done()
			cat := allCategories[n%len(allCategories)]
			rs.MarkCompleted(cat)
			rs.AddDiscovery("users", fmt.Sprintf("user%d", n))
			_ = rs.Summary()
			_ = rs.PendingList()
			_ = rs.GetDiscoveries("users")
		}(i)
	}
	wg.Wait()

	if len(rs.CompletedList()) == 0 {
		t.Error("expected some completed categories")
	}
}

func TestClassifyCommand(t *testing.T) {
	tests := []struct {
		cmd      string
		expected []ReconCategory
	}{
		{"whoami /all", []ReconCategory{ReconIdentity}},
		{"net user /domain", []ReconCategory{ReconDomainUsers}},
		{"net localgroup administrators", []ReconCategory{ReconLocalUsers}},
		{"ipconfig /all", []ReconCategory{ReconNetwork}},
		{"tasklist /v", []ReconCategory{ReconProcesses}},
		{"ps", []ReconCategory{ReconProcesses}},
		{"mimikatz sekurlsa::logonpasswords", []ReconCategory{ReconCredentials}},
		{"schtasks /query", []ReconCategory{ReconScheduled}},
		{"random_command", nil},
		// false-positive guards
		{"opsec_check", nil},
		{"shell java -jar app.jar", nil},
		{"shell save output.txt", nil},
		{"cat /etc/passwd", nil},
	}

	for _, tt := range tests {
		cats := classifyCommand(tt.cmd)
		if len(cats) != len(tt.expected) {
			t.Errorf("classifyCommand(%q) = %v, want %v", tt.cmd, cats, tt.expected)
			continue
		}
		for i, cat := range cats {
			if cat != tt.expected[i] {
				t.Errorf("classifyCommand(%q)[%d] = %q, want %q", tt.cmd, i, cat, tt.expected[i])
			}
		}
	}
}

func TestParseDiscoveries_IPs(t *testing.T) {
	output := "Ethernet adapter:\n  IPv4 Address: 10.0.0.5\n  Subnet Mask: 255.255.255.0\n  Gateway: 10.0.0.1\n"
	d := parseDiscoveries("ipconfig /all", output)
	hosts := d["hosts"]
	if len(hosts) < 2 {
		t.Errorf("expected at least 2 hosts, got %v", hosts)
	}
	found := false
	for _, h := range hosts {
		if h == "10.0.0.5" {
			found = true
		}
	}
	if !found {
		t.Errorf("expected 10.0.0.5 in hosts, got %v", hosts)
	}
}

func TestParseDiscoveries_Users(t *testing.T) {
	output := "Token: CORP\\admin\nImpersonating: CORP\\svc_sql\n"
	d := parseDiscoveries("whoami /all", output)
	users := d["users"]
	if len(users) != 2 {
		t.Errorf("expected 2 users, got %v", users)
	}
}

func TestParseDiscoveries_FiltersWellKnown(t *testing.T) {
	output := "User: NT AUTHORITY\\SYSTEM\nSession: CORP\\admin\n"
	d := parseDiscoveries("whoami", output)
	users := d["users"]
	for _, u := range users {
		if strings.Contains(strings.ToLower(u), "nt authority") {
			t.Errorf("should filter NT AUTHORITY accounts, got %q", u)
		}
	}
}

func TestParseDiscoveries_NoIPsFromNonNetworkCommands(t *testing.T) {
	output := "Version: 10.0.19041\nBuild: 192.168.1.1\n"
	d := parseDiscoveries("whoami", output)
	if d != nil && len(d["hosts"]) > 0 {
		t.Errorf("should not extract IPs from non-network commands, got %v", d["hosts"])
	}
}

func TestBuildSystemPrompt_OPSECContent(t *testing.T) {
	prompt := BuildSystemPrompt("")
	checks := []string{
		"OPSEC",
		"BOF",
		"ANTI-PATTERNS",
		"shell whoami",
		"opsec_note",
		"detection",
		"stealthy",
	}
	for _, check := range checks {
		if !strings.Contains(prompt, check) {
			t.Errorf("OPSEC system prompt missing %q", check)
		}
	}
}

func TestBuildUserPrompt_WithReconSummary(t *testing.T) {
	ctx := AgentContext{Hostname: "HOST"}
	prompt := BuildUserPrompt(ctx, nil, "cmd", "out", 4000, "Completed: identity | Pending: network", "")
	if !strings.Contains(prompt, "RECON STATE") {
		t.Error("prompt should contain RECON STATE section")
	}
	if !strings.Contains(prompt, "Completed: identity") {
		t.Error("prompt should contain recon summary")
	}
}

func TestBuildUserPrompt_WithOtherAgents(t *testing.T) {
	ctx := AgentContext{Hostname: "HOST"}
	prompt := BuildUserPrompt(ctx, nil, "cmd", "out", 4000, "", "  [abc12345] CORP\\admin@DC01 | elevated=YES\n")
	if !strings.Contains(prompt, "OTHER AGENTS") {
		t.Error("prompt should contain OTHER AGENTS section")
	}
	if !strings.Contains(prompt, "DC01") {
		t.Error("prompt should contain other agent info")
	}
}

func TestBuildUserPrompt_EmptyReconAndAgents(t *testing.T) {
	ctx := AgentContext{Hostname: "HOST"}
	prompt := BuildUserPrompt(ctx, nil, "cmd", "out", 4000, "", "")
	if strings.Contains(prompt, "RECON STATE") {
		t.Error("prompt should not contain RECON STATE when empty")
	}
	if strings.Contains(prompt, "OTHER AGENTS") {
		t.Error("prompt should not contain OTHER AGENTS when empty")
	}
}

func TestAdvisor_ReconTracking(t *testing.T) {
	mock := &mockLLM{
		response: `{"command":"net user /domain","rationale":"enum domain","look_for":"users","priority":"high","category":"recon","opsec_note":"LDAP better","alternatives":[]}`,
	}
	ts := newMockTS()
	ts.agents["agent-1"] = adaptix.AgentData{
		Id: "agent-1", Computer: "WS1", Username: "admin", Domain: "CORP", Os: 1, Elevated: true,
	}

	a := &Advisor{
		config: AdvisorConfig{Enabled: true, MaxHistory: 10, MaxOutputLen: 4000},
		llm:    mock,
		states: make(map[string]*AgentState),
		ts:     ts,
	}

	a.handleTaskComplete("agent-1", adaptix.TaskData{
		AgentId:     "agent-1",
		CommandLine: "whoami /all",
		ClearText:   "CORP\\admin\nSeBkupPrivilege",
		MessageType: 7,
		Completed:   true,
	})

	state := a.states["agent-1"]
	if !state.Recon.IsCompleted(ReconIdentity) {
		t.Error("whoami should mark identity recon as completed")
	}

	users := state.Recon.GetDiscoveries("users")
	found := false
	for _, u := range users {
		if u == "CORP\\admin" {
			found = true
		}
	}
	if !found {
		t.Errorf("expected CORP\\admin in discoveries, got %v", users)
	}
}

func TestAdvisor_OpsecNoteInPacket(t *testing.T) {
	mock := &mockLLM{
		response: `{"command":"steal_token 1234","rationale":"get SYSTEM","look_for":"token","priority":"high","category":"privesc","opsec_note":"In-process, no child process","alternatives":[]}`,
	}
	ts := newMockTS()
	a := &Advisor{
		config: AdvisorConfig{Enabled: true, MaxHistory: 10, MaxOutputLen: 4000},
		llm:    mock,
		states: make(map[string]*AgentState),
		ts:     ts,
	}

	a.handleTaskComplete("agent-1", adaptix.TaskData{
		AgentId: "agent-1", CommandLine: "ps", ClearText: "PID 1234 SYSTEM", MessageType: 7, Completed: true,
	})

	packets := ts.getPackets()
	if len(packets) != 1 {
		t.Fatalf("expected 1 packet, got %d", len(packets))
	}
	pkt := packets[0].(SyncPacketAdvisor)
	if pkt.OpsecNote != "In-process, no child process" {
		t.Errorf("opsec_note = %q, want %q", pkt.OpsecNote, "In-process, no child process")
	}
}

func TestAdvisor_MultiAgentContext(t *testing.T) {
	mock := &mockLLM{
		response: `{"command":"test","rationale":"test","look_for":"test","priority":"low","category":"recon","alternatives":[]}`,
	}
	ts := newMockTS()
	ts.agents["agent-1"] = adaptix.AgentData{
		Id: "agent-1", Computer: "WS1", Username: "admin", Domain: "CORP",
		Os: 1, Elevated: true, Pid: "1234", Process: "explorer.exe", OsDesc: "Win10",
	}
	ts.agents["agent-2"] = adaptix.AgentData{
		Id: "agent-2", Computer: "DC01", Username: "SYSTEM", Domain: "CORP",
		Os: 1, Elevated: true, Pid: "4", Process: "System", OsDesc: "WinSrv2019",
	}

	a := &Advisor{
		config: AdvisorConfig{Enabled: true, MaxHistory: 10, MaxOutputLen: 4000},
		llm:    mock,
		states: make(map[string]*AgentState),
		ts:     ts,
	}

	a.handleTaskComplete("agent-1", adaptix.TaskData{
		AgentId: "agent-1", CommandLine: "whoami", ClearText: "CORP\\admin", MessageType: 7, Completed: true,
	})

	lastPrompt := mock.lastCall()
	if !strings.Contains(lastPrompt, "DC01") {
		t.Error("prompt should include other agent DC01 in multi-agent context")
	}
}

func TestAdvisor_FailedTaskSkipsRecon(t *testing.T) {
	mock := &mockLLM{
		response: `{"command":"retry","rationale":"retry","look_for":"output","priority":"low","category":"recon","alternatives":[]}`,
	}
	ts := newMockTS()
	a := &Advisor{
		config: AdvisorConfig{Enabled: true, MaxHistory: 10, MaxOutputLen: 4000},
		llm:    mock,
		states: make(map[string]*AgentState),
		ts:     ts,
	}

	a.handleTaskComplete("agent-1", adaptix.TaskData{
		AgentId: "agent-1", CommandLine: "whoami", ClearText: "error: access denied",
		MessageType: 6, Completed: true,
	})

	state := a.states["agent-1"]
	if state.Recon.IsCompleted(ReconIdentity) {
		t.Error("failed task should not mark recon as completed")
	}
}
