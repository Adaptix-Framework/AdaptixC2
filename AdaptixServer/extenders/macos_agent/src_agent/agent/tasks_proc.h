#ifndef TASKS_PROC_H
#define TASKS_PROC_H

#include "msgpack.h"
#include <stdint.h>

int task_ps(mp_writer_t* w);
int task_kill(const uint8_t* data, uint32_t data_len, mp_writer_t* w);
int task_shell(const uint8_t* data, uint32_t data_len, mp_writer_t* w);

#endif // TASKS_PROC_H
