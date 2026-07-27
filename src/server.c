#include <stdio.h>
#include "config.h"
#include "ipc.h"


int main(int argc, char** argv) {
    setbuf(stdout, NULL);

    server_config_t config;
    parse_arguments(argc, argv, &config);
    printf("[C-SERVER] Initialisation is successfull!\n");
    printf("[C-SETTING] Port for Nginx: %d\n", config.port);
    printf("[C-SETTINGS] Clients limit: %d\n", config.max_clients);
    printf("[C-SETTINGS] Python-workers: %d\n", config.max_workers);
    printf("[C-SETTINGS] IPC-socket path: %s\n", config.socket_path);

    int server_fd = create_ipc_socket(config.socket_path);

    printf("[C-SEVER] Server is reay for creating IPC chanels.\n");

    

    return 0;
}
