#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "epoll_manager.h"
#include "connection.h"
#include "ipc.h"


int epoll_manager_init(const server_config_t *config, int *out_ipc_fd, int *out_net_fd) {
    struct epoll_event ev;

    // 1. IPC
    int ipc_fd = create_ipc_socket(config->socket_path);
    if (ipc_fd < 0) {
        printf("[C-ERROR] Failed to create IPC socket\n");
        return -1;
    }
    if (conn_set_nonblocking(ipc_fd) < 0) {
        printf("[C-ERROR] Failed to set non-blocking for IPC socket\n");
        close(ipc_fd);
        return -1;
    }

    // 2. Создаем диспетчер epoll
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        printf("[C-ERROR] Failed to create epoll instance\n");
        close(ipc_fd);
        return -1;
    }

    // Регистрируем IPC в epoll
    conn_context_t *ipc_listen_ctx = conn_create(ipc_fd, CONN_TYPE_IPC_LISTEN);
    if (!ipc_listen_ctx) {
        close(epoll_fd);
        close(ipc_fd);
        return -1;
    }
    ev.events = EPOLLIN | EPOLLET;
    ev.data.ptr = ipc_listen_ctx;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ipc_fd, &ev) == -1) {
        printf("[C-ERROR] epoll_ctl failed for IPC\n");
        conn_destroy(ipc_listen_ctx);
        close(epoll_fd);
        return -1;
    }

    // 3. Создаем и настраиваем сетевой сокет (Nginx)
    int net_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (net_fd == -1) {
        printf("[C-ERROR] Failed to create network socket\n");
        conn_destroy(ipc_listen_ctx); // Очистит и закроет ipc_fd
        close(epoll_fd);
        return -1;
    }
    int opt = 1;
    setsockopt(net_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in net_addr;
    memset(&net_addr, 0, sizeof(net_addr));
    net_addr.sin_family = AF_INET;
    net_addr.sin_addr.s_addr = INADDR_ANY;
    net_addr.sin_port = htons(config->port);

    if (bind(net_fd, (struct sockaddr *)&net_addr, sizeof(net_addr)) < 0) {
        printf("[C-ERROR] Network bind failed on port %d\n", config->port);
        close(net_fd);
        conn_destroy(ipc_listen_ctx);
        close(epoll_fd);
        return -1;
    }
    if (listen(net_fd, config->max_clients) < 0) {
        printf("[C-ERROR] Network listen failed\n");
        close(net_fd);
        conn_destroy(ipc_listen_ctx);
        close(epoll_fd);
        return -1;
    }
    if (conn_set_nonblocking(net_fd) < 0) {
        printf("[C-ERROR] Failed to set non-blocking for network socket\n");
        close(net_fd);
        conn_destroy(ipc_listen_ctx);
        close(epoll_fd);
        return -1;
    }

    // Регистрируем сеть в epoll
    conn_context_t *net_listen_ctx = conn_create(net_fd, CONN_TYPE_NETWORK_LISTEN);
    if (!net_listen_ctx) {
        close(net_fd);
        conn_destroy(ipc_listen_ctx);
        close(epoll_fd);
        return -1;
    }
    ev.events = EPOLLIN | EPOLLET;
    ev.data.ptr = net_listen_ctx;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, net_fd, &ev) == -1) {
        printf("[C-ERROR] epoll_ctl failed for network\n");
        conn_destroy(net_listen_ctx);
        conn_destroy(ipc_listen_ctx);
        close(epoll_fd);
        return -1;
    }

    // Возвращаем дескрипторы наружу
    *out_ipc_fd = ipc_fd;
    *out_net_fd = net_fd;
    return epoll_fd;
}
