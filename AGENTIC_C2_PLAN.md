# Agentic C2 — AI-Assisted Operator Plan

## Concept

An AI advisor that watches all beacon output in real-time, maintains engagement state, and recommends what to do next. The operator always approves — the AI never auto-executes. When output is empty or unhelpful, the AI suggests targeted recon. When output reveals opportunities (credentials, misconfigs, new hosts), it recommends exploitation paths.

---

## Feasibility: HIGH

AdaptixC2 already has the three things needed:

1. **Event hooks** — `TsEventHookOnPost("task.complete", handler)` fires with full `TaskData` (command, output, agent context) on every completed task. Zero code changes to subscribe.
2. **WebSocket broker** — `TsSyncAllClients()` pushes arbitrary JSON to all connected clients. The AI's recommendations travel the same pipe as task output.
3. **Dock widget system** — KDDockWidgets in the client. A new "AI Advisor" panel is a copy of `ChatWidget` with a different data source.

No core rewrites needed. This can be built as a **server-side Go module + client-side dock widget**.

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    AdaptixClient (Qt)                    │
│                                                         │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────┐  │
│  │ ConsoleWidget │  │ SessionsTab  │  │ AdvisorWidget │  │
│  │ (per-agent)   │  │              │  │ (new dock)    │  │
│  └──────┬───────┘  └──────────────┘  └───────┬───────┘  │
│         │ command input                      │ approve   │
│         ▼                                    ▼           │
│  ┌──────────────────────────────────────────────────┐    │
│  │           WebSocket + REST transport              │    │
│  └──────────────────────┬───────────────────────────┘    │
└─────────────────────────┼───────────────────────────────┘
                          │
┌─────────────────────────┼───────────────────────────────┐
│                  AdaptixServer (Go)                       │
│                         │                                │
│  ┌──────────────────────▼───────────────────────────┐    │
│  │              core/advisor/advisor.go               │    │
│  │                                                    │    │
│  │  ┌────────────┐  ┌─────────────┐  ┌────────────┐  │    │
│  │  │ EventHook  │  │ StateEngine │  │ LLM Client │  │    │
│  │  │ subscriber │──▶│ (per-agent  │──▶│ (Claude    │  │    │
│  │  │            │  │  context)   │  │  API)      │  │    │
│  │  └────────────┘  └─────────────┘  └─────┬──────┘  │    │
│  │                                         │          │    │
│  │                    recommendation ◀─────┘          │    │
│  │                         │                          │    │
│  │                         ▼                          │    │
│  │              TsSyncAllClients() push                │    │
│  └────────────────────────────────────────────────────┘    │
│                                                           │
└───────────────────────────────────────────────────────────┘
                          │
                          ▼
              ┌───────────────────────┐
              │   Claude API / Local  │
              │   LLM (operator's    │
              │   choice)            │
              └───────────────────────┘
```

---

## Data Flow

### 1. Agent output arrives

```
EventTaskComplete fires
  → TaskData.AgentId    (which beacon)
  → TaskData.ClearText  (plain text output — what the AI reads)
  → TaskData.CommandLine (what was run)
  → AgentData.*         (OS, user, hostname, domain, elevated, arch, process, IPs)
```

### 2. State engine builds context

Per-agent state accumulates across tasks:

```go
type AgentState struct {
    AgentID       string
    OS            string
    User          string
    Hostname      string
    Domain        string
    Elevated      bool
    
    // Accumulated from output parsing
    LocalUsers    []string
    LocalAdmins   []string
    NetSessions   []string
    Shares        []ShareInfo
    Credentials   []Credential
    Interfaces    []NetInterface
    InstalledSW   []string
    RunningProcs  []ProcessInfo
    ReconDone     map[string]bool   // tracks what recon has been run
    
    // Recent command history (sliding window)
    History       []HistoryEntry    // last N commands + outputs
}
```

### 3. LLM prompt construction

```
SYSTEM: You are an authorized red team operator assistant for engagement {scope}.
        You analyze command output and recommend the single best next action.
        Rules:
        - Only suggest commands available in this C2's command set
        - Never suggest actions outside the authorized scope
        - If current output is not useful, suggest targeted recon
        - Prioritize: credential access > lateral movement > persistence > escalation
        - Be specific: exact command syntax, not vague advice

