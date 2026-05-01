#ifndef AGENT_INFO_H
#define AGENT_INFO_H

#include "msgpack.h"
#include <stdint.h>

/// System info collection for session registration.
/// Individual getters fill a caller-provided buffer, return 0 on success.
/// create_session_info() builds the full SessionInfo msgpack payload.

int agent_info_hostname(char *buf, int len);
int agent_info_username(char *buf, int len);
int agent_info_ipaddr(char *buf, int len);
int agent_info_osversion(char *buf, int len);

/// Build SessionInfo msgpack payload matching Go's utils.SessionInfo struct.
/// Also generates a random 16-byte session encryption key.
///
/// msgpack keys (declaration order, matching Go vmihailenco/msgpack):
///   acp, elevated, encrypt_key, host, ipaddr, oem, os, os_version, pid, process, user
///
/// Returns 0 on success, fills session_key (16 bytes).
int create_session_info(mp_writer_t *w, uint8_t *session_key);

#endif /* AGENT_INFO_H */
