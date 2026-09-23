#include "server_init.h"
#include "epoll_manager/epoll_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>

int server_boot(server_ctx_t *ctx, server_config_t *config) {
    ctx->max_clients = config->max_clients;
    ctx->max_workers = config->max_workers;

    // 1. ALLOCATE SHARED MEMORY PIPELINES PRIOR TO NETWORK SPIN-UP
    // Allocate exactly 1 media chunk (64KB) per allowed concurrent client slot
    ctx->chunks_pool = shm_chunks_create("/gallery_chunks", ctx->max_clients);
    if (!ctx->chunks_pool) {
        fprintf(stderr, "[C-ERROR] Critical: Failed to allocate Shared Memory Chunks Pool.\n");
        return -1;
    }
    printf("[C-SERVER] SHM Media Chunk Pool initialized successfully (%d slots).\n", ctx->max_clients);

    // Instantiate hardware lock-free ACL table with appropriate sizing index (e.g., 1024 slots)
    ctx->acl_table = shm_acl_create("/gallery_acl", config->max_clients * config->max_viewers);
    if (!ctx->acl_table) {
        fprintf(stderr, "[C-ERROR] Critical: Failed to allocate Lock-free SHM ACL Table.\n");
        shm_chunks_destroy(ctx->chunks_pool);
        return -1;
    }
    printf("[C-SERVER] Lock-free SHM Access Control List (ACL) table online.\n");

    // 2. ALLOCATE THE STATIC REGISTRY MEMORY LOOP POOL
    // Total connections = Clients + Workers + Network Listener + IPC Listener
    int max_connections = ctx->max_clients + ctx->max_workers + 2;
    ctx->registry = registry_create(max_connections);
    if (!ctx->registry) {
        fprintf(stderr, "[C-ERROR] Critical: Failed to allocate Connection Memory Pool.\n");
        shm_acl_destroy(ctx->acl_table);
        shm_chunks_destroy(ctx->chunks_pool);
        return -1;
    }
    printf("[C-SERVER] Connection pool for %d contexts allocated successfully.\n", max_connections);

    ctx->ipc_listen_fd = -1;
    ctx->net_listen_fd = -1;
    
    // 3. BIND INTERFACES AND ASSEMBLE EPOLL DESCRIPTORS VIA MANAGERS
    ctx->epoll_fd = epoll_manager_init(config, &ctx->ipc_listen_fd, &ctx->net_listen_fd);
    if (ctx->epoll_fd == -1) {
        fprintf(stderr, "[C-ERROR] Device/Socket manager initialization failed.\n");
        registry_destroy(ctx->registry);
        shm_acl_destroy(ctx->acl_table);
        shm_chunks_destroy(ctx->chunks_pool);
        return -1;
    }

    // Capture vacant slots for core background tracking
    conn_context_t *ipc_listen_ctx = registry_pop_free(ctx->registry, ctx->ipc_listen_fd, CONN_TYPE_IPC_LISTEN);
    conn_context_t *net_listen_ctx = registry_pop_free(ctx->registry, ctx->net_listen_fd, CONN_TYPE_HTTP_LISTEN);

    if (!ipc_listen_ctx || !net_listen_ctx) {
        fprintf(stderr, "[C-ERROR] Critical: Pool exhausted during system listeners instantiation.\n");
        goto fail_listeners;
    }

    struct epoll_event ev;
    
    // Register IPC Unix listener for incoming Python microservices
    ev.events = EPOLLIN | EPOLLET;
    ev.data.ptr = ipc_listen_ctx;
    if (epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, ctx->ipc_listen_fd, &ev) == -1) {
        perror("[C-ERROR] epoll_ctl failed to bind IPC listener");
        goto fail_listeners;
    }

    // Register Network HTTP socket interface for remote client uploads
    ev.data.ptr = net_listen_ctx;
    if (epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, ctx->net_listen_fd, &ev) == -1) {
        perror("[C-ERROR] epoll_ctl failed to bind HTTP listener");
        goto fail_listeners;
    }

    printf("[C-SERVER] Edge-Triggered epoll core is online. Channels bound.\n");
    return 0;

fail_listeners:
    close(ctx->epoll_fd);
    registry_destroy(ctx->registry);
    shm_acl_destroy(ctx->acl_table);
    shm_chunks_destroy(ctx->chunks_pool);
    return -1;
}

void server_shutdown(server_ctx_t *ctx, server_config_t *config) {
    printf("[C-SERVER] Terminating system pipelines and freeing memory loops...\n");
    
    // Release the main monolithic descriptor connection manager context
    if (ctx->registry) {
        registry_destroy(ctx->registry);
    }
    
    // Safely unmap and erase virtual shared hardware table contexts
    if (ctx->acl_table) {
        shm_acl_destroy(ctx->acl_table);
    }
    if (ctx->chunks_pool) {
        shm_chunks_destroy(ctx->chunks_pool);
    }
    
    // Disconnect kernel polling hooks
    if (ctx->epoll_fd != -1) {
        close(ctx->epoll_fd);
    }
    
    // Unlink file boundaries for clean consecutive launches
    if (config && config->socket_path[0] != '\0') {
        unlink(config->socket_path);
    }
    
    printf("[C-SERVER] Cleanup completed safely. Exiting process scope.\n");
}
