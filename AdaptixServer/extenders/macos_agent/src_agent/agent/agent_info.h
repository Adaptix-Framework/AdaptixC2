#ifndef AGENT_INFO_H
#define AGENT_INFO_H

#include "msgpack.h"

/// Build SessionInfo msgpack payload matching Go's utils.SessionInfo struct
/// Also generates a random 16-byte session encryption key
///
/// msgpack keys (alphabetical order, matching Go vmihailenco/msgpack):
///   acp, elevated, encrypt_key, host, ipaddr, oem, os, os_version, pid, process, user
///
/// Returns 0 on success, fills session_key (16 bytes)
int create_session_info(mp_writer_t* w, uint8_t* session_key);

#endif // AGENT_INFO_H
