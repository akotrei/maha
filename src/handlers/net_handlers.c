#include "net_handlers.h"
#include "connection/connection.h"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

// Forward declarations for internal client state micro-handlers
static void client_phase_read_headers(server_ctx_t *ctx, conn_context_t *slot);
static void client_phase_stream_upload(server_ctx_t *ctx, conn_context_t *slot);
static void client_phase_stream_download(server_ctx_t *ctx, conn_context_t *slot);
static void handle_client_disconnect(server_ctx_t *ctx, conn_context_t *slot, const char *reason);

/**
 * Amortized network accept handler for incoming remote HTTP clients.
 * Extracts sockets in non-blocking mode up to NET_ACCEPT_QUOTA bounds.
 */
void handle_net_accept(server_ctx_t *ctx) {
    int quota = NET_ACCEPT_QUOTA;
    
    // We assume ctx->net_listen_saturated exists or will be added to server_ctx_t
    // to manage the timeout = 0 fast-track rotation for remaining clients.
    // Setting it to 0 at the start of the accept check chunk.
    // (We will touch server_ctx_t later if needed, for now logic is isolated)

    while (quota > 0) {
        // Atomically accept incoming client, enforce O_NONBLOCK and close-on-exec definitions
        int client_fd = accept4(ctx->net_listen_fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
        
        if (client_fd == -1) {
            // If the OS incoming queue is fully drained, break gracefully from the loop
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                ctx->net_listen_saturated = 0;
                break;
            }
            perror("[C-ERROR] accept4 failed during client connection extraction");
            break;
        }

        // Pop a clean memory slot from our O(1) static LIFO registry pool
        // This automatically sets the base client state to CONN_STATE_CLIENT_READ_HEADERS
        conn_context_t *slot = registry_pop_free(ctx->registry, client_fd, CONN_TYPE_CLIENT);
        if (!slot) {
            fprintf(stderr, "[C-ERROR] Network connection pool saturated! Dropping remote client.\n");
            close(client_fd);
            continue;
        }

        // Initialize state indicators inside the extracted context slot
        slot->as.client.buf_len = 0;
        memset(slot->as.client.buf, 0, sizeof(slot->as.client.buf));
        slot->as.client.next_sse_idx = -1;
        slot->as.client.prev_sse_idx = -1;

        // Register client into epoll instance with strict isolation bounds.
        // EPOLLONESHOT isolates the socket during header parsing and state modifications.
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
        ev.data.ptr = slot;

        if (epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
            perror("[C-ERROR] epoll_ctl ADD failed while anchoring new remote client");
            registry_push_free(ctx->registry, slot); // Recycles slot back to the free list
            continue;
        }

        quota--;
    }

    // Quick evaluation anchor: if quota reached 0, there might be more clients waiting.
    // In future loop aggregation, this can toggle the timeout shift.
    if (quota == 0) {
        // Saturated boundary reached. Inform the system loop to skip deep sleep.
        ctx->net_listen_saturated = 1;
    }
}

/**
 * Main state machine dispatcher for active remote network HTTP users.
 * Routes execution flow based on the current isolated session phase.
 */
void handle_client_request(server_ctx_t *ctx, conn_context_t *slot, uint32_t revents) {
    int fd = slot->fd;

    // SECURITY & LINE GUARD: Handle abrupt disconnections, socket errors, or half-closed lines
    if (revents & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
        // EPOLLRDHUP is exceptionally beautiful for clean SSE client unsubscribe detection
        handle_client_disconnect(ctx, slot, "Remote peer disconnected or line broken");
        return;
    }

    // Process the active client based on its designated state machine position
    switch (slot->as.client.state) {
        case CONN_STATE_CLIENT_READ_HEADERS:
            // Client is transmitting raw HTTP request text headers
            if (revents & EPOLLIN) {
                client_phase_read_headers(ctx, slot);
            }
            break;

        case CONN_STATE_CLIENT_PROXY_TO_AUTH:
        case CONN_STATE_CLIENT_FETCHING_META:
            // In these states, the client socket is completely dormant in epoll loop.
            // It is waiting for the respective Python worker to reply, so EPOLLIN here is unexpected.
            fprintf(stderr, "[C-WARNING] Phantom activity on fd %d while waiting for Python backend response.\n", fd);
            break;

        case CONN_STATE_CLIENT_STREAM_UPLOAD:
            // Hot I/O path: client is actively streaming multimedia binary payload bytes into SHM
            if (revents & EPOLLIN) {
                client_phase_stream_upload(ctx, slot);
            }
            break;

        case CONN_STATE_CLIENT_STREAM_DOWNLOAD:
            // Hot I/O path: server is pumping file data back to the network line
            if (revents & EPOLLOUT) {
                client_phase_stream_download(ctx, slot);
            }
            break;

        case CONN_STATE_CLIENT_SSE_SUBSCRIBER:
            // SSE clients sleep indefinitely awaiting reactive server push broadcasts.
            // Since we only monitor EPOLLRDHUP for them, active EPOLLIN here indicates an anomaly or bad request data.
            if (revents & EPOLLIN) {
                handle_client_disconnect(ctx, slot, "SSE subscriber sent unexpected upstream data");
            }
            break;

        default:
            fprintf(stderr, "[C-ERROR] Client on fd %d is in an unmanageable state: %d. Emergency purge.\n", fd, slot->as.client.state);
            handle_client_disconnect(ctx, slot, "Unroutable internal state exception");
            break;
    }
}

// --- PLACEHOLDER IMPLEMENTATIONS FOR STATE MICRO-HANDLERS ---

static void client_phase_read_headers(server_ctx_t *ctx, conn_context_t *slot) {
    // TODO: Step 7 - Implement non-blocking accumulation and parsing of HTTP headers with \r\n\r\n bounds
    (void)ctx; (void)slot;
}

static void client_phase_stream_upload(server_ctx_t *ctx, conn_context_t *slot) {
    // TODO: Step 9 - Implement chunked SHM streaming and dispatching commands to rw_worker.py
    (void)ctx; (void)slot;
}

static void client_phase_stream_download(server_ctx_t *ctx, conn_context_t *slot) {
    // TODO: Future Step - Implement non-blocking file streaming download to client
    (void)ctx; (void)slot;
}

static void handle_client_disconnect(server_ctx_t *ctx, conn_context_t *slot, const char *reason) {
    printf("[C-CLIENT] Purging connection on fd %d. Reason: %s\n", slot->fd, reason);
    
    // Safely recycle any chunks mapped to this client back into the LIFO memory loops pool
    if (slot->as.client.slot_id != -1) {
        shm_chunks_push_free_chunk(ctx->chunks_pool, slot->as.client.slot_id);
        slot->as.client.slot_id = -1;
    }

    // If the client was sleeping inside an intrusive SSE group chain, cut it out cleanly here (O(1))
    // (We will write the intrusive list link extraction macros/logic when implementing step 11)

    // Wipe memory cleanly to avoid session contamination and release the OS file descriptor
    registry_push_free(ctx->registry, slot);
}

