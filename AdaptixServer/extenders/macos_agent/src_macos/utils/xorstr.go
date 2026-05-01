package utils

// Xor decodes a XOR-obfuscated byte slice with the given key.
// Used to hide sensitive strings from static analysis (strings command).
func Xor(data []byte, key []byte) string {
	out := make([]byte, len(data))
	kl := len(key)
	for i := range data {
		out[i] = data[i] ^ key[i%kl]
	}
	return string(out)
}
