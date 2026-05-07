package token

import (
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"sync"
	"time"
)

type otpEntry struct {
	otpType   string
	data      interface{}
	createdAt time.Time
}

type OTPManager struct {
	mutex      sync.Mutex
	entries    map[string]*otpEntry
	ttl        time.Duration
	tokenBytes int
	done       chan struct{}
}

func NewOTPManager(ttl time.Duration, cleanupInterval time.Duration) *OTPManager {
	s := &OTPManager{
		entries:    make(map[string]*otpEntry),
		ttl:        ttl,
		tokenBytes: 32,
		done:       make(chan struct{}),
	}
	go s.cleanupLoop(cleanupInterval)
	return s
}

func (s *OTPManager) generateToken() (string, error) {
	b := make([]byte, s.tokenBytes)
	if _, err := rand.Read(b); err != nil {
		return "", fmt.Errorf("otp: failed to generate token: %w", err)
	}
	return hex.EncodeToString(b), nil
}

func (s *OTPManager) Create(otpType string, data interface{}) (string, error) {
	token, err := s.generateToken()
	if err != nil {
		return "", err
	}

	s.mutex.Lock()
	s.entries[token] = &otpEntry{
		otpType:   otpType,
		data:      data,
		createdAt: time.Now(),
	}
	s.mutex.Unlock()

	return token, nil
}

func (s *OTPManager) Validate(token string) (string, interface{}, bool) {
	s.mutex.Lock()
	defer s.mutex.Unlock()

	entry, exists := s.entries[token]
	if !exists {
		return "", nil, false
	}

	delete(s.entries, token)

	if time.Since(entry.createdAt) > s.ttl {
		return "", nil, false
	}

	return entry.otpType, entry.data, true
}

func (s *OTPManager) Len() int {
	s.mutex.Lock()
	defer s.mutex.Unlock()
	return len(s.entries)
}

func (s *OTPManager) Stop() {
	select {
	case <-s.done:
	default:
		close(s.done)
	}
}

func (s *OTPManager) cleanupLoop(interval time.Duration) {
	ticker := time.NewTicker(interval)
	defer ticker.Stop()

	for {
		select {
		case <-s.done:
			return
		case <-ticker.C:
			s.purgeExpired()
		}
	}
}

func (s *OTPManager) purgeExpired() {
	now := time.Now()
	s.mutex.Lock()
	defer s.mutex.Unlock()

	for token, entry := range s.entries {
		if now.Sub(entry.createdAt) > s.ttl {
			delete(s.entries, token)
		}
	}
}
