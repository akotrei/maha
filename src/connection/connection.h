#ifndef CONNECTION_H
#define CONNECTION_H

#include <stdint.h>
#include <stddef.h>

#define HTTP_BUF_SIZE 4096

// Root dispatcher: What physical role does this socket perform?
typedef enum {
    CONN_TYPE_HTTP_LISTEN,      // Main socket listening for external HTTP clients
    CONN_TYPE_IPC_LISTEN,       // Unix socket listening for local Python workers
    CONN_TYPE_CLIENT,           // Active external network user (browser, app)
    CONN_TYPE_AUTH_WORKER,      // Active connection to auth_service.py
    CONN_TYPE_RW_WORKER,        // Active connection to rw_worker.py
    CONN_TYPE_TRACKER_WORKER,    // Active connection to tracker.py
    CONN_TYPE_IPC_HANDSHAKE     // Transit state while reading initial worker handshake packet
} conn_type_t;

// States exclusively for listener sockets (HTTP & IPC)
typedef enum {
    CONN_STATE_LISTENING = 0
} listen_state_t;

// States exclusively for external network clients (CONN_TYPE_CLIENT)
typedef enum {
    CONN_STATE_CLIENT_READ_HEADERS,     // Accumulating HTTP headers in buffer
    CONN_STATE_CLIENT_PROXY_TO_AUTH,    // Waiting for Python auth_service to finish log/reg
    CONN_STATE_CLIENT_FETCHING_META,    // Waiting for Python tracker to return actual metadata
    CONN_STATE_CLIENT_STREAM_UPLOAD,    // Streaming media bytes from net into shm_chunks
    CONN_STATE_CLIENT_STREAM_DOWNLOAD,  // Writing media bytes from file/shm back to net
    CONN_STATE_CLIENT_SSE_SUBSCRIBER    // Dormant state, waiting for reactive storage updates
} client_state_t;

// States exclusively for internal Python backend workers
typedef enum {
    CONN_STATE_WORKER_IDLE,       // Worker is free and waiting for incoming C-server tasks
    CONN_STATE_WORKER_BUSY_AUTH,  // Auth worker is currently processing registration/login
    CONN_STATE_WORKER_BUSY_WRITE, // RW worker is currently flushing a 64KB chunk to NVMe disk
    CONN_STATE_WORKER_BUSY_META,  // Tracker worker is searching/building metadata JSON response
    CONN_STATE_WORKER_SSE_STREAM  // Active event stream pipelined from tracker.py
} worker_state_t;

// Unified connection slot structure mapping the state machine layout
typedef struct {
    int fd;                     // OS file descriptor descriptor (-1 if slot is unallocated)
    conn_type_t type;           // Dispatch type to safely navigate the union branches

    union {
        // Dedicated branch for listening contexts
        struct {
            listen_state_t state;
        } listener;

        // Dedicated branch for active clients (takes ~4.1 KB due to buffer)
        struct {
            client_state_t state;
            int peer_idx;             // Index of the paired python worker handling the request
            int slot_id;              // Allocated index in shm_chunks (-1 if none)
            int bytes_in_slot;        // Total bytes written into current 64KB shm block
            
            char buf[HTTP_BUF_SIZE];  // Network accumulation buffer for Edge-Triggered epoll
            int buf_len;              // Current size of accumulated raw HTTP data
            
            uint64_t visitor_id;      // Extracted business context metrics
            uint64_t owner_id;
            uint64_t file_id;
            int32_t  media_type;      // Maps to media_type_t from protocol.h

            int next_sse_idx;         // The first one on a single SSE group in registry_t
            int prev_sse_idx;         // The last one on a single SSE group in registry_t
        } client;

        // Dedicated branch for internal Python service hooks
        struct {
            worker_state_t state;
            int peer_idx;             // Index of the target network client currently being served

            // Asynchronous handshake accumulation buffer (used during CONN_TYPE_IPC_HANDSHAKE)
            uint8_t handshake_buf[8]; // Raw container for holding the 8-byte initial packet
            int32_t  handshake_bytes;  // Track exactly how many bytes have been aggregated so far
        } worker;

        // Link node utilized only when the current slot sits free inside the pool
        int next_free_idx;            // LIFO Free List pointer index
    } as;
} conn_context_t;

// Contiguous pool structure managing connection allocation allocation without malloc friction
typedef struct {
    conn_context_t *slots;     // Array of pre-allocated structured connection contexts
    int head_free_idx;         // Index pointing to the head of the LIFO Free List
    int size;                  // Maximum capacity threshold (MAX_CONNECTIONS limit)
} registry_t;

/**
 * Creates and formats a static connection pool array in the heap space.
 * @param size Maximum parallel connections (MAX_CONNECTIONS allocation count)
 */
registry_t* registry_create(int size);

/**
 * Frees all local structures and closes open descriptors cleanly.
 */
void registry_destroy(registry_t *reg);

/**
 * Extracts a blank socket context slot from the pool. Lock-free/O(1) list pop.
 * @return Connection pointer context or NULL if pool is completely saturated.
 */
conn_context_t* registry_pop_free(registry_t *reg, int fd, conn_type_t type);

/**
 * Recycles an active socket slot back into the LIFO Free List loop. O(1) push.
 */
void registry_push_free(registry_t *reg, conn_context_t *slot);

/**
 * Direct abstraction helper ensuring sockets match mandatory O_NONBLOCK configuration for ET epoll.
 */
int conn_set_nonblocking(int fd);

#endif // CONNECTION_H
