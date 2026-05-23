#include "connector.h"
#include "crt.h"
#include "types.h"

#ifdef ARCH_X86_64
#include "syscalls_x64.h"
#endif
#ifdef ARCH_AARCH64
#include "syscalls_aarch64.h"
#endif

// Linux socket constants
#define AF_INET      2
#define SOCK_STREAM  1
#define IPPROTO_TCP  6

// sockaddr_in structure (manual — no libc headers)
struct linux_sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;    // network byte order
    uint32_t sin_addr;    // network byte order
    uint8_t  sin_zero[8];
};

// Parse "host:port" string — host must be an IP address (no DNS resolution)
// For Phase 1, we only support direct IP:port (DNS will come in Phase 2 with resolver)
static int parse_address(const char* address, uint32_t* ip, uint16_t* port) {
    const char* colon = (const char*)0;
    for (const char* p = address; *p; p++) {
        if (*p == ':') colon = p;
    }
    if (!colon) return -1;

    // Parse IP: a.b.c.d
    uint32_t octets[4] = {0};
    int octet_idx = 0;
    for (const char* p = address; p < colon && octet_idx < 4; p++) {
        if (*p == '.') {
            octet_idx++;
        } else if (*p >= '0' && *p <= '9') {
            octets[octet_idx] = octets[octet_idx] * 10 + (*p - '0');
        } else {
            return -1;
        }
    }
    if (octet_idx != 3) return -1;
    for (int i = 0; i < 4; i++) {
        if (octets[i] > 255) return -1;
    }

    // Network byte order (big-endian)
    *ip = (octets[0]) | (octets[1] << 8) | (octets[2] << 16) | (octets[3] << 24);

    // Parse port
    *port = 0;
    for (const char* p = colon + 1; *p >= '0' && *p <= '9'; p++) {
        *port = *port * 10 + (*p - '0');
    }
    if (*port == 0) return -1;

    // Convert port to network byte order (big-endian)
    *port = ((*port >> 8) & 0xFF) | ((*port & 0xFF) << 8);

    return 0;
}

int conn_open(connector_t* c, const char* address) {
    uint32_t ip;
    uint16_t port;

    if (parse_address(address, &ip, &port) != 0)
        return -1;

    // Create TCP socket via direct syscall
    int fd = (int)sys_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return -1;

    // Build sockaddr_in
    struct linux_sockaddr_in addr;
    ax_memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = port;
    addr.sin_addr = ip;

    // Connect via direct syscall
    if (sys_connect(fd, (const void*)&addr, sizeof(addr)) != 0) {
        sys_close(fd);
        return -1;
    }

    c->fd = fd;
    return 0;
}

void conn_close(connector_t* c) {
    if (c->fd >= 0) {
        sys_close(c->fd);
        c->fd = -1;
    }
}

int conn_read_exact(connector_t* c, uint8_t* buf, size_t size) {
    size_t total = 0;
    while (total < size) {
        long n = sys_read(c->fd, buf + total, size - total);
        if (n <= 0) return -1;
        total += (size_t)n;
    }
    return 0;
}

int conn_recv_msg(connector_t* c, uint8_t** data, size_t* len) {
    // Read 4-byte big-endian length
    uint8_t len_buf[4];
    if (conn_read_exact(c, len_buf, 4) != 0) return -1;

    uint32_t msg_len = ((uint32_t)len_buf[0] << 24) | ((uint32_t)len_buf[1] << 16) |
                       ((uint32_t)len_buf[2] << 8)  | len_buf[3];

    if (msg_len == 0) {
        *data = (uint8_t*)0;
        *len = 0;
        return 0;
    }

    // Sanity check: max 64MB
    if (msg_len > 64 * 1024 * 1024) return -1;

    *data = (uint8_t*)ax_malloc(msg_len);
    if (!*data) return -1;

    if (conn_read_exact(c, *data, msg_len) != 0) {
        ax_free(*data);
        *data = (uint8_t*)0;
        return -1;
    }

    *len = msg_len;
    return 0;
}

