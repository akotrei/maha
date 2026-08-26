#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// Supported media types in the system
typedef enum {
    MEDIA_TYPE_UNKNOWN = 0,
    MEDIA_TYPE_JPG     = 1,
    MEDIA_TYPE_PNG     = 2,
    MEDIA_TYPE_MP4     = 3,
    MEDIA_TYPE_MOV     = 4
} media_type_t;

typedef enum {
    CMD_WRITE_CHUNK = 1,  // Write a new chunk to the file
    CMD_ACK         = 2,  // Chunk successfully written to disk by worker
    CMD_FINAL_CHUNK = 3,  // Final chunk of the file (signal to close the file)
    CMD_ABORT       = 4   // Abort writing, delete temporary file / clean resources
    CMD_DELETE_FILE = 5   // Signal to delete a file from disk
} ipc_cmd_t;

typedef struct __attribute__((packed)) {
    int32_t  type;        // Command type from ipc_cmd_t (4 bytes)
    int32_t  slot_id;     // Slot ID in Shared Memory (4 bytes)
    int32_t  data_size;   // Number of bytes to write in this chunk (4 bytes)
    int32_t  media_type;  // File extension type from media_type_t (4 bytes)
    uint64_t owner_id;    // Storage owner identifier / folder name (8 bytes)
    uint64_t file_id;     // Unique file identifier (8 bytes)
} ipc_msg_t;              // Total size: 32 bytes (perfectly aligned for 64-bit systems)

#endif // PROTOCOL_H
