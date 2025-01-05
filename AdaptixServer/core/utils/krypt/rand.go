package krypt

import (
	"crypto/rand"
	"encoding/hex"
	"fmt"
)

func GenerateUID(length int) (string, error) {
	if length <= 0 {
		return "", fmt.Errorf("length must be greater than 0")
	}

	byteLength := length / 2
	if length%2 != 0 {
		byteLength++
	}

	randomBytes := make([]byte, byteLength)
	_, err := rand.Read(randomBytes)
	if err != nil {
		return "", err
	}

	return hex.EncodeToString(randomBytes)[:length], nil
}

func GenerateSlice(length int) []byte {
	if length <= 0 {
		return nil
	}

	randomBytes := make([]byte, length)
	_, err := rand.Read(randomBytes)
	if err != nil {
		return nil
	}

	return randomBytes[:length]
}
