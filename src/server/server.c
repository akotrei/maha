#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include "server_init.h"

int main(int argc, char** argv) {
    setbuf(stdout, NULL);

    server_config_t config;
    server_ctx_t ctx = { .epoll_fd = -1, .net_listen_fd = -1, .ipc_listen_fd = -1, .registry = NULL };

    parse_arguments(argc, argv, &config);
    printf("[C-SERVER] Initialization parameters captured successfully.\n");

    // Boot up and isolate code execution tracking prior to server loop sequence
    if (server_boot(&ctx, &config) == -1) {
        fprintf(stderr, "[C-ERROR] Boot configuration failure. Critical exit.\n");
        return 1;
    }

    printf("[C-SERVER] Entering main asynchronous loop...\n");

    // ==========================================
    // MAIN ASYNCHRONOUS SERVER LOOP
    // ==========================================
    // While(1) and event loop goes here...

    server_shutdown(&ctx, &config);

    return 0;
}
