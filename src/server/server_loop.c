#include "server_init.h"
#include "connection/connection.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <errno.h>

#define MAX_EVENTS 64

// Prototypes for internal event micro-handlers (implemented step-by-step)
void handle_ipc_accept(server_ctx_t *ctx);
void handle_worker_handshake(server_ctx_t *ctx, conn_context_t *slot);
void handle_net_accept(server_ctx_t *ctx);
void handle_client_request(server_ctx_t *ctx, conn_context_t *slot, uint32_t revents);
void handle_active_worker_response(server_ctx_t *ctx, conn_context_t *slot, uint32_t revents);

/**
 * Main asynchronous event engine layer. Spins the core epoll cycle.
 */
void server_loop(server_ctx_t *ctx) {
    struct epoll_event events[MAX_EVENTS];
    
    printf("[C-SERVER] Asynchronous kernel is spinning up. Ready for incoming traffic.\n");

    while (1) {
        // Fetch a batch of pending I/O events from the edge-triggered system core.
        // Timeout is set to -1 (block indefinitely until an event arrives).
        int nfds = epoll_wait(ctx->epoll_fd, events, MAX_EVENTS, 100);
        
        if (nfds == -1) {
            // If interrupted by an OS signal (e.g., SIGINT / Ctrl+C), safely resume the loop
            if (errno == EINTR) {
                continue;
            }
            // Critical OS kernel interface failure
            perror("[C-ERROR] Fatal crash encountered during epoll_wait cycle loop execution");
            break;
        }

        // Loop over the triggered batch of synchronous descriptors
        for (int i = 0; i < nfds; i++) {
            conn_context_t *slot = (conn_context_t *)events[i].data.ptr;
            uint32_t revents = events[i].events;

            // Guard against closed, cleaned or untracked descriptors during hot-path execution
            if (!slot || slot->fd == -1) {
                continue;
            }

            // Core State Machine Router based on the physical socket role
            switch (slot->type) {
                case CONN_TYPE_IPC_LISTEN:
                    // A new local Python worker microservice process is trying to connect
                    if (revents & EPOLLIN) {
                        handle_ipc_accept(ctx);
                    }
                    break;

                case CONN_TYPE_IPC_HANDSHAKE:
                    // A connected worker sent its initial 8-byte authentication profile
                    if (revents & EPOLLIN) {
                        handle_worker_handshake(ctx, slot);
                    }
                    break;

                case CONN_TYPE_AUTH_WORKER:
                case CONN_TYPE_RW_WORKER:
                case CONN_TYPE_TRACKER_WORKER:
                    // An officially promoted python backend worker sent a command or verification ACK
                    handle_active_worker_response(ctx, slot, revents);
                    break;

                case CONN_TYPE_HTTP_LISTEN:
                    // A remote network client (browser/mobile app) is knocking on port 8080
                    if (revents & EPOLLIN) {
                        handle_net_accept(ctx);
                    }
                    break;

                case CONN_TYPE_CLIENT:
                    // An active network user is uploading/downloading or waiting for SSE updates
                    handle_client_request(ctx, slot, revents);
                    break;

                default:
                    fprintf(stderr, "[C-WARNING] Encounted unroutable type descriptor context: %d. Dropping.\n", slot->type);
                    break;
            }
        }

        // Amortized chunked SSE broadcast tasks will execute right here (Step 11)
    }

    printf("[C-SERVER] Asynchronous execution engine has terminated process scope loop.\n");
}
