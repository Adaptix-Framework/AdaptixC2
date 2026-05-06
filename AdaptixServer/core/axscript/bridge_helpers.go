package axscript

import (
	"encoding/base64"
	"encoding/hex"
	"fmt"
	"strings"
)

// /---
func bytesToCode(language string, data []byte, varName string) string {
	if len(data) == 0 {
		return ""
	}

	switch language {
	case "c", "cpp":
		var sb strings.Builder
		sb.WriteString(fmt.Sprintf("unsigned char %s[] = {\n    ", varName))
		for i, b := range data {
			if i > 0 {
				sb.WriteString(", ")
				if i%16 == 0 {
					sb.WriteString("\n    ")
				}
			}
			sb.WriteString(fmt.Sprintf("0x%02x", b))
		}
		sb.WriteString("\n};\n")
		sb.WriteString(fmt.Sprintf("unsigned int %s_len = %d;\n", varName, len(data)))
		return sb.String()

	case "csharp", "cs":
		var sb strings.Builder
		sb.WriteString(fmt.Sprintf("byte[] %s = new byte[%d] {\n    ", varName, len(data)))
		for i, b := range data {
			if i > 0 {
				sb.WriteString(", ")
				if i%16 == 0 {
					sb.WriteString("\n    ")
				}
			}
			sb.WriteString(fmt.Sprintf("0x%02x", b))
		}
		sb.WriteString("\n};\n")
		return sb.String()

	case "python", "py":
		var sb strings.Builder
		sb.WriteString(fmt.Sprintf("%s = b\"", varName))
		for _, b := range data {
			sb.WriteString(fmt.Sprintf("\\x%02x", b))
		}
		sb.WriteString("\"\n")
		return sb.String()

	case "powershell", "ps":
		var sb strings.Builder
		sb.WriteString(fmt.Sprintf("[Byte[]] $%s = ", varName))
		for i, b := range data {
			if i > 0 {
				sb.WriteString(",")
			}
			sb.WriteString(fmt.Sprintf("0x%02x", b))
		}
		sb.WriteString("\n")
		return sb.String()

	case "vba", "vbs":
		var sb strings.Builder
		sb.WriteString(fmt.Sprintf("%s = Array(", varName))
		for i, b := range data {
			if i > 0 {
				sb.WriteString(", ")
				if i%16 == 0 {
					sb.WriteString("_\n    ")
				}
			}
			sb.WriteString(fmt.Sprintf("%d", b))
		}
		sb.WriteString(")\n")
		return sb.String()

	case "ruby":
		var sb strings.Builder
		sb.WriteString(fmt.Sprintf("%s = \"", varName))
		for _, b := range data {
			sb.WriteString(fmt.Sprintf("\\x%02x", b))
		}
		sb.WriteString("\"\n")
		return sb.String()

	default:
		return base64.StdEncoding.EncodeToString(data)
	}
}

// /---
func encodeData(alg string, data []byte, key string) string {
	switch alg {
	case "base64":
		return base64.StdEncoding.EncodeToString(data)
	case "hex":
		return hex.EncodeToString(data)
	case "xor":
		if len(key) == 0 {
			return base64.StdEncoding.EncodeToString(data)
		}
		keyBytes := []byte(key)
		result := make([]byte, len(data))
		for i := range data {
			result[i] = data[i] ^ keyBytes[i%len(keyBytes)]
		}
		return base64.StdEncoding.EncodeToString(result)
	default:
		return base64.StdEncoding.EncodeToString(data)
	}
}

// /---
func decodeData(alg string, data string, key string) string {
	switch alg {
	case "base64":
		decoded, err := base64.StdEncoding.DecodeString(data)
		if err != nil {
			return ""
		}
		return string(decoded)
	case "hex":
		decoded, err := hex.DecodeString(data)
		if err != nil {
			return ""
		}
		return string(decoded)
	case "xor":
		raw, err := base64.StdEncoding.DecodeString(data)
		if err != nil {
			return ""
		}
		if len(key) == 0 {
			return string(raw)
		}
		keyBytes := []byte(key)
		result := make([]byte, len(raw))
		for i := range raw {
			result[i] = raw[i] ^ keyBytes[i%len(keyBytes)]
		}
		return string(result)
	default:
		return data
	}
}

// /---
func decodeRawData(alg string, rawData []byte, key string) []byte {
	switch alg {
	case "xor":
		if len(key) == 0 {
			return rawData
		}
		keyBytes := []byte(key)
		result := make([]byte, len(rawData))
		for i := range rawData {
			result[i] = rawData[i] ^ keyBytes[i%len(keyBytes)]
		}
		return result
	default:
		return rawData
	}
}
