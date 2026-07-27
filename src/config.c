#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


int parse_arguments(int argc, char **argv, server_config_t *config) {
    config->port = CONFIG_PORT;
    config->max_clients = CONFIG_MAX_CLIENTS;
    config->max_workers = CONFIG_MAX_WORKERS;
    strncpy(config->socket_path, CONFIG_SOCKET_PATH, sizeof(config->socket_path) - 1);

    int opt;
    while ((opt = getopt(argc, argv, "p:c:w:s")) != -1) {
        switch (opt) {
            case 'p': config->port = atoi(optarg); break;
            case 'c': config->max_clients = atoi(optarg); break;
            case 'w': config->max_workers = atoi(optarg); break;
            case 's':
                memset(config->socket_path, 0, sizeof(config->socket_path));
                strncpy(config->socket_path, optarg, sizeof(config->socket_path) - 1);
                break;
            default:
                fprintf(stderr, "Using: %s [-p port] [-c max number of clients] [-w number of workers]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // 3. Validate settings
    if (config->port <= 0 || config->port > 65535) {
        fprintf(stderr, "Error: Incorrect port %d (allowed 1-65535)\n", config->port);
        exit(EXIT_FAILURE);
    }
    if (config->max_clients <= 0) {
        fprintf(stderr, "Error: Max number of clients should be positive\n");
        exit(EXIT_FAILURE);
    }
    if (config->max_workers <= 0) {
        fprintf(stderr, "Error: Number of workers should be positive\n");
        exit(EXIT_FAILURE);
    }
    if (strlen(config->socket_path) == 0) {
        fprintf(stderr, "Error: Path to Unix-socket can't be empty\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}
