package main

import (
	"encoding/json"
	"fmt"
	"io"
	"os"
	"os/signal"
	"strings"
	"sync/atomic"
	"time"
)

var interruptFlag int32

func init() {
	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, os.Interrupt)
	go func() {
		for range sigCh {
			atomic.StoreInt32(&interruptFlag, 1)
		}
	}()
}

type agentData struct {
	Id           string `json:"a_id"`
	Name         string `json:"a_name"`
	Listener     string `json:"a_listener"`
	Async        bool   `json:"a_async"`
	ExternalIP   string `json:"a_external_ip"`
	InternalIP   string `json:"a_internal_ip"`
	Sleep        uint   `json:"a_sleep"`
	Jitter       uint   `json:"a_jitter"`
	Pid          string `json:"a_pid"`
	Tid          string `json:"a_tid"`
	Arch         string `json:"a_arch"`
	Elevated     bool   `json:"a_elevated"`
	Process      string `json:"a_process"`
	Os           int    `json:"a_os"`
	OsDesc       string `json:"a_os_desc"`
	Domain       string `json:"a_domain"`
	Computer     string `json:"a_computer"`
	Username     string `json:"a_username"`
	Impersonated string `json:"a_impersonated"`
	ACP          int    `json:"a_acp"`
	OemCP        int    `json:"a_oemcp"`
	CreateTime   int64  `json:"a_create_time"`
	LastTick     int    `json:"a_last_tick"`
	Tags         string `json:"a_tags"`
	Mark         string `json:"a_mark"`
	Color        string `json:"a_color"`
}

type listenerData struct {
	Name     string `json:"l_name"`
	Protocol string `json:"l_protocol"`
	Type     string `json:"l_type"`
	BindHost string `json:"l_bind_host"`
	BindPort string `json:"l_bind_port"`
	Status   string `json:"l_status"`
}

type downloadData struct {
	FileId    string `json:"d_file_id"`
	AgentId   string `json:"d_agent_id"`
	AgentName string `json:"d_agent_name"`
	File      string `json:"d_file"`
	Size      int64  `json:"d_size"`
	RecvSize  int64  `json:"d_recv_size"`
	State     int    `json:"d_state"`
	User      string `json:"d_user"`
	Computer  string `json:"d_computer"`
	Date      int64  `json:"d_date"`
}

func runAgentList(c *Client, jsonOut, onlineOnly bool) error {
	respBody, err := c.doJSON("GET", "/agent/list", nil)
	if err != nil {
		return err
	}

	var agents []agentData
	if err := json.Unmarshal(respBody, &agents); err != nil {
		var resp struct {
			OK      bool        `json:"ok"`
			Message string      `json:"message"`
			Agents  []agentData `json:"agents"`
		}
		json.Unmarshal(respBody, &resp)
		if !resp.OK {
			return fmt.Errorf("server error: %s", resp.Message)
		}
		agents = resp.Agents
	}

	if onlineOnly {
		filtered := make([]agentData, 0, len(agents))
		for _, a := range agents {
			if a.Mark != "Disconnect" {
				filtered = append(filtered, a)
			}
		}
		agents = filtered
	}

	if len(agents) == 0 {
		fmt.Println("no agents connected")
		return nil
	}

	if jsonOut {
		printJSON(agents)
		return nil
	}

	tw := newTabWriter()
	fmt.Fprintln(tw, "ID\tNAME\tOS\tUSER\tCOMPUTER\tPROCESS\tARCH\tLISTENER")
	for _, a := range agents {
		fmt.Fprintf(tw, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
			truncate(a.Id, 12),
			a.Name,
			osName(a.Os)+" "+a.OsDesc,
			a.Username,
			a.Computer,
			a.Process,
			a.Arch,
			a.Listener,
		)
	}
	tw.Flush()
	return nil
}

