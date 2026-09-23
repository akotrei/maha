#ifndef IPC_HANDLERS_H
#define IPC_HANDLERS_H

#include "server/server_init.h"
#include "connection/connection.h"

/**
 * Edge-Triggered accept handler for incoming local Python workers.
 * Loops accept4 until EAGAIN to exhaust the incoming connection queue.
 */
void handle_ipc_accept(server_ctx_t *ctx);

/**
 * Asynchronous handshake manager. Accumulates exactly 8 bytes from the transit 
 * socket, validates the magic signature, and promotes the worker to its active role.
 */
void handle_worker_handshake(server_ctx_t *ctx, conn_context_t *slot);

/**
 * Handles incoming events (read/error/close) from verified operational workers.
 */
void handle_active_worker_response(server_ctx_t *ctx, conn_context_t *slot, uint32_t revents);

#endif // IPC_HANDLERS_H
