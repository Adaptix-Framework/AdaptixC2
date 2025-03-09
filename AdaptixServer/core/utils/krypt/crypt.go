package krypt

import (
	"crypto/rc4"
	"errors"
)

func RC4Crypt(data []byte, key []byte) ([]byte, error) {
	rc4crypt, err := rc4.NewCipher(key)
	if err != nil {
		return nil, errors.New("rc4 decrypt error")
	}
	decryptData := make([]byte, len(data))
	rc4crypt.XORKeyStream(decryptData, data)
	return decryptData, nil
}
