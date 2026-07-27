#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "epoll_manager.h"
#include "connection.h"
#include "ipc.h"


int epoll_manager_init(const server_config_t *config, int *out_ipc_fd, int *out_net_fd) {
    int epoll_fd = -1;
    int ipc_fd = -1;
    int net_fd = -1;

    // Create epoll_fd
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        printf("[C-ERROR] Failed to create epoll instance\n");
        return -1;
    }

    // Create ipc_fd
    ipc_fd = create_ipc_socket(config->socket_path);
    if (ipc_fd < 0) {
        printf("[C-ERROR] Failed to create IPC socket\n");
        goto fail;
    }
    if (conn_set_nonblocking(ipc_fd) < 0) {
        printf("[C-ERROR] Failed to set non-blocking for IPC socket\n");
        goto fail;
    }

    // Create net_fd
    net_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (net_fd == -1) {
        printf("[C-ERROR] Failed to create network socket\n");
        goto fail;
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
        goto fail;
    }
    
    if (listen(net_fd, SOMAXCONN) < 0) {
        printf("[C-ERROR] Network listen failed\n");
        goto fail;
    }
    
    if (conn_set_nonblocking(net_fd) < 0) {
        printf("[C-ERROR] Failed to set non-blocking for network socket\n");
        goto fail;
    }

    *out_ipc_fd = ipc_fd;
    *out_net_fd = net_fd;
    return epoll_fd;

fail:
    if (net_fd >= 0) close(net_fd);
    if (ipc_fd >= 0) close(ipc_fd);
    if (epoll_fd >= 0) close(epoll_fd);

    return -1;
}
