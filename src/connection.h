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
    union {
        struct {
            conn_type_t type;
            int slot_id;
        } active;
        int next_free_idx;
    } state;
} conn_context_t;

typedef struct {
    conn_context_t *slots;
    int head_free_idx;
    int size;
} registry_t;

registry_t* registry_create(int size);

void registry_destroy(registry_t *reg);

conn_context_t* registry_pop_free(registry_t *reg, int fd, conn_type_t type);

void registry_push_free(registry_t *reg, conn_context_t *slot);

int conn_set_nonblocking(int fd);

#endif // CONNECTION_H
