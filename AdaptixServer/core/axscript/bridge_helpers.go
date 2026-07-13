package axscript

import (
	"encoding/base64"
	"encoding/hex"
	"fmt"
	"strings"

	"github.com/dop251/goja"
)

// /---
func jsBytes(v goja.Value) ([]byte, bool) {
	if v == nil || goja.IsUndefined(v) || goja.IsNull(v) {
		return nil, false
	}
	switch x := v.Export().(type) {
	case goja.ArrayBuffer:
		return x.Bytes(), true
	case []byte:
		return x, true
	case string:
		return []byte(x), true
	}
	return nil, false
}

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

// / ---
func xorBytes(data []byte, key string) []byte {
	if len(key) == 0 {
		return data
	}
	keyBytes := []byte(key)
	out := make([]byte, len(data))
	for i := range data {
		out[i] = data[i] ^ keyBytes[i%len(keyBytes)]
	}
	return out
}

// / ---
func encodeData(alg string, data []byte, key string) (text string, raw []byte, isText bool) {
	switch alg {
	case "base64":
		return base64.StdEncoding.EncodeToString(data), nil, true
	case "hex":
		return hex.EncodeToString(data), nil, true
	case "xor":
		return "", xorBytes(data, key), false
	default:
		return base64.StdEncoding.EncodeToString(data), nil, true
	}
}
