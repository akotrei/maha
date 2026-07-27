#ifndef CONFIG_H
#define CONFIG_H

#define CONFIG_PORT 8080
#define CONFIG_MAX_CLIENTS 10
#define CONFIG_MAX_WORKERS 1
#define CONFIG_SOCKET_PATH "/tmp/gallery.sock"


typedef struct {
    int port;
    int max_clients;
    int max_workers;
    char socket_path[128];
} server_config_t;

int parse_arguments(int argc, char **argv, server_config_t *config);

#endif // CONFIG_H
