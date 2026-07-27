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

conn_context_t* conn_create(int fd, conn_type_t type);
int conn_set_nonblocking(int fd);
void conn_destroy(conn_context_t *ctx);

#endif // CONNECTION_H
