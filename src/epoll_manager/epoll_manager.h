#ifndef EPOLL_MANAGER_H
#define EPOLL_MANAGER_H

#include "config/config.h"
#include <sys/epoll.h>

// Returns epoll fd and fill out_ipc_fd and out_net_fd
int epoll_manager_init(const server_config_t *config, int *out_ipc_fd, int *out_net_fd);

#endif // EPOLL_MANAGER_H
