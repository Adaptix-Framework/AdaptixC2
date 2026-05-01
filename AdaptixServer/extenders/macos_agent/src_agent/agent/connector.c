#include "connector.h"
#include "crt.h"
#include "dyld_resolve.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

// Parse "host:port" string
static int parse_address(const char* address, char* host, size_t host_len, uint16_t* port) {
    const char* colon = (const char*)0;
    // Find last colon (handles IPv6 in brackets)
    for (const char* p = address; *p; p++) {
        if (*p == ':') colon = p;
    }
    if (!colon) return -1;

    size_t hlen = (size_t)(colon - address);
    if (hlen >= host_len) return -1;

    ax_memcpy(host, address, hlen);
    host[hlen] = '\0';

    // Parse port
    *port = 0;
    const char* p = colon + 1;
    while (*p >= '0' && *p <= '9') {
        *port = *port * 10 + (*p - '0');
        p++;
    }
    if (*port == 0) return -1;

    return 0;
}

int conn_open(connector_t* c, const char* address) {
    char host[256];
    uint16_t port;

    if (parse_address(address, host, sizeof(host), &port) != 0)
        return -1;

    // Resolve hostname
    struct addrinfo hints, *result;
    ax_memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP

    char port_str[8];
    ax_itoa(port, port_str, 10);

    if (R_getaddrinfo(host, port_str, &hints, &result) != 0)
        return -1;

    // Create socket
    c->fd = R_socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (c->fd < 0) {
        R_freeaddrinfo(result);
        return -1;
    }

    // Connect
    if (R_connect(c->fd, result->ai_addr, result->ai_addrlen) != 0) {
        R_close(c->fd);
        c->fd = -1;
        R_freeaddrinfo(result);
        return -1;
    }

    R_freeaddrinfo(result);
    return 0;
}

void conn_close(connector_t* c) {
    if (c->fd >= 0) {
        R_close(c->fd);
        c->fd = -1;
    }
}

int conn_read_exact(connector_t* c, uint8_t* buf, size_t size) {
    size_t total = 0;
    while (total < size) {
        ssize_t n = R_read(c->fd, buf + total, size - total);
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
        ax_free(*data, msg_len);
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
        ssize_t n = R_write(c->fd, header + total, 4 - total);
        if (n <= 0) return -1;
        total += (size_t)n;
    }

    // Send data
    total = 0;
    while (total < len) {
        ssize_t n = R_write(c->fd, data + total, len - total);
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
