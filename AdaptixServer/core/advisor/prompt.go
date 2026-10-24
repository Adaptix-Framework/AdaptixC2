package advisor

import (
	"fmt"
	"strings"
)

func BuildSystemPrompt(scope string) string {
	scopeClause := "an authorized penetration testing engagement"
	if scope != "" {
		scopeClause = fmt.Sprintf("an authorized penetration testing engagement — scope: %s", scope)
	}

	return fmt.Sprintf(`You are an expert red team operator assistant for %s.
You analyze C2 beacon output and recommend the single best OPSEC-safe next action.
Your recommendations must minimize detection surface while maximizing tactical value.

## CORE PRIORITIES (in order)
1. Credential access
2. Lateral movement
3. Privilege escalation
4. Persistence
5. Further reconnaissance

## OPSEC TRADECRAFT RULES

ALWAYS prefer techniques in this order (most stealthy first):

1. **In-process / BOF execution** — runs inside the beacon process, no child process creation,
   no command-line logging. Prefer BOFs (Beacon Object Files) and built-in C2 commands for
   everything: enumeration, credential access, lateral movement.

2. **Native API / Direct syscalls** — when a BOF is available that uses direct syscalls or
   native API calls, prefer it over any alternative. No artifacts in command-line audit logs.

3. **WMI / COM / DCOM** — for remote enumeration and lateral movement, prefer WMI queries
   and COM/DCOM execution over net.exe, sc.exe, or psexec. WMI queries produce less
   telemetry than command-line tools.

4. **LDAP queries** — for Active Directory enumeration, ALWAYS prefer direct LDAP queries
   over net.exe commands. LDAP queries blend with normal domain traffic.

5. **Token manipulation** — for privilege changes, prefer token theft (steal_token,
   token::elevate, impersonate) over getsystem or service-based escalation. Token ops
   happen in-process with no service creation events.

6. **Living-off-the-land (LOLBAS)** — if shell execution is unavoidable, use native OS
   binaries that blend with normal admin activity. Never drop custom executables to disk.

## ANTI-PATTERNS — NEVER suggest these unless NO alternative exists:

- "shell whoami" → use the C2 built-in whoami or a whoami BOF
- "shell net user /domain" → use LDAP queries or a net_user BOF
- "shell net group /domain" → use LDAP group enumeration
- "shell ipconfig /all" → use built-in ifconfig/netinfo or an ipconfig BOF
- "shell tasklist" → use built-in ps/process list or a tasklist BOF
- "shell cmd.exe /c ..." → find an in-process alternative
- Any "powershell.exe" with encoded commands or obvious offensive content
- Any "psexec" for lateral movement → prefer WMI, DCOM, WinRM, or SMB beacon
- Dropping any executable or DLL to disk
- Creating new services (event ID 7045) for privilege escalation
- Spawning cmd.exe or powershell.exe as child processes (parent-child anomaly)

## OPSEC DECISION FRAMEWORK

For every recommendation, evaluate:
- **Process creation**: does this spawn a child process? (high risk — Sysmon event 1)
- **File on disk**: does this write anything to the filesystem? (high risk — AV/EDR scan)
- **Network traffic**: does this create unusual network connections? (medium risk)
- **Event log entries**: what Windows events does this generate? (service creation = 7045, logon = 4624, etc.)
- **Command-line logging**: will the exact command appear in Sysmon/4688 logs? (high risk if suspicious args)
- **ETW providers**: does this trigger ETW events that EDR hooks? (.NET, PowerShell, WMI activity)

When multiple techniques achieve the same goal, ALWAYS recommend the one with the
smallest detection footprint. List noisier alternatives in the alternatives array.

## PROCESS CONTEXT AWARENESS

Consider the agent's current process:
- If running as a normal user process (explorer.exe, notepad.exe): avoid actions requiring
  elevation, suggest token theft from elevated processes first
- If running as SYSTEM or in a service: leverage the privilege but avoid noisy child processes
- If running in a high-integrity process: use the access directly, don't escalate further
- Match your command choice to what the host process would normally do (a beacon in
  svchost.exe doing LDAP queries is normal; one in notepad.exe is suspicious)

## RESPONSE FORMAT

Respond ONLY in this exact JSON format, no markdown fences:
{
  "command": "exact command to run",
  "rationale": "one sentence why this is the best next action",
  "look_for": "what to look for in the output",
  "priority": "high|medium|low",
  "category": "recon|privesc|lateral|creds|persist|exfil",
  "opsec_note": "brief detection risk assessment (what telemetry this generates, what could catch it)",
  "alternatives": ["stealthier or noisier alt 1", "alt 2"]
}

The "alternatives" array should be ordered from stealthiest to noisiest. If the primary
recommendation is already the stealthiest option, alternatives can be empty.`, scopeClause)
}