func runAgentInfo(c *Client, agentID string, jsonOut bool) error {
	respBody, err := c.doJSON("GET", "/agent/list", nil)
	if err != nil {
		return err
	}

	var agents []agentData
	json.Unmarshal(respBody, &agents)

	var found *agentData
	for i := range agents {
		if strings.HasPrefix(agents[i].Id, agentID) {
			found = &agents[i]
			break
		}
	}
	if found == nil {
		return fmt.Errorf("agent %s not found", agentID)
	}

	if jsonOut {
		printJSON(found)
		return nil
	}

	fmt.Printf("ID:            %s\n", found.Id)
	fmt.Printf("Name:          %s\n", found.Name)
	fmt.Printf("OS:            %s %s\n", osName(found.Os), found.OsDesc)
	fmt.Printf("Computer:      %s\n", found.Computer)
	fmt.Printf("Domain:        %s\n", found.Domain)
	fmt.Printf("Username:      %s\n", found.Username)
	fmt.Printf("Internal IP:   %s\n", found.InternalIP)
	fmt.Printf("External IP:   %s\n", found.ExternalIP)
	fmt.Printf("Process:       %s (PID: %s)\n", found.Process, found.Pid)
	fmt.Printf("Arch:          %s\n", found.Arch)
	fmt.Printf("Elevated:      %v\n", found.Elevated)
	fmt.Printf("Listener:      %s\n", found.Listener)
	fmt.Printf("Sleep/Jitter:  %d/%d\n", found.Sleep, found.Jitter)
	fmt.Printf("Tags:          %s\n", found.Tags)
	fmt.Printf("Mark:          %s\n", found.Mark)
	return nil
}

func runAgentRemove(c *Client, agentIDs []string) error {
	respBody, err := c.doJSON("POST", "/agent/remove", map[string]interface{}{
		"agent_id_array": agentIDs,
	})
	if err != nil {
		return err
	}

	var resp apiResponse
	json.Unmarshal(respBody, &resp)
	if resp.OK || resp.Message == "" {
		fmt.Println("agents removed")
		return nil
	}
	return fmt.Errorf("remove failed: %s", resp.Message)
}

func runListenerList(c *Client, jsonOut bool) error {
	respBody, err := c.doJSON("GET", "/listener/list", nil)
	if err != nil {
		return err
	}

	var listeners []listenerData
	json.Unmarshal(respBody, &listeners)

	if len(listeners) == 0 {
		fmt.Println("no active listeners")
		return nil
	}

	if jsonOut {
		printJSON(listeners)
		return nil
	}

	tw := newTabWriter()
	fmt.Fprintln(tw, "NAME\tTYPE\tPROTOCOL\tBIND\tSTATUS")
	for _, l := range listeners {
		fmt.Fprintf(tw, "%s\t%s\t%s\t%s:%s\t%s\n",
			l.Name, l.Type, l.Protocol, l.BindHost, l.BindPort, l.Status,
		)
	}
	tw.Flush()
	return nil
}

func runListenerStart(c *Client, configType, name, config string) error {
	respBody, err := c.doJSON("POST", "/listener/create", map[string]string{
		"name":   name,
		"type":   configType,
		"config": config,
	})
	if err != nil {
		return err
	}

	var resp apiResponse
	json.Unmarshal(respBody, &resp)
	if resp.OK {
		fmt.Println("listener started")
		return nil
	}
	return fmt.Errorf("start failed: %s", resp.Message)
}

func runListenerStop(c *Client, configType, name string) error {
	respBody, err := c.doJSON("POST", "/listener/stop", map[string]string{
		"name": name,
		"type": configType,
	})
	if err != nil {
		return err
	}

	var resp apiResponse
	json.Unmarshal(respBody, &resp)
	if resp.OK {
		fmt.Println("listener stopped")
		return nil
	}
	return fmt.Errorf("stop failed: %s", resp.Message)
}

