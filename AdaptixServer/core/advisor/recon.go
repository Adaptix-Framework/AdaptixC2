package advisor

import (
	"fmt"
	"regexp"
	"strings"
	"sync"
	"time"
)

type ReconCategory string

const (
	ReconIdentity    ReconCategory = "identity"
	ReconLocalUsers  ReconCategory = "local_users"
	ReconNetwork     ReconCategory = "network"
	ReconDomainUsers ReconCategory = "domain_users"
	ReconDomainTrust ReconCategory = "domain_trusts"
	ReconShares      ReconCategory = "shares"
	ReconServices    ReconCategory = "services"
	ReconProcesses   ReconCategory = "processes"
	ReconSoftware    ReconCategory = "software"
	ReconAV          ReconCategory = "av_edr"
	ReconFirewall    ReconCategory = "firewall"
	ReconScheduled   ReconCategory = "scheduled_tasks"
	ReconCredentials ReconCategory = "credentials"
	ReconSPNs        ReconCategory = "spns"
	ReconACLs        ReconCategory = "acls"
	ReconGPO         ReconCategory = "gpo"
)

var allCategories = []ReconCategory{
	ReconIdentity, ReconLocalUsers, ReconNetwork,
	ReconDomainUsers, ReconDomainTrust, ReconShares,
	ReconServices, ReconProcesses, ReconSoftware,
	ReconAV, ReconFirewall, ReconScheduled,
	ReconCredentials, ReconSPNs, ReconACLs, ReconGPO,
}

type ReconState struct {
	mu          sync.RWMutex
	Completed   map[ReconCategory]time.Time
	Discoveries map[string][]string
}

func NewReconState() *ReconState {
	return &ReconState{
		Completed:   make(map[ReconCategory]time.Time),
		Discoveries: make(map[string][]string),
	}
}

func (r *ReconState) MarkCompleted(cat ReconCategory) {
	r.mu.Lock()
	defer r.mu.Unlock()
	if _, ok := r.Completed[cat]; !ok {
		r.Completed[cat] = time.Now().UTC()
	}
}

func (r *ReconState) IsCompleted(cat ReconCategory) bool {
	r.mu.RLock()
	defer r.mu.RUnlock()
	_, ok := r.Completed[cat]
	return ok
}

func (r *ReconState) AddDiscovery(category string, items ...string) {
	r.mu.Lock()
	defer r.mu.Unlock()

	existing := r.Discoveries[category]
	seen := make(map[string]bool, len(existing))
	for _, e := range existing {
		seen[e] = true
	}
	for _, item := range items {
		item = strings.TrimSpace(item)
		if item != "" && !seen[item] {
			existing = append(existing, item)
			seen[item] = true
		}
	}
	r.Discoveries[category] = existing
}

func (r *ReconState) GetDiscoveries(category string) []string {
	r.mu.RLock()
	defer r.mu.RUnlock()
	out := make([]string, len(r.Discoveries[category]))
	copy(out, r.Discoveries[category])
	return out
}

func (r *ReconState) CompletedList() []ReconCategory {
	r.mu.RLock()
	defer r.mu.RUnlock()
	out := make([]ReconCategory, 0, len(r.Completed))
	for cat := range r.Completed {
		out = append(out, cat)
	}
	return out
}

func (r *ReconState) PendingList() []ReconCategory {
	r.mu.RLock()
	defer r.mu.RUnlock()
	out := make([]ReconCategory, 0)
	for _, cat := range allCategories {
		if _, ok := r.Completed[cat]; !ok {
			out = append(out, cat)
		}
	}
	return out
}

func (r *ReconState) Summary() string {
	r.mu.RLock()
	defer r.mu.RUnlock()

	var completed, pending []string
	for _, cat := range allCategories {
		if _, ok := r.Completed[cat]; ok {
			completed = append(completed, string(cat))
		} else {
			pending = append(pending, string(cat))
		}
	}

	var sb strings.Builder
	if len(completed) > 0 {
		fmt.Fprintf(&sb, "Completed: %s", strings.Join(completed, ", "))
	} else {
		sb.WriteString("Completed: (none)")
	}
	sb.WriteString(" | ")
	if len(pending) > 0 {
		fmt.Fprintf(&sb, "Pending: %s", strings.Join(pending, ", "))
	} else {
		sb.WriteString("Pending: (none)")
	}
	return sb.String()
}

