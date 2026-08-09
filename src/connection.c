#include "connection.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

registry_t* registry_create(int size) {
    registry_t *reg = malloc(sizeof(registry_t));
    if (!reg) return NULL;

    reg->slots = calloc(size, sizeof(conn_context_t));
    if (!reg->slots) {
        free(reg);
        return NULL;
    }

    reg->size = size;
    reg->head_free_idx = 0;

    for (int i = 0; i < size - 1; i++) {
        reg->slots[i].fd = -1;
        reg->slots[i].state.next_free_idx = i + 1;
    }
    reg->slots[size - 1].fd = -1;
    reg->slots[size - 1].state.next_free_idx = -1;

    return reg;
}

void registry_destroy(registry_t *reg) {
    if (!reg) return;
    
    for (int i = 0; i < reg->size; i++) {
        if (reg->slots[i].fd != -1) {
            close(reg->slots[i].fd);
        }
    }
    free(reg->slots);
    free(reg);
}

conn_context_t* registry_pop_free(registry_t *reg, int fd, conn_type_t type) {
    if (!reg || reg->head_free_idx == -1) return NULL;

    int current_idx = reg->head_free_idx;
    conn_context_t *slot = &reg->slots[current_idx];

    reg->head_free_idx = slot->state.next_free_idx;

    slot->fd = fd;
    slot->state.active.type = type;
    slot->state.active.slot_id = -1;

    return slot;
}

void registry_push_free(registry_t *reg, conn_context_t *slot) {
    if (!reg || !slot) return;
    slot->fd = -1;

    int registry_idx = slot - reg->slots;

    slot->state.next_free_idx = reg->head_free_idx;
    reg->head_free_idx = registry_idx;
}

int conn_set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
