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

    int max_clients = config.max_clients;
    int max_workers = config.max_workers;

    int max_connections = max_clients + max_workers + 2;
    registry_t *registry = registry_create(max_connections);
    if (!registry) {
        printf("[C-ERROR] Critical: Failed to allocate Memory Pool for connections.\n");
        return 1;
    }
    printf("[C-SERVER] Memory Pool for %d connections successfully allocated.\n", max_connections);

    int ipc_fd = -1;
    int net_fd = -1;
    int epoll_fd = epoll_manager_init(&config, &ipc_fd, &net_fd);
    if (epoll_fd == -1) {
        printf("[C-ERROR] Server initialization failed. Exiting.\n");
        registry_destroy(registry);
        return 1;
    }

    conn_context_t *ipc_listen_ctx = registry_pop_free(registry, ipc_fd, CONN_TYPE_IPC_LISTEN);
    conn_context_t *net_listen_ctx = registry_pop_free(registry, net_fd, CONN_TYPE_NETWORK_LISTEN);

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

    int current_clients = 0;
    int current_workers = 0;
    conn_context_t *ctx;
    conn_context_t *free_slot;
    int registry_idx;
    struct epoll_event client_ev;

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
            ctx = (conn_context_t *)events[i].data.ptr;
            if (!ctx || ctx->fd == -1) continue;

            // Вычисляем индекс элемента в пуле памяти с помощью вашей математики указателей!
            registry_idx = ctx - registry->slots;
            
            switch (ctx->state.active.type){
                case CONN_TYPE_NETWORK_LISTEN: {
                    // From net_fd
                    while (1) {
                        // 1. Check our limits
                        if (current_clients >= max_clients) {
                            printf("[C-WARNING] Client limit reached (%d). Rejecting incoming connection.\n", max_clients);
                            int temp_fd = accept(ctx->fd, NULL, NULL);
                            if (temp_fd >= 0) close(temp_fd); // Close politely
                            break; 
                        }

                        // 2. Accept a new client
                        int client_fd = accept(ctx->fd, NULL, NULL);
                        if (client_fd < 0) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                break; // All clients are pulled out from the epoll queue
                            }
                            perror("[C-ERROR] accept failed for network client");
                            break;
                        }

                        // 3. Make new sockets client non-blocking
                        conn_set_nonblocking(client_fd);

                        // 4. Search a new slot
                        conn_context_t *free_slot = registry_pop_free(registry, client_fd, CONN_TYPE_CLIENT);
                        if (!free_slot) {
                            printf("[C-ERROR] Critical: No free memory slot found, but limit wasn't reached.\n");
                            close(client_fd);
                            break;
                        }

                        // 5. Bind to epoll
                        client_ev.events = EPOLLIN | EPOLLET;
                        client_ev.data.ptr = free_slot;
                        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_ev) == -1) {
                            perror("[C-ERROR] epoll_ctl: failed to add client");
                            close(client_fd);
                            registry_push_free(registry, free_slot);
                            break;
                        }

                        current_clients++;
                        printf("[C-SERVER] Client successfully registered on fd %d; number of clients\n", client_fd, current_clients);
                    }
                    break;
                }

                case CONN_TYPE_IPC_LISTEN: {
                    // from IPC
                    while (1) {
                        // 1. Check our limits
                        if (current_workers >= max_workers) {
                            printf("[C-WARNING] Worker limit reached (%d). Rejecting new worker.\n", config.max_workers);
                            int temp_fd = accept(ctx->fd, NULL, NULL);
                            if (temp_fd >= 0) close(temp_fd); // Close politely
                            break;
                        }

                        // 2. Accept a new worker
                        int worker_fd = accept(ctx->fd, NULL, NULL);
                        if (worker_fd < 0) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                break; // All workers are accepted
                            }
                            perror("[C-ERROR] accept failed for Python worker");
                            break;
                        }

                        // 3. Make it non-blocking
                        conn_set_nonblocking(worker_fd);

                        // 4. Search a new slot
                        conn_context_t *free_slot = registry_pop_free(registry, worker_fd, CONN_TYPE_WORKER);
                        if (!free_slot) {
                            printf("[C-ERROR] Critical: No free memory slot found, but limit wasn't reached.\n");
                            close(worker_fd);
                            break;
                        }

                        // 5. Bind to epoll
                        client_ev.events = EPOLLIN | EPOLLET;
                        client_ev.data.ptr = free_slot;
                        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, worker_fd, &client_ev) == -1) {
                            perror("[C-ERROR] epoll_ctl: failed to add client");
                            close(worker_fd);
                            registry_push_free(registry, free_slot);
                            break;
                        }

                        current_workers++;
                        printf("[C-SERVER] Python worker successfully registered on fd %d number of workers: %d\n", worker_fd, current_workers);
                    }
                    break;
                }
            }

        }
    }

    printf("[C-SERVER] Shutting down and cleaning memory pool...\n");
    
    registry_destroy(registry);
    close(epoll_fd);
    unlink(config.socket_path);
    
    printf("[C-SERVER] Shutdown successful. Goodbye!\n");
    return 0;
}
