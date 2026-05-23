#ifndef TASKS_FS_H
#define TASKS_FS_H

#include "msgpack.h"
#include <stdint.h>

int task_cd(const uint8_t* data, uint32_t data_len, mp_writer_t* w);
int task_cat(const uint8_t* data, uint32_t data_len, mp_writer_t* w);
int task_ls(const uint8_t* data, uint32_t data_len, mp_writer_t* w);
int task_cp(const uint8_t* data, uint32_t data_len, mp_writer_t* w);
int task_mv(const uint8_t* data, uint32_t data_len, mp_writer_t* w);
int task_mkdir(const uint8_t* data, uint32_t data_len, mp_writer_t* w);
int task_rm(const uint8_t* data, uint32_t data_len, mp_writer_t* w);
int task_zip(const uint8_t* data, uint32_t data_len, mp_writer_t* w);

#endif // TASKS_FS_H
