package krypt

import (
	"crypto/md5"
	"crypto/sha256"
	"encoding/hex"
	"hash/crc32"
)

func SHA256(data []byte) string {
	hash := sha256.New()
	hash.Write(data)
	hashBytes := hash.Sum(nil)
	return hex.EncodeToString(hashBytes)
}

func MD5(data []byte) string {
	hash := md5.New()
	hash.Write(data)
	hashBytes := hash.Sum(nil)
	return hex.EncodeToString(hashBytes)
}

func CRC32(data []byte) uint32 {
	table := crc32.MakeTable(crc32.IEEE)
	return crc32.Checksum(data, table)
}
