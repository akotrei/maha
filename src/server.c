#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>

#include "config.h"
#include "epoll_manager.h"
#include "connection.h"


#define MAX_EVENTS 64


int main(int argc, char** argv) {
    setbuf(stdout, NULL);

    server_config_t config;
    parse_arguments(argc, argv, &config);
    printf("[C-SERVER] Initialisation is successful!\n");

    int max_connections = config.max_clients + config.max_workers + 2;
    conn_context_t *registry = calloc(max_connections, sizeof(conn_context_t));
    if (!registry) {
        printf("[C-ERROR] Critical: Failed to allocate Memory Pool for connections.\n");
        return 1;
    }
    for (int i = 0; i < max_connections; i++) {
        registry[i].fd = -1;
        registry[i].slot_id = -1;
    }
    printf("[C-SERVER] Memory Pool for %d connections successfully allocated.\n", max_connections);

    int ipc_fd = -1;
    int net_fd = -1;
    int epoll_fd = epoll_manager_init(&config, &ipc_fd, &net_fd);
    if (epoll_fd == -1) {
        printf("[C-ERROR] Server initialization failed. Exiting.\n");
        free(registry);
        return 1;
    }

    conn_context_t *ipc_listen_ctx = &registry[0];
    ipc_listen_ctx->fd = ipc_fd;
    ipc_listen_ctx->type = CONN_TYPE_IPC_LISTEN;

    conn_context_t *net_listen_ctx = &registry[1];
    net_listen_ctx->fd = net_fd;
    net_listen_ctx->type = CONN_TYPE_NETWORK_LISTEN;

    // Bind to epoll
    struct epoll_event ev;
    struct epoll_event events[MAX_EVENTS];

    ev.events = EPOLLIN | EPOLLET;
    ev.data.ptr = ipc_listen_ctx;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ipc_fd, &ev);

    ev.data.ptr = net_listen_ctx;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, net_fd, &ev);

    printf("[C-SERVER] Server is ready. Both IPC and Network sockets are up.\n");
    printf("[C-SERVER] Entering main asynchronous loop...\n");


    // ==========================================
    // MAIN ASYNCHRONIOUS SERVER LOOP
    // ==========================================
    while (1) {
        // Sleep max 500 ms
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, 500);

        if (num_events < 0) {
            if (errno == EINTR) {
                continue; 
            }
            perror("[C-ERROR] epoll_wait critical error");
            break;
        }

        if (num_events == 0) {
            printf("."); 
            continue;
        }

        for (int i = 0; i < num_events; i++) {
            conn_context_t *ctx = (conn_context_t *)events[i].data.ptr;
            printf("[C-SERVER] New event\n");
            if (!ctx) continue;

            // Сюда мы шаг за шагом встроим обработку switch(ctx->type)
        }
    }

    printf("[C-SERVER] Shutting down and cleaning memory pool...\n");
    
    for (int i = 0; i < max_connections; i++) {
        if (registry[i].fd != -1) {
            close(registry[i].fd);
        }
    }
    
    free(registry);
    close(epoll_fd);
    unlink(config.socket_path);
    
    printf("[C-SERVER] Shutdown successful. Goodbye!\n");
    return 0;
}
