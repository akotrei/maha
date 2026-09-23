#ifndef SERVER_INIT_H
#define SERVER_INIT_H

#include "config/config.h"
#include "connection/connection.h"
#include "shm/shm_chunks.h"
#include "shm/shm_acl.h"

// Context structure holding vital runtime primitives of the server
typedef struct {
    int epoll_fd;
    int net_listen_fd;
    int ipc_listen_fd;
    int max_clients;
    int max_workers;
    registry_t *registry;
    
    // Shared Memory components initialized prior to the server loop sequence
    shm_chunks_t *chunks_pool;      // Static 64KB memory chunk manager for media streaming
    shm_acl_t    *acl_table;        // Lock-free atomic hardware table for user permissions
} server_ctx_t;

/**
 * Orchestrates memory allocation, SHM segment mapping, and socket preparation up to loop entry.
 * @return 0 on successful setup, -1 on failure.
 */
int server_boot(server_ctx_t *ctx, server_config_t *config);

/**
 * Safely tear down all allocations, descriptors, SHM mapping regions, and system artifacts.
 */
void server_shutdown(server_ctx_t *ctx, server_config_t *config);

#endif // SERVER_INIT_H
