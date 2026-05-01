#ifndef COMMANDER_H
#define COMMANDER_H

#include "types.h"
#include "msgpack.h"

/// Process a list of commands received from the server
/// Input: array of msgpack-encoded Command structs
/// Output: array of msgpack-encoded response buffers
///
/// Each Command has: {code: uint, id: uint, data: []byte}
/// Response format depends on the command code

// Process all commands from inMessage.Object
// Returns msgpack-encoded array of response buffers
// Caller must free the returned buffer
int process_commands(const uint8_t** commands, uint32_t* cmd_sizes,
                     uint32_t cmd_count,
                     buffer_t* out_responses, uint32_t* out_count);

// Process a single command, write response to writer
// Returns 0 on success
int handle_command(uint32_t code, uint32_t cmd_id,
                   const uint8_t* data, uint32_t data_len,
                   mp_writer_t* response);

#endif // COMMANDER_H
