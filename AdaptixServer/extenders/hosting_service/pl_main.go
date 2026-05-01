package main

import (
	"bytes"
	"crypto/aes"
	"crypto/cipher"
	"crypto/rand"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"regexp"
	"strings"
	"sync"
	"time"

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

const ServiceName = "Hosting"
const ExtenderName = "hosting_service"

// ============================================================================
// Data Model
// ============================================================================

type HostedFile struct {
	ID           string `json:"id"`
	Path         string `json:"path"`
	Filename     string `json:"filename"`
	MimeType     string `json:"mime_type"`
	ContentSize  int    `json:"content_size"`
	Encrypted    bool   `json:"encrypted"`
	EncKey       string `json:"enc_key,omitempty"`
	UAFilter     string `json:"ua_filter"`
	OneShot      bool   `json:"one_shot"`
	MaxDownloads int    `json:"max_downloads"`
	Downloads    int    `json:"downloads"`
	ExpiresAt    string `json:"expires_at"`
	Enabled      bool   `json:"enabled"`
	CreatedBy    string `json:"created_by"`
	CreatedAt    string `json:"created_at"`
}

// ============================================================================
// Service struct
// ============================================================================

type HostingService struct {
	ts        Teamserver
	moduleDir string
	mu        sync.RWMutex
	files     map[string]*HostedFile
}

var (
	Ts        Teamserver
	ModuleDir string
	Service   *HostingService
)

// ============================================================================
// Plugin Entry Points
// ============================================================================

func InitPlugin(ts any, moduleDir string, serviceConfig string) adaptix.PluginService {
	Ts = ts.(Teamserver)
	ModuleDir = moduleDir

	Service = &HostingService{
		ts:        Ts,
		moduleDir: moduleDir,
		files:     make(map[string]*HostedFile),
	}

	Service.restoreFiles()

	return Service
}

func (s *HostingService) Call(operator string, function string, args string) {
	switch function {

	case "list":
		s.HandleList(operator)

	case "add":
		s.HandleAdd(operator, args)

	case "delete":
		s.HandleDelete(operator, args)

	case "toggle":
		s.HandleToggle(operator, args)

	case "copyurl":
		s.HandleCopyURL(operator, args)

	case "host_payload":
		s.HandleHostPayload(operator, args)
	}
}

// ============================================================================
// Helpers: responses
// ============================================================================

func (s *HostingService) sendResponseAll(msgType string, data interface{}) {
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

func (s *HostingService) sendResponseClient(operator string, msgType string, data interface{}) {
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

func (s *HostingService) sendEvent(eventType string, data interface{}) {
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

func (s *HostingService) sendError(operator string, message string) {
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

// ============================================================================
// Helpers: ID generation
// ============================================================================

func generateID() string {
	b := make([]byte, 16)
	rand.Read(b)
	return hex.EncodeToString(b)
}

func generatePath() string {
	b := make([]byte, 8)
	rand.Read(b)
	return "/" + hex.EncodeToString(b)
}

// ============================================================================
// Handlers
// ============================================================================

func (s *HostingService) HandleList(operator string) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	var list []HostedFile
	for _, f := range s.files {
		list = append(list, *f)
	}
	s.sendResponseClient(operator, "files", list)
}

func (s *HostingService) HandleAdd(operator string, args string) {
	var req struct {
		Filename     string `json:"filename"`
		Content      string `json:"content"` // base64
		Path         string `json:"path"`
		MimeType     string `json:"mime_type"`
		OneShot      bool   `json:"one_shot"`
		Encrypted    bool   `json:"encrypted"`
		UAFilter     string `json:"ua_filter"`
		MaxDownloads int    `json:"max_downloads"`
		ExpiresAt    string `json:"expires_at"`
	}
	if err := json.Unmarshal([]byte(args), &req); err != nil {
		s.sendError(operator, "Invalid request: "+err.Error())
		return
	}

	if req.Content == "" {
		s.sendError(operator, "No file content provided")
		return
	}

	content, err := base64.StdEncoding.DecodeString(req.Content)
	if err != nil {
		s.sendError(operator, "Invalid base64 content: "+err.Error())
		return
	}

	s.addFile(operator, req.Filename, content, req.Path, req.MimeType, req.OneShot, req.Encrypted, req.UAFilter, req.MaxDownloads, req.ExpiresAt)
}

func (s *HostingService) HandleHostPayload(operator string, args string) {
	var req struct {
		Filename string `json:"filename"`
		Content  string `json:"content"` // base64
		Path     string `json:"path"`
		MimeType string `json:"mime_type"`
	}
	if err := json.Unmarshal([]byte(args), &req); err != nil {
		s.sendError(operator, "Invalid host_payload request: "+err.Error())
		return
	}

	content, err := base64.StdEncoding.DecodeString(req.Content)
	if err != nil {
		s.sendError(operator, "Invalid base64 content: "+err.Error())
		return
	}

	mime := req.MimeType
	if mime == "" {
		mime = "application/octet-stream"
	}

	s.addFile(operator, req.Filename, content, req.Path, mime, false, false, "", 0, "")
}

func (s *HostingService) addFile(operator, filename string, content []byte, path, mimeType string, oneShot, encrypted bool, uaFilter string, maxDownloads int, expiresAt string) {
	id := generateID()

	if path == "" {
		path = generatePath()
	}
	if !strings.HasPrefix(path, "/") {
		path = "/" + path
	}
	if mimeType == "" {
		mimeType = "application/octet-stream"
	}

	// Check path collision
	s.mu.RLock()
	for _, f := range s.files {
		if f.Path == path {
			s.mu.RUnlock()
			s.sendError(operator, "Path already in use: "+path)
			return
		}
	}
	s.mu.RUnlock()

	// Check if endpoint already registered externally
	if s.ts.TsEndpointExistsPublic("GET", path) {
		s.sendError(operator, "Endpoint already exists: "+path)
		return
	}

	var encKey string
	storedContent := content

	if encrypted {
		key := make([]byte, 32)
		rand.Read(key)
		encKey = hex.EncodeToString(key)

		encData, err := aesEncrypt(content, key)
		if err != nil {
			s.sendError(operator, "AES encryption failed: "+err.Error())
			return
		}
		storedContent = encData
	}

	hf := &HostedFile{
		ID:           id,
		Path:         path,
		Filename:     filename,
		MimeType:     mimeType,
		ContentSize:  len(content),
		Encrypted:    encrypted,
		EncKey:       encKey,
		UAFilter:     uaFilter,
		OneShot:      oneShot,
		MaxDownloads: maxDownloads,
		Downloads:    0,
		ExpiresAt:    expiresAt,
		Enabled:      true,
		CreatedBy:    operator,
		CreatedAt:    time.Now().Format("2006-01-02 15:04:05"),
	}

	// Save metadata
	metaJSON, err := json.Marshal(hf)
	if err != nil {
		s.sendError(operator, "Failed to marshal metadata: "+err.Error())
		return
	}
	if err := s.ts.TsExtenderDataSave(ExtenderName, "meta:"+id, metaJSON); err != nil {
		s.sendError(operator, "Failed to save metadata: "+err.Error())
		return
	}

	// Save content
	if err := s.ts.TsExtenderDataSave(ExtenderName, "data:"+id, storedContent); err != nil {
		s.sendError(operator, "Failed to save content: "+err.Error())
		return
	}

	// Register endpoint
	s.registerEndpoint(hf)

	s.mu.Lock()
	s.files[id] = hf
	s.mu.Unlock()

	// Broadcast updated file list
	s.broadcastFiles()
}

func (s *HostingService) HandleDelete(operator string, args string) {
	var req struct {
		ID string `json:"id"`
	}
	if err := json.Unmarshal([]byte(args), &req); err != nil {
		s.sendError(operator, "Invalid request")
		return
	}

	s.mu.Lock()
	hf, ok := s.files[req.ID]
	if !ok {
		s.mu.Unlock()
		s.sendError(operator, "File not found")
		return
	}

	path := hf.Path
	delete(s.files, req.ID)
	s.mu.Unlock()

	// Unregister endpoint
	s.ts.TsEndpointUnregisterPublic("GET", path)

	// Delete from storage
	s.ts.TsExtenderDataDelete(ExtenderName, "meta:"+req.ID)
	s.ts.TsExtenderDataDelete(ExtenderName, "data:"+req.ID)

	s.broadcastFiles()
}

func (s *HostingService) HandleToggle(operator string, args string) {
	var req struct {
		ID string `json:"id"`
	}
	if err := json.Unmarshal([]byte(args), &req); err != nil {
		s.sendError(operator, "Invalid request")
		return
	}

	s.mu.Lock()
	hf, ok := s.files[req.ID]
	if !ok {
		s.mu.Unlock()
		s.sendError(operator, "File not found")
		return
	}

	hf.Enabled = !hf.Enabled
	s.mu.Unlock()

	// Save updated metadata
	s.saveMeta(hf)

	s.broadcastFiles()
}

func (s *HostingService) HandleCopyURL(operator string, args string) {
	var req struct {
		ID string `json:"id"`
	}
	if err := json.Unmarshal([]byte(args), &req); err != nil {
		s.sendError(operator, "Invalid request")
		return
	}

	s.mu.RLock()
	hf, ok := s.files[req.ID]
	if !ok {
		s.mu.RUnlock()
		s.sendError(operator, "File not found")
		return
	}
	path := hf.Path
	s.mu.RUnlock()

	s.sendResponseClient(operator, "url", map[string]interface{}{
		"url": path,
	})
}

// ============================================================================
// HTTP Serving
// ============================================================================

func (s *HostingService) registerEndpoint(hf *HostedFile) {
	fileID := hf.ID
	path := hf.Path

	handler := func(w http.ResponseWriter, r *http.Request) {
		s.mu.RLock()
		f, ok := s.files[fileID]
		if !ok {
			s.mu.RUnlock()
			http.NotFound(w, r)
			return
		}

		// 1. Check enabled
		if !f.Enabled {
			s.mu.RUnlock()
			http.NotFound(w, r)
			return
		}

		// 2. Check expiration
		if f.ExpiresAt != "" {
			expiry, err := time.Parse("2006-01-02T15:04:05", f.ExpiresAt)
			if err == nil && time.Now().After(expiry) {
				s.mu.RUnlock()
				http.NotFound(w, r)
				return
			}
		}

		// 3. Check max downloads
		if f.MaxDownloads > 0 && f.Downloads >= f.MaxDownloads {
			s.mu.RUnlock()
			http.NotFound(w, r)
			return
		}

		// 4. Check UA filter
		if f.UAFilter != "" {
			ua := r.UserAgent()
			matched, err := regexp.MatchString(f.UAFilter, ua)
			if err != nil || !matched {
				s.mu.RUnlock()
				http.NotFound(w, r)
				return
			}
		}

		// Snapshot what we need before upgrading the lock
		mimeType := f.MimeType
		filename := f.Filename
		encrypted := f.Encrypted
		encKey := f.EncKey
		oneShot := f.OneShot
		s.mu.RUnlock()

		// Load content from storage
		contentData, err := s.ts.TsExtenderDataLoad(ExtenderName, "data:"+fileID)
		if err != nil || len(contentData) == 0 {
			http.NotFound(w, r)
			return
		}

		// 5. Increment download count
		s.mu.Lock()
		f2, ok2 := s.files[fileID]
		if ok2 {
			f2.Downloads++
			if oneShot {
				f2.Enabled = false
			}
		}
		s.mu.Unlock()

		// 7. Serve content
		w.Header().Set("Content-Type", mimeType)
		w.Header().Set("Content-Disposition", fmt.Sprintf("attachment; filename=\"%s\"", filename))
		w.Header().Set("Content-Length", fmt.Sprintf("%d", len(contentData)))
		w.Header().Set("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")

		// 8. If encrypted, add key header
		if encrypted && encKey != "" {
			w.Header().Set("X-Enc-Key", encKey)
		}

		w.Write(contentData)

		// 9. Broadcast download event
		remoteIP := getRemoteIP(r)
		userAgent := r.UserAgent()
		go func() {
			s.sendEvent("download", map[string]interface{}{
				"file_id":    fileID,
				"filename":   filename,
				"path":       path,
				"remote_ip":  remoteIP,
				"user_agent": userAgent,
				"time":       time.Now().Format("2006-01-02 15:04:05"),
			})

			// 10. Persist updated metadata
			s.mu.RLock()
			f3, ok3 := s.files[fileID]
			if ok3 {
				s.saveMeta(f3)
			}
			s.mu.RUnlock()

			// Broadcast updated files list
			s.broadcastFiles()
		}()
	}

	s.ts.TsEndpointRegisterPublicRaw("GET", path, handler)
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
// Persistence
// ============================================================================

func (s *HostingService) saveMeta(hf *HostedFile) {
	metaJSON, err := json.Marshal(hf)
	if err != nil {
		return
	}
	s.ts.TsExtenderDataSave(ExtenderName, "meta:"+hf.ID, metaJSON)
}

func (s *HostingService) restoreFiles() {
	keys, err := s.ts.TsExtenderDataKeys(ExtenderName)
	if err != nil {
		return
	}

	for _, key := range keys {
		if !strings.HasPrefix(key, "meta:") {
			continue
		}

		data, err := s.ts.TsExtenderDataLoad(ExtenderName, key)
		if err != nil {
			continue
		}

		var hf HostedFile
		if json.Unmarshal(data, &hf) != nil {
			continue
		}

		// Verify content exists
		id := strings.TrimPrefix(key, "meta:")
		_, err = s.ts.TsExtenderDataLoad(ExtenderName, "data:"+id)
		if err != nil {
			continue
		}

		s.files[hf.ID] = &hf

		// Re-register endpoint
		s.registerEndpoint(&hf)
	}
}

func (s *HostingService) broadcastFiles() {
	s.mu.RLock()
	defer s.mu.RUnlock()

	var list []HostedFile
	for _, f := range s.files {
		list = append(list, *f)
	}
	s.sendResponseAll("files", list)
}

// ============================================================================
// AES-256-CBC Encryption
// ============================================================================

func aesEncrypt(plaintext []byte, key []byte) ([]byte, error) {
	block, err := aes.NewCipher(key)
	if err != nil {
		return nil, err
	}

	// PKCS7 padding
	blockSize := block.BlockSize()
	padding := blockSize - len(plaintext)%blockSize
	padtext := bytes.Repeat([]byte{byte(padding)}, padding)
	plaintext = append(plaintext, padtext...)

	// Random IV
	iv := make([]byte, blockSize)
	if _, err := io.ReadFull(rand.Reader, iv); err != nil {
		return nil, err
	}

	// Encrypt
	mode := cipher.NewCBCEncrypter(block, iv)
	ciphertext := make([]byte, len(plaintext))
	mode.CryptBlocks(ciphertext, plaintext)

	// Prepend IV
	return append(iv, ciphertext...), nil
}