func (r *ReconState) DiscoverySummary() string {
	r.mu.RLock()
	defer r.mu.RUnlock()

	if len(r.Discoveries) == 0 {
		return ""
	}

	var sb strings.Builder
	for cat, items := range r.Discoveries {
		if len(items) == 0 {
			continue
		}
		display := items
		if len(display) > 10 {
			display = display[:10]
		}
		fmt.Fprintf(&sb, "  %s: %s", cat, strings.Join(display, ", "))
		if len(items) > 10 {
			fmt.Fprintf(&sb, " (+%d more)", len(items)-10)
		}
		sb.WriteString("\n")
	}
	return sb.String()
}

type reconRule struct {
	category ReconCategory
	patterns []string
}

var reconRules = []reconRule{
	{ReconIdentity, []string{"whoami", "getuid", "token::", "getprivs", "getsystem"}},
	{ReconLocalUsers, []string{"net user", "net localgroup", "local_users", "wmic useraccount"}},
	{ReconNetwork, []string{"ipconfig", "ifconfig", "netstat", "arp -", "arp /", "route print", "route -n", "nslookup", "resolve"}},
	{ReconDomainUsers, []string{"net user /domain", "net group /domain", "ldap_query", "get-domainuser", "get-netuser", "adfind"}},
	{ReconDomainTrust, []string{"nltest", "get-domaintrust", "get-foresttrust", "domain trust"}},
	{ReconShares, []string{"net share", "net view", "shares", "get-netshare", "invoke-sharefinder"}},
	{ReconServices, []string{"sc query", "get-service", "wmic service"}},
	{ReconProcesses, []string{"tasklist", "ps", "process_list", "get-process", "wmic process"}},
	{ReconSoftware, []string{"wmic product", "reg query.*uninstall", "get-installedsoftware"}},
	{ReconAV, []string{"defender", "seatbelt", "get-mpcomputerstatus", "wmic /namespace:\\\\root\\securitycenter2", "av_check", "edr_query"}},
	{ReconFirewall, []string{"netsh advfirewall", "firewall", "get-netfirewallrule", "iptables"}},
	{ReconScheduled, []string{"schtasks", "crontab"}},
	{ReconCredentials, []string{"mimikatz", "hashdump", "sekurlsa", "logonpasswords", "lsadump", "dcsync", "credential", "vault::"}},
	{ReconSPNs, []string{"kerberoast", "setspn", "get-netuser -spn", "invoke-kerberoast"}},
	{ReconACLs, []string{"dacl", "bloodhound", "sharphound", "invoke-acl", "get-objectacl", "get-domainobjectacl", "find-interestingdomainacl"}},
	{ReconGPO, []string{"gpresult", "get-gpo", "get-domaingpo", "group policy"}},
}

func isWordMatch(s, word string) bool {
	idx := strings.Index(s, word)
	if idx < 0 {
		return false
	}
	if idx > 0 {
		prev := s[idx-1]
		if (prev >= 'a' && prev <= 'z') || (prev >= '0' && prev <= '9') || prev == '_' {
			return false
		}
	}
	end := idx + len(word)
	if end < len(s) {
		next := s[end]
		if (next >= 'a' && next <= 'z') || (next >= '0' && next <= '9') || next == '_' {
			return false
		}
	}
	return true
}

var wordBoundaryPatterns = map[string]bool{
	"ps": true, "shares": true, "services": true,
}

func classifyCommand(commandLine string) []ReconCategory {
	lower := strings.ToLower(commandLine)
	var matched []ReconCategory
	seen := make(map[ReconCategory]bool)

	for _, rule := range reconRules {
		if seen[rule.category] {
			continue
		}
		for _, pat := range rule.patterns {
			var hit bool
			if wordBoundaryPatterns[pat] {
				hit = isWordMatch(lower, pat)
			} else {
				hit = strings.Contains(lower, pat)
			}
			if hit {
				matched = append(matched, rule.category)
				seen[rule.category] = true
				break
			}
		}
	}

	// Domain-specific commands that also match local: domain takes priority
	// but we keep both if explicitly both
	if seen[ReconDomainUsers] && seen[ReconLocalUsers] {
		if strings.Contains(lower, "/domain") && !strings.Contains(lower, "localgroup") {
			// "net user /domain" — remove local_users
			filtered := matched[:0]
			for _, c := range matched {
				if c != ReconLocalUsers {
					filtered = append(filtered, c)
				}
			}
			matched = filtered
		}
	}

	return matched
}

