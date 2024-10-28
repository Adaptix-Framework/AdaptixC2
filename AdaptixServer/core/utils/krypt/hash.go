package krypt

import (
	"crypto/sha256"
	"encoding/hex"
	"hash/crc32"
)

func SHA256(data []byte) string {
	hash := sha256.New()
	hash.Write([]byte(data))
	hashBytes := hash.Sum(nil)
	return hex.EncodeToString(hashBytes)
}

func CRC32(data []byte) uint32 {
	table := crc32.MakeTable(crc32.IEEE)
	return crc32.Checksum(data, table)

}
