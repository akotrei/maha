#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "ipc.h"


int create_ipc_socket(const char* socket_path) {
    int server_fd;
    struct sockaddr_un address;

    // 1. Remove previous socket
    unlink(socket_path);

    // 2. Create new socket
    if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("[C-IPC] CRITICAL ERROR: Couldn't create new Unix-socket");
        exit(EXIT_FAILURE);
    }

    // 3. Zero all fields and set path
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, socket_path, sizeof(address.sun_path) - 1);

    // 4. Link socket server_fd with the file
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("[C-IPC] CRITICAL ERROR: Couldn't bind socket to file");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 5. Enable listening mode
    if (listen(server_fd, 5) < 0) {
        perror("[C-IPC] CRITICAL ERROR: Couldn't set socket to listen mode");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("[C-IPC] Unix-socket is created successfully by path: %s\n", socket_path);
    printf("[C-IPC] Server is ready to accept new connections from workers.\n");

    return server_fd;
}
