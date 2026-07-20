#ifndef CONFIG_H
#define CONFIG_H


typedef struct {
    int port;
    int max_clients;
    int max_workers;
} server_config_t;

int parse_arguments(int argc, char **argv, server_config_t *config);

#endif // CONFIG_H