var (
	reIPv4     = regexp.MustCompile(`\b(?:\d{1,3}\.){3}\d{1,3}\b`)
	reDomUser  = regexp.MustCompile(`(?i)\b([A-Z][A-Z0-9._-]{0,14})\\([A-Za-z0-9._-]{1,64})\b`)
	reUPN      = regexp.MustCompile(`(?i)\b([A-Za-z0-9._-]+)@([A-Za-z0-9.-]+\.[A-Za-z]{2,})\b`)
	reNTHash   = regexp.MustCompile(`(?i)\b[a-f0-9]{32}\b`)
	reHostFQDN = regexp.MustCompile(`(?i)\b([A-Za-z][A-Za-z0-9-]{1,14})\.([A-Za-z0-9.-]+\.[A-Za-z]{2,})\b`)
)

func parseDiscoveries(commandLine, output string) map[string][]string {
	if output == "" {
		return nil
	}

	discoveries := make(map[string][]string)
	lower := strings.ToLower(commandLine)

	// IPs — only extract from network-related commands
	if containsAny(lower, "ipconfig", "ifconfig", "arp", "netstat", "route", "ping", "net view", "resolve", "nslookup", "scan") {
		ips := reIPv4.FindAllString(output, 50)
		var filtered []string
		for _, ip := range ips {
			if ip != "0.0.0.0" && ip != "255.255.255.255" && ip != "127.0.0.1" && !strings.HasPrefix(ip, "255.") {
				filtered = append(filtered, ip)
			}
		}
		if len(filtered) > 0 {
			discoveries["hosts"] = dedupe(filtered)
		}
	}

	// DOMAIN\user patterns
	if matches := reDomUser.FindAllStringSubmatch(output, 50); len(matches) > 0 {
		var users []string
		for _, m := range matches {
			full := m[1] + "\\" + m[2]
			lu := strings.ToLower(full)
			if lu == "nt authority\\system" || lu == "nt authority\\local service" || lu == "nt authority\\network service" {
				continue
			}
			users = append(users, full)
		}
		if len(users) > 0 {
			discoveries["users"] = dedupe(users)
		}
	}

	// user@domain UPN patterns
	if matches := reUPN.FindAllStringSubmatch(output, 50); len(matches) > 0 {
		var upns []string
		for _, m := range matches {
			upns = append(upns, m[0])
		}
		if len(upns) > 0 {
			existing := discoveries["users"]
			discoveries["users"] = dedupe(append(existing, upns...))
		}
	}

	// NTLM hashes — only from credential-related commands
	if containsAny(lower, "mimikatz", "hashdump", "sekurlsa", "lsadump", "dcsync", "sam", "credential") {
		hashes := reNTHash.FindAllString(output, 20)
		if len(hashes) > 0 {
			discoveries["hashes"] = dedupe(hashes)
		}
	}

	// FQDN hostnames
	if matches := reHostFQDN.FindAllString(output, 30); len(matches) > 0 {
		var hosts []string
		for _, m := range matches {
			lm := strings.ToLower(m)
			if strings.HasSuffix(lm, ".dll") || strings.HasSuffix(lm, ".exe") || strings.HasSuffix(lm, ".sys") || strings.HasSuffix(lm, ".com") || strings.HasSuffix(lm, ".org") || strings.HasSuffix(lm, ".net") {
				continue
			}
			hosts = append(hosts, m)
		}
		if len(hosts) > 0 {
			existing := discoveries["hosts"]
			discoveries["hosts"] = dedupe(append(existing, hosts...))
		}
	}

	if len(discoveries) == 0 {
		return nil
	}
	return discoveries
}

func containsAny(s string, substrs ...string) bool {
	for _, sub := range substrs {
		if strings.Contains(s, sub) {
			return true
		}
	}
	return false
}

func dedupe(items []string) []string {
	seen := make(map[string]bool, len(items))
	out := make([]string, 0, len(items))
	for _, item := range items {
		if !seen[item] {
			seen[item] = true
			out = append(out, item)
		}
	}
	return out
}