func runDownloadList(c *Client, jsonOut bool) error {
	respBody, err := c.doJSON("GET", "/download/list", nil)
	if err != nil {
		return err
	}

	var downloads []downloadData
	json.Unmarshal(respBody, &downloads)

	if len(downloads) == 0 {
		fmt.Println("no downloads")
		return nil
	}

	if jsonOut {
		printJSON(downloads)
		return nil
	}

	tw := newTabWriter()
	fmt.Fprintln(tw, "FILE ID\tAGENT\tFILE\tSIZE\tSTATE")
	for _, dl := range downloads {
		size := ""
		if dl.Size > 0 {
			if dl.State == 3 {
				size = formatBytes(dl.Size)
			} else {
				size = fmt.Sprintf("%s / %s", formatBytes(dl.RecvSize), formatBytes(dl.Size))
			}
		}
		fmt.Fprintf(tw, "%s\t%s\t%s\t%s\t%s\n",
			truncate(dl.FileId, 12),
			dl.AgentName,
			dl.File,
			size,
			stateName(dl.State),
		)
	}
	tw.Flush()
	return nil
}

func runDownloadGet(c *Client, fileID, outputPath string) error {
	respBody, err := c.doJSON("GET", "/download/list", nil)
	if err != nil {
		return err
	}

	var downloads []downloadData
	json.Unmarshal(respBody, &downloads)

	var found *downloadData
	for i := range downloads {
		if strings.HasPrefix(downloads[i].FileId, fileID) {
			found = &downloads[i]
			break
		}
	}
	if found == nil {
		return fmt.Errorf("download %s not found", fileID)
	}

	otp, err := c.getOTP("download", map[string]string{"id": found.FileId})
	if err != nil {
		return fmt.Errorf("get download OTP: %w", err)
	}

	resp, err := c.httpClient.Get(c.baseURL + "/otp/download/sync?otp=" + otp)
	if err != nil {
		return fmt.Errorf("download request: %w", err)
	}
	defer resp.Body.Close()

	content, err := io.ReadAll(resp.Body)
	if err != nil {
		return fmt.Errorf("read download: %w", err)
	}

	if outputPath == "" {
		outputPath = found.File
		if idx := strings.LastIndex(outputPath, "/"); idx >= 0 {
			outputPath = outputPath[idx+1:]
		}
		if idx := strings.LastIndex(outputPath, "\\"); idx >= 0 {
			outputPath = outputPath[idx+1:]
		}
	}

	if err := os.WriteFile(outputPath, content, 0644); err != nil {
		return fmt.Errorf("write file: %w", err)
	}
	fmt.Printf("downloaded %s -> %s (%s)\n", found.File, outputPath, formatBytes(int64(len(content))))
	return nil
}

func runAgentExec(c *Client, agentID string, cmdline string, args []string, timeout time.Duration, jsonOut bool) error {
	if err := c.connectWebSocket(); err != nil {
		return fmt.Errorf("connect: %w", err)
	}
	defer c.disconnect()

	if err := c.syncAndWait(); err != nil {
		return err
	}

	return c.execCommand(agentID, cmdline, timeout)
}

func (c *Client) syncAndWait() error {
	if _, err := c.doJSON("POST", "/sync", map[string]interface{}{}); err != nil {
		return fmt.Errorf("trigger sync: %w", err)
	}
	if err := c.waitSync(); err != nil {
		return fmt.Errorf("sync: %w", err)
	}
	return nil
}

func (c *Client) execCommand(agentID string, cmdline string, timeout time.Duration) error {
	cmdBody := map[string]interface{}{
		"id":      agentID,
		"cmdline": cmdline,
	}
	respBody, err := c.doJSON("POST", "/agent/command/raw", cmdBody)
	if err != nil {
		return fmt.Errorf("send command: %w", err)
	}
	var resp apiResponse
	json.Unmarshal(respBody, &resp)
	if !resp.OK && resp.Message != "" {
		return fmt.Errorf("command error: %s", resp.Message)
	}

	return c.waitOutput(agentID, timeout)
}

