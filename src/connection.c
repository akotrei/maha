#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "connection.h"


conn_context_t* conn_create(int fd, conn_type_t type) {
    conn_context_t *ctx = malloc(sizeof(conn_context_t));
    if (!ctx) return NULL;
    ctx->fd = fd;
    ctx->type = type;
    ctx->slot_id = -1;
    return ctx;
}

int conn_set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void conn_destroy(conn_context_t *ctx) {
    if (!ctx) return;
    if (ctx->fd >= 0) {
        close(ctx->fd);
    }
    free(ctx);
}
