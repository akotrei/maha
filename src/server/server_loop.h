#ifndef SERVER_LOOP_H
#define SERVER_LOOP_H

#include "server_init.h"

/**
 * Core asynchronous server engine execution layer. Spins the epoll_wait cycle.
 */
void server_loop(server_ctx_t *ctx);

#endif // SERVER_LOOP_H