#ifndef TASKS_ASYNC_H
#define TASKS_ASYNC_H

#include "msgpack.h"
#include <stdint.h>

/// Async command handlers -- download, upload, run, job_list, job_kill
/// These launch background threads with separate C2 connections

int task_download(const uint8_t *data, uint32_t data_len, mp_writer_t *w);
int task_upload(const uint8_t *data, uint32_t data_len, mp_writer_t *w);
int task_run(const uint8_t *data, uint32_t data_len, mp_writer_t *w);
int task_job_list(mp_writer_t *w);
int task_job_kill(const uint8_t *data, uint32_t data_len, mp_writer_t *w);

#endif /* TASKS_ASYNC_H */
