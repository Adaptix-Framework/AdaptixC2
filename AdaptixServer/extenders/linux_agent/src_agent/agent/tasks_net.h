#ifndef TASKS_NET_H
#define TASKS_NET_H

#include "msgpack.h"
#include <stdint.h>

/// Network command handlers
/// Tunnels: MUX model via proxyfire (no threads, data in main channel)
/// Terminals: thread + separate connection (Phase 2 migration)

int task_tunnel_start(const uint8_t *data, uint32_t data_len, mp_writer_t *w);
int task_tunnel_write(const uint8_t *data, uint32_t data_len, mp_writer_t *w);
int task_tunnel_stop(const uint8_t *data, uint32_t data_len, mp_writer_t *w);
int task_tunnel_pause(const uint8_t *data, uint32_t data_len, mp_writer_t *w);
int task_tunnel_resume(const uint8_t *data, uint32_t data_len, mp_writer_t *w);

int task_terminal_start(const uint8_t *data, uint32_t data_len, mp_writer_t *w);
int task_terminal_stop(const uint8_t *data, uint32_t data_len, mp_writer_t *w);

#endif /* TASKS_NET_H */
