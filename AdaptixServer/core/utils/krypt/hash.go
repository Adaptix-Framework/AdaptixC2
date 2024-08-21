package krypt

import (
	"crypto/sha256"
	"encoding/hex"
)

func SHA256(data []byte) string {
	hash := sha256.New()
	hash.Write([]byte(data))
	hashBytes := hash.Sum(nil)
	return hex.EncodeToString(hashBytes)
}
