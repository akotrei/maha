#ifndef CONNECTION_H
#define CONNECTION_H


typedef enum {
    CONN_TYPE_NETWORK_LISTEN,
    CONN_TYPE_IPC_LISTEN,
    CONN_TYPE_CLIENT,
    CONN_TYPE_WORKER
} conn_type_t;

typedef struct {
    int fd;
    conn_type_t type;
    int slot_id;
} conn_context_t;

int conn_set_nonblocking(int fd);

#endif // CONNECTION_H
