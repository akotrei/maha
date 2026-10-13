#include "ipc_handlers.h"
#include "server_init.h"
#include "connection/connection.h"
#include "server/worker_handshake.h"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

/**
 * Edge-Triggered accept handler for incoming local Python workers.
 * Loops accept4 until EAGAIN to exhaust the OS incoming connection queue.
 */
void handle_ipc_accept(server_ctx_t *ctx) {
    while (1) {
        // Atomically accept connection, set non-blocking flag and enable close-on-exec
        int worker_fd = accept4(ctx->ipc_listen_fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
        
        if (worker_fd == -1) {
            // In Edge-Triggered mode, we must loop until accept4 signals resource exhaustion
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; 
            }
            perror("[C-ERROR] accept4 failed during incoming IPC connection extraction");
            break;
        }

        // Extract a clean pre-allocated slot from our O(1) LIFO connection memory pool
        // We brand it with CONN_TYPE_IPC_HANDSHAKE transit type for protocol isolation
        conn_context_t *slot = registry_pop_free(ctx->registry, worker_fd, CONN_TYPE_IPC_HANDSHAKE);
        if (!slot) {
            fprintf(stderr, "[C-ERROR] Connection registry pool saturated! Dropping upcoming worker connection.\n");
            close(worker_fd);
            continue;
        }

        // Initialize transit variables for tracking the gradual accumulation of the 8-byte frame
        slot->as.worker.handshake_bytes = 0;
        memset(slot->as.worker.handshake_buf, 0, sizeof(slot->as.worker.handshake_buf));

        // Register the new socket inside epoll instance for Edge-Triggered reading.
        // EPOLLONESHOT guarantees that no other thread handles this handshake concurrently.
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
        ev.data.ptr = slot;

        if (epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, worker_fd, &ev) == -1) {
            perror("[C-ERROR] epoll_ctl ADD failed while anchoring temporary worker to epoll");
            registry_push_free(ctx->registry, slot); // Safely recycle memory slot back to LIFO pool
            continue;
        }

        printf("[C-IPC] Registered unverified worker connection on fd %d. Awaiting protocol handshake.\n", worker_fd);
    }
}

/**
 * Asynchronous handshake manager. Accumulates exactly 8 bytes from the transit 
 * socket, validates the magic signature, and promotes the worker to its active role.
 */
void handle_worker_handshake(server_ctx_t *ctx, conn_context_t *slot) {
    int fd = slot->fd;
    size_t target_len = sizeof(ipc_handshake_pkt_t); // Exactly 8 bytes to form a clean block

    // Read available bytes incrementally to handle rare fragmented network packets over ET epoll
    while (slot->as.worker.handshake_bytes < (int32_t)target_len) {
        size_t bytes_left = target_len - slot->as.worker.handshake_bytes;
        ssize_t n = read(fd, slot->as.worker.handshake_buf + slot->as.worker.handshake_bytes, bytes_left);

        if (n > 0) {
            slot->as.worker.handshake_bytes += n;
        } else if (n == -1) {
            // If the kernel buffer is fully drained, re-arm the descriptor inside epoll and return
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                struct epoll_event ev;
                ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                ev.data.ptr = slot;
                epoll_ctl(ctx->epoll_fd, EPOLL_CTL_MOD, fd, &ev);
                return;
            }
            // Real physical read error encountered on the socket line
            fprintf(stderr, "[C-ERROR] Read error during worker handshake validation on fd %d. Dropping.\n", fd);
            registry_push_free(ctx->registry, slot);
            return;
        } else {
            // Worker process disconnected or closed the connection prematurely (EOF)
            fprintf(stderr, "[C-IPC] Worker closed communication pipeline prematurely on fd %d.\n", fd);
            registry_push_free(ctx->registry, slot);
            return;
        }
    }

    // Cast the accumulated buffer back into our packed protocol structure for syntax validation
    ipc_handshake_pkt_t *pkt = (ipc_handshake_pkt_t *)slot->as.worker.handshake_buf;

    // SECURITY ANCHOR: Validate the protocol signature to reject third-party clutter
    if (pkt->magic != IPC_HANDSHAKE_MAGIC) {
        fprintf(stderr, "[C-SECURITY] Critical: Invalid handshake signature (0x%X) on fd %d! Connection severed.\n", pkt->magic, fd);
        registry_push_free(ctx->registry, slot);
        return;
    }

    // Role verification: ensure the Python backend claimed an existing functional type
    if (pkt->worker_type != CONN_TYPE_AUTH_WORKER && 
        pkt->worker_type != CONN_TYPE_RW_WORKER && 
        pkt->worker_type != CONN_TYPE_TRACKER_WORKER) {
        fprintf(stderr, "[C-ERROR] Worker on fd %d declared unregistered role type enum: %d. Dropping.\n", fd, pkt->worker_type);
        registry_push_free(ctx->registry, slot);
        return;
    }

    // PROTOCOL PROMOTION: Upgrade the slot from quarantine transit type to an active operational role
    slot->type = pkt->worker_type;
    slot->as.worker.state = CONN_STATE_WORKER_IDLE; // Worker enters the pool as free and ready
    slot->as.worker.peer_idx = -1;

    // Re-register into epoll for production mode: permanent Edge-Triggered reading without ONESHOT
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.ptr = slot;

    if (epoll_ctl(ctx->epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
        perror("[C-ERROR] epoll_ctl MOD failed during promoting worker to active production pool");
        registry_push_free(ctx->registry, slot);
        return;
    }

    const char* type_str = (slot->type == CONN_TYPE_AUTH_WORKER) ? "AUTH" :
                           (slot->type == CONN_TYPE_RW_WORKER)   ? "READ/WRITE" : "TRACKER";

    printf("[C-SERVER] Worker on fd %d successfully verified! Role allocated: [%s].\n", fd, type_str);
}