int conn_send_msg(connector_t* c, const uint8_t* data, size_t len) {
    // Write 4-byte big-endian length + data
    uint8_t header[4] = {
        (uint8_t)(len >> 24), (uint8_t)(len >> 16),
        (uint8_t)(len >> 8),  (uint8_t)len
    };

    // Send header
    size_t total = 0;
    while (total < 4) {
        long n = sys_write(c->fd, header + total, 4 - total);
        if (n <= 0) return -1;
        total += (size_t)n;
    }

    // Send data
    total = 0;
    while (total < len) {
        long n = sys_write(c->fd, data + total, len - total);
        if (n <= 0) return -1;
        total += (size_t)n;
    }

    return 0;
}

int conn_discard(connector_t* c, size_t size) {
    uint8_t tmp[1024];
    size_t remaining = size;
    while (remaining > 0) {
        size_t chunk = remaining < sizeof(tmp) ? remaining : sizeof(tmp);
        if (conn_read_exact(c, tmp, chunk) != 0) return -1;
        remaining -= chunk;
    }
    return 0;
}

// fd_set manipulation for pselect6
// Linux fd_set is an array of unsigned long bitmasks
typedef struct {
    unsigned long fds_bits[1024 / (8 * sizeof(unsigned long))];
} conn_fdset_t;

static inline void conn_fd_zero(conn_fdset_t* set) {
    for (unsigned i = 0; i < sizeof(set->fds_bits) / sizeof(set->fds_bits[0]); i++)
        set->fds_bits[i] = 0;
}

static inline void conn_fd_set(int fd, conn_fdset_t* set) {
    unsigned idx = (unsigned)fd / (8 * sizeof(unsigned long));
    unsigned bit = (unsigned)fd % (8 * sizeof(unsigned long));
    set->fds_bits[idx] |= (1UL << bit);
}

static inline int conn_fd_isset(int fd, conn_fdset_t* set) {
    unsigned idx = (unsigned)fd / (8 * sizeof(unsigned long));
    unsigned bit = (unsigned)fd % (8 * sizeof(unsigned long));
    return (set->fds_bits[idx] & (1UL << bit)) != 0;
}

int conn_poll_read(connector_t* c, int timeout_ms) {
    if (c->fd < 0) return -1;

    conn_fdset_t rfds;
    conn_fd_zero(&rfds);
    conn_fd_set(c->fd, &rfds);

    struct linux_timespec ts;
    ts.tv_sec = timeout_ms / 1000;
    ts.tv_nsec = (long)(timeout_ms % 1000) * 1000000L;

    int ret = sys_pselect6(c->fd + 1, (void*)&rfds, (void*)0, (void*)0, &ts, (void*)0);
    if (ret < 0) return -1;  // error
    if (ret == 0) return 0;  // timeout
    return 1;                // data available
}

int conn_recv_msg_timeout(connector_t* c, uint8_t** data, size_t* len, int timeout_ms) {
    *data = (uint8_t*)0;
    *len = 0;

    int poll = conn_poll_read(c, timeout_ms);
    if (poll <= 0) return poll;  // 0 = timeout, -1 = error

    // Data available — do blocking recv (data is ready)
    return conn_recv_msg(c, data, len);
}

// Socket option constants
#define SOL_SOCKET     1
#define SO_REUSEADDR   2

int conn_bind_listen(connector_t* server, uint16_t port) {
    int fd = (int)sys_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return -1;

    // SO_REUSEADDR — allow quick rebind after disconnect
    int opt = 1;
    sys_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind to 0.0.0.0:port
    struct linux_sockaddr_in addr;
    ax_memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = ((port >> 8) & 0xFF) | ((port & 0xFF) << 8); // host→network byte order
    addr.sin_addr = 0; // INADDR_ANY

    if (sys_bind(fd, (const void*)&addr, sizeof(addr)) != 0) {
        sys_close(fd);
        return -1;
    }

    if (sys_listen(fd, 1) != 0) {
        sys_close(fd);
        return -1;
    }

    server->fd = fd;
    return 0;
}

int conn_accept(connector_t* client, connector_t* server) {
    int fd = sys_accept(server->fd, (void*)0, (void*)0);
    if (fd < 0) return -1;

    client->fd = fd;
    return 0;
}