func (c *Client) waitOutput(agentID string, timeout time.Duration) error {
	deadline := time.After(timeout)
	var lastOutputTime time.Time

	for {
		if c.wsConn == nil {
			return nil
		}

		if atomic.LoadInt32(&interruptFlag) == 1 {
			atomic.StoreInt32(&interruptFlag, 0)
			fmt.Print("^C\n")
			return nil
		}

		select {
		case <-deadline:
			return fmt.Errorf("timeout waiting for output")
		default:
		}

		c.wsConn.SetReadDeadline(time.Now().Add(500 * time.Millisecond))
		msg, err := c.wsReadMessage()
		if err != nil {
			if isTimeoutErr(err) {
				if !lastOutputTime.IsZero() && time.Since(lastOutputTime) > 5*time.Second {
					return nil
				}
				continue
			}
			return nil
		}
		c.wsConn.SetReadDeadline(time.Time{})

		output, done := parseSyncPacket(msg, agentID)
		if output != "" {
			fmt.Print(output)
			lastOutputTime = time.Now()
		}
		if done {
			return nil
		}
	}
}

func parseSyncPacket(msg []byte, agentID string) (output string, done bool) {
	var packet struct {
		Type        int    `json:"type"`
		AgentId     string `json:"a_id"`
		MessageType int    `json:"a_msg_type"`
		Message     string `json:"a_message"`
		ClearText   string `json:"a_text"`
		Completed   bool   `json:"a_completed"`
	}
	if err := json.Unmarshal(msg, &packet); err != nil {
		return "", false
	}

	if packet.AgentId == "" {
		return "", false
	}

	isTargetAgent := packet.AgentId == agentID || strings.HasPrefix(agentID, packet.AgentId) || strings.HasPrefix(packet.AgentId, agentID)
	if !isTargetAgent {
		return "", false
	}

	switch packet.Type {
	case 0x67, 0x68, 0x69:
		output := packet.Message
		if packet.ClearText != "" {
			output = packet.ClearText
		}
		return output, false

	case 0x49, 0x4a, 0x6a, 0x6b:
		output := packet.Message
		if packet.ClearText != "" {
			output = packet.ClearText
		}
		return output, packet.Completed

	case 0x4d:
		var hook struct {
			Completed bool   `json:"a_completed"`
			Message   string `json:"a_message"`
			Text      string `json:"a_text"`
		}
		json.Unmarshal(msg, &hook)
		if hook.Text != "" {
			return hook.Text, hook.Completed
		}
		if hook.Message != "" {
			return hook.Message, hook.Completed
		}
		return "", hook.Completed

	default:
		return "", false
	}
}

func getAgentInfo(c *Client, agentID string) *agentData {
	respBody, err := c.doJSON("GET", "/agent/list", nil)
	if err != nil {
		return nil
	}
	var agents []agentData
	json.Unmarshal(respBody, &agents)
	for i := range agents {
		if strings.HasPrefix(agents[i].Id, agentID) || strings.HasPrefix(agentID, agents[i].Id) {
			return &agents[i]
		}
	}
	return nil
}

func detectShellType(a *agentData) string {
	if a == nil {
		return "sh"
	}
	switch a.Os {
	case 1: // Windows
		return "cmd"
	case 2: // Linux
		return "bash"
	case 3: // macOS
		return "sh"
	default:
		return "sh"
	}
}

func isTimeoutErr(err error) bool {
	if err == nil {
		return false
	}
	s := err.Error()
	return strings.Contains(s, "timeout") || strings.Contains(s, "deadline")
}

func formatBytes(n int64) string {
	if n < 1024 {
		return fmt.Sprintf("%d B", n)
	}
	if n < 1024*1024 {
		return fmt.Sprintf("%.1f KB", float64(n)/1024)
	}
	if n < 1024*1024*1024 {
		return fmt.Sprintf("%.1f MB", float64(n)/(1024*1024))
	}
	return fmt.Sprintf("%.1f GB", float64(n)/(1024*1024*1024))
}

func truncate(s string, max int) string {
	if len(s) <= max {
		return s
	}
	return s[:max]
}
