#ifndef HANDSHAKE_H
#define HANDSHAKE_H

#include <stdint.h>

#define IPC_HANDSHAKE_MAGIC 0x47414C59 // Explicit ASCII signature for 'GALY' (Gallery Engine)

/**
 * Packed binary layout dispatched by Python microservices immediately 
 * upon initiating a connection to the primary C Unix socket domain.
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;         // Validation anchor: must equal IPC_HANDSHAKE_MAGIC
    uint8_t  worker_type;   // Target role maps to conn_type_t (AUTH=3, RW=4, TRACKER=5)
    uint8_t  reserved[3];   // Perfect padding to guarantee 8-byte boundary alignment
} ipc_handshake_pkt_t;

#endif // HANDSHAKE_H