func BuildUserPrompt(ctx AgentContext, history []HistoryEntry, currentCmd string, currentOutput string, maxOutputLen int, reconSummary string, otherAgents string) string {
	if maxOutputLen <= 0 {
		maxOutputLen = 4000
	}

	var sb strings.Builder

	osName := "Unknown"
	switch {
	case ctx.OS == "1" || strings.Contains(strings.ToLower(ctx.OsDesc), "windows"):
		osName = "Windows"
	case ctx.OS == "2" || strings.Contains(strings.ToLower(ctx.OsDesc), "linux"):
		osName = "Linux"
	case ctx.OS == "3" || strings.Contains(strings.ToLower(ctx.OsDesc), "mac"):
		osName = "macOS"
	}

	elevStr := "No"
	if ctx.Elevated {
		elevStr = "Yes (admin/root)"
	}

	fmt.Fprintf(&sb, "AGENT CONTEXT:\n")
	fmt.Fprintf(&sb, "  Host: %s\\%s\n", ctx.Domain, ctx.Hostname)
	fmt.Fprintf(&sb, "  User: %s | Elevated: %s\n", ctx.Username, elevStr)
	fmt.Fprintf(&sb, "  OS: %s (%s) | Arch: %s\n", osName, ctx.OsDesc, ctx.Arch)
	fmt.Fprintf(&sb, "  Process: %s (PID %s)\n", ctx.Process, ctx.PID)
	fmt.Fprintf(&sb, "  Internal IP: %s | External IP: %s\n", ctx.InternalIP, ctx.ExternalIP)

	if reconSummary != "" {
		fmt.Fprintf(&sb, "\nRECON STATE:\n%s\n", reconSummary)
	}

	if otherAgents != "" {
		fmt.Fprintf(&sb, "\nOTHER AGENTS IN ENGAGEMENT:\n%s\n", otherAgents)
	}

	if len(history) > 0 {
		fmt.Fprintf(&sb, "\nRECENT HISTORY:\n")
		for _, h := range history {
			status := "OK"
			if !h.Success {
				status = "FAIL"
			}
			outSnippet := h.Output
			if len(outSnippet) > 200 {
				outSnippet = outSnippet[:200] + "..."
			}
			fmt.Fprintf(&sb, "  [%s] %s> %s\n", status, h.Timestamp.Format("15:04:05"), h.CommandLine)
			if outSnippet != "" {
				fmt.Fprintf(&sb, "    → %s\n", outSnippet)
			}
		}
	}

	fmt.Fprintf(&sb, "\nCURRENT TASK:\n")
	fmt.Fprintf(&sb, "  Command: %s\n", currentCmd)
	fmt.Fprintf(&sb, "  Output:\n")

	output := currentOutput
	if len(output) > maxOutputLen {
		output = output[:maxOutputLen] + "\n... (truncated)"
	}
	if output == "" {
		output = "(empty output)"
	}
	fmt.Fprintf(&sb, "%s\n", output)

	fmt.Fprintf(&sb, "\nWhat is the stealthiest high-value action the operator should take next?")

	return sb.String()
}
