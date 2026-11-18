#ifndef NET_HANDLERS_H
#define NET_HANDLERS_H

#include "server_init.h"
#include "connection/connection.h"

// The maximum number of concurrent HTTP accept operations processed per single epoll loop iteration
#define NET_ACCEPT_QUOTA 16

/**
 * Amortized network accept handler for incoming remote HTTP clients.
 * Extracts sockets in non-blocking mode up to NET_ACCEPT_QUOTA bounds 
 * to maintain strict processing fairness across existing active connections.
 * 
 * @param ctx Global server runtime environment context primitives anchor.
 */
void handle_net_accept(server_ctx_t *ctx);

/**
 * Main state machine dispatcher for network client activities.
 * Examines current connection phase (Headers parsing, SHM streaming, etc.) 
 * and routes execution to appropriate reactive non-blocking pipelines.
 * 
 * @param ctx     Global server runtime environment context primitives anchor.
 * @param slot    Pre-allocated monolithic context index mapping this specific client session.
 * @param revents Bitmask containing the triggered hardware events returned by the epoll layer.
 */
void handle_client_request(server_ctx_t *ctx, conn_context_t *slot, uint32_t revents);

#endif // NET_HANDLERS_H
