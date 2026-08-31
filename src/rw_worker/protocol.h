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

// Unified command types driving the asynchronous IPC worker pipeline
typedef enum {
    // --- RW_WORKER DISK COMMANDS (Disk I/O) ---
    CMD_RW_WRITE_CHUNK = 1,  // Write a new chunk to the file
    CMD_RW_ACK         = 2,  // Chunk successfully written to disk by worker
    CMD_RW_FINAL_CHUNK = 3,  // Final chunk of the file (signal to close the file)
    CMD_RW_ABORT       = 4,  // Abort writing, delete temporary file / clean resources
    CMD_RW_DELETE_FILE = 5,  // Signal to delete a file from disk

    // --- AUTH_WORKER COMMANDS (Authentication Service) ---
    CMD_AUTH_REQUEST   = 6,  // C-Server sends credentials payload to auth_service.py
    CMD_AUTH_RESPONSE  = 7,  // Auth_service.py returns operational status and owner_id tokens

    // --- TRACKER_WORKER COMMANDS (Metadata & Reactive Events) ---
    CMD_TRK_REQ_METADATA = 8, // C-Server requests latest JSON snapshot from tracker.py
    CMD_TRK_META_UPDATED = 9  // Tracker.py signals DB change to initiate amortized SSE broadcast
} ipc_cmd_t;

// Monolithic 32-byte network message frame container (perfectly aligned for 64-bit bounds)
typedef struct __attribute__((packed)) {
    int32_t  type;        // Command dispatch token mapped via ipc_cmd_t (4 bytes)
    int32_t  slot_id;     // Slot ID in Shared Memory or Registry socket handle index (4 bytes)
    int32_t  data_size;   // Exact payload size boundary to digest (4 bytes)
    int32_t  media_type;  // Operational status code or file extension format via media_type_t (4 bytes)
    uint64_t owner_id;    // Global security entity cluster token / user identifier (8 bytes)
    uint64_t file_id;     // Unique target object reference/session key identifier (8 bytes)
} ipc_msg_t;              // Total structural size: Exactly 32 bytes

#endif // PROTOCOL_H