CONTEXT:
  Agent: {hostname}\{user} | {os} | {elevated} | PID {pid} ({process})
  Domain: {domain}
  Recon completed: {list of what's been enumerated}
  Recon NOT done: {list of what hasn't been tried}
  Known credentials: {if any}
  Known hosts: {if any}
  
RECENT HISTORY:
  [{timestamp}] operator> {command}
  {output summary or full output if short}
  ... last 5-10 commands ...

CURRENT:
  Command: {command that just completed}
  Output:
  {full ClearText output}

What should the operator do next? Respond with:
1. Recommended command (exact syntax)
2. Why (one sentence)
3. What to look for in the output
```

### 4. Recommendation pushed to client

```go
type AdvisorRecommendation struct {
    AgentID     string   `json:"agent_id"`
    Command     string   `json:"command"`      // exact command to run
    Rationale   string   `json:"rationale"`     // why this action
    LookFor     string   `json:"look_for"`      // what to watch for in output
    Priority    string   `json:"priority"`      // high/medium/low
    Category    string   `json:"category"`      // recon/privesc/lateral/creds/persist
    Alternatives []string `json:"alternatives"` // other options
}
```

Pushed via WebSocket as a new sync packet type → AdvisorWidget displays it.

---

## Implementation Phases

### Phase 1 — Server-side advisor core (Go)

Files to create:
```
AdaptixServer/core/advisor/
├── advisor.go        // main module, event hook registration
├── state.go          // per-agent state accumulation
├── llm.go            // Claude API client (or pluggable LLM interface)
├── prompt.go         // prompt construction and response parsing
└── commands.go       // known command set for the current agent type
```

Key work:
- [x] Register `EventTaskComplete` post-hook in advisor.go
- [x] Build `AgentState` from `AgentData` + accumulated `TaskData.ClearText` parsing
- [x] Implement output parsers for common commands (whoami, net user, ipconfig, tasklist, etc.)
- [x] Claude API client (non-streaming, 120s timeout)
- [x] Prompt builder that includes agent context, recon state, and recent history
- [x] Response parser that extracts structured recommendation
- [x] New sync packet type `TYPE_ADVISOR_RECOMMENDATION` pushed via broker
- [x] Config in `profile.yaml`: API key, model, enable/disable, scope description

### Phase 2 — Client-side advisor widget (Qt/C++)

Files to create:
```
AdaptixClient/
├── Headers/UI/Widgets/AdvisorWidget.h
└── Source/UI/Widgets/AdvisorWidget.cpp
```

Key work:
- [x] New dock widget modeled on `ChatWidget` — shows recommendations per-agent
- [x] Each recommendation card shows: command, rationale, category badge, priority, OPSEC note
- [ ] "Execute" button → calls `HttpReqAgentCommandAsync()` with the suggested command
- [ ] "Dismiss" / "Ask why" buttons
- [ ] Toggle per-agent: operator can mute advisor for specific beacons
- [x] Add dock button to `AdaptixWidget` toolbar alongside existing Chat/Logs/etc.
- [x] Handle `TYPE_ADVISOR_RECOMMENDATION` in `ProcessSyncPacket.cpp`

### Phase 3 — Smart recon tracking

- [x] Parse output of enumeration commands to track what's been discovered (recon.go: classifyCommand + parseDiscoveries)
- [x] Maintain a "recon checklist" per agent — 16 categories (identity, local_users, network, domain_users, domain_trusts, shares, services, processes, software, av_edr, firewall, scheduled_tasks, credentials, spns, acls, gpo)
- [x] Recon state included in LLM prompt (completed/pending categories + discovered items)
- [x] Cross-agent awareness: multi-agent summary injected into prompt showing all beacons and their discoveries
- [x] OPSEC-aware system prompt: recommends stealthy commands (BOFs, LDAP, WMI, token manipulation) over noisy alternatives

### Phase 4 — Multi-agent orchestration

- [x] AI sees all agents in the engagement via GetAllAgents() + buildMultiAgentSummary()
- [x] Cross-agent discoveries shared in prompt (Agent A's found users/hosts/hashes visible to Agent B's advisor)
- [ ] Graph view integration: visual topology from agent discoveries
- [ ] Recommend lateral movement targets based on discovered credentials + network topology
- [ ] Suggest which agent should perform which action (closest to target, right privileges)
- [ ] Campaign-level strategy: "you have DA creds on Agent 3, recommend DCSync from there"

---

## LLM Provider Options

| Option | Pros | Cons |
|--------|------|------|
| **Claude API (cloud)** | Best reasoning, tool-use support, large context window | Requires internet from C2 server, API costs, engagement data leaves network |
| **Local model (ollama/llama.cpp)** | Air-gapped safe, no API costs, no data exfil risk | Weaker reasoning, needs GPU on C2 server, slower |
| **Pluggable interface** | Operator chooses per-engagement | More code to maintain |

**Recommendation**: Build a pluggable `LLMClient` interface. Default to Claude API for connected engagements, local model for air-gapped. The prompt format stays the same either way.

```go
type LLMClient interface {
    Complete(ctx context.Context, systemPrompt string, messages []Message) (string, error)
}

// Implementations:
// - ClaudeClient (Anthropic API)
// - OllamaClient (local models)
// - OpenAIClient (compatible APIs)
```

---

## Safety Rails

These are non-negotiable:

1. **Human-in-the-loop**: AI NEVER auto-executes. Every recommendation requires operator click.
2. **Scope enforcement**: `profile.yaml` defines authorized scope (IP ranges, domains). AI prompt includes scope. Commands targeting out-of-scope assets are filtered server-side before reaching the client.
3. **Audit log**: Every recommendation (accepted or dismissed) is logged with timestamp, agent, operator.
4. **Kill switch**: Global enable/disable in profile.yaml + per-agent toggle in client.
5. **Rate limiting**: Max 1 recommendation per task completion. No autonomous polling loops.
6. **No credential storage in LLM context**: Found creds are referenced by ID ("Credential #3"), not by value, in prompts sent to cloud LLMs. Only local LLM gets raw values.

---

## Config Addition (profile.yaml)

```yaml
advisor:
  enabled: true
  provider: "claude"           # claude | ollama | openai-compatible
  api_key: "${ANTHROPIC_API_KEY}"
  model: "claude-sonnet-5-5"   # cost-effective for advisory
  scope: "Internal pentest — 10.0.0.0/8, CORP.LOCAL domain"
  local_endpoint: ""           # for ollama: http://localhost:11434
  max_history: 10              # commands to include in context
  auto_recon: true             # suggest recon when idle
  redact_creds: true           # redact credential values in cloud LLM prompts
```

---

## Effort Estimate

| Phase | Effort | Dependencies |
|-------|--------|-------------|
| Phase 1 (server core) | 2-3 days | Go, Claude API SDK |
| Phase 2 (client widget) | 2-3 days | Qt/C++, Phase 1 |
| Phase 3 (smart recon) | 1-2 days | Output parsers, Phase 1 |
| Phase 4 (multi-agent) | 3-5 days | Phase 1-3 complete |

Total: ~2 weeks for a fully functional AI advisor. Phase 1+2 alone gives a usable MVP in ~5 days.

---

## What Makes This Different From "Just Ask ChatGPT"

1. **Real-time context** — the AI sees every command output as it arrives, not copy-paste
2. **Stateful** — it remembers what recon has been done, what creds were found, across the entire engagement
3. **Integrated execution** — one click to run the suggested command, not copy-paste into console
4. **Multi-agent awareness** — sees all beacons, can recommend cross-agent tactics
5. **Scoped** — knows the exact C2 command set, won't suggest commands that don't exist
6. **Auditable** — every recommendation logged, accepted or rejected
