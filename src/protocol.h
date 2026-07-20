#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>


typedef enum {
    CMD_WRITE_CHUNK = 1,  // we want to write a new chunk
    CMD_ACK         = 2,  // a chunk is alredy written to file
    CMD_FINAL_CHUNK = 3,  // we send a final chunk of a file
    CMD_ABORT       = 4   // a user is willing to abort writing a file
} ipc_cmd_t;

typedef struct __attribute__((packed)) {
    int32_t type;       // from ipc_cmd_t
    int32_t slot_id;    // a slot id in Shared Memory
    int32_t data_size;  // number of bytes to write
} ipc_msg_t;

#endif // PROTOCOL_H
