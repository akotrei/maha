#include "connection.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

registry_t* registry_create(int size) {
    if (size <= 0) return NULL;

    registry_t *reg = malloc(sizeof(registry_t));
    if (!reg) return NULL;

    // Allocate a contiguous block of memory for all connection slots
    reg->slots = malloc(sizeof(conn_context_t) * size);
    if (!reg->slots) {
        free(reg);
        return NULL;
    }

    reg->size = size;
    reg->head_free_idx = 0;

    // Initialize the LIFO Free List chain across unallocated slots
    for (int i = 0; i < size; i++) {
        reg->slots[i].fd = -1; // -1 means the slot is vacant
        reg->slots[i].as.next_free_idx = i + 1;
    }
    // Terminal element indicates the end of the free pool chain
    reg->slots[size - 1].as.next_free_idx = -1;

    return reg;
}

void registry_destroy(registry_t *reg) {
    if (!reg) return;

    if (reg->slots) {
        // Clean up and close any descriptors that are still left open
        for (int i = 0; i < reg->size; i++) {
            if (reg->slots[i].fd != -1) {
                close(reg->slots[i].fd);
            }
        }
        free(reg->slots);
    }
    free(reg);
}

conn_context_t* registry_pop_free(registry_t *reg, int fd, conn_type_t type) {
    if (!reg || reg->head_free_idx == -1 || fd < 0) {
        return NULL; 
    }

    int allocated_idx = reg->head_free_idx;
    conn_context_t *slot = &reg->slots[allocated_idx];

    // Pop the slot by moving the free chain head forward
    reg->head_free_idx = slot->as.next_free_idx;

    // Bind descriptor and identify dispatcher type
    slot->fd = fd;
    slot->type = type;

    // OPTIMIZED: No more massive memset here! The slot was already wiped in push_free.
    // We only explicitly set fields that must NOT be zero.
    if (type == CONN_TYPE_CLIENT) {
        slot->as.client.state = CONN_STATE_CLIENT_READ_HEADERS;
        slot->as.client.slot_id = -1; // -1 indicates no shm_chunk is mapped yet
        slot->as.client.peer_idx = -1;
    } else if (type == CONN_TYPE_AUTH_WORKER || type == CONN_TYPE_RW_WORKER || type == CONN_TYPE_TRACKER_WORKER) {
        slot->as.worker.state = CONN_STATE_WORKER_IDLE;
        slot->as.worker.peer_idx = -1;
    } else {
        slot->as.listener.state = CONN_STATE_LISTENING;
    }

    return slot;
}

void registry_push_free(registry_t *reg, conn_context_t *slot) {
    if (!reg || !slot || slot->fd == -1) return;

    // Calculate the array distance index to prevent out-of-bounds pointer tampering
    ptrdiff_t idx = slot - reg->slots;
    if (idx < 0 || idx >= reg->size) return;

    // Gracefully shutdown and release the OS file descriptor handle
    close(slot->fd);
    slot->fd = -1;

    // CRITICAL SECURITY FIX: Wipe the entire data payload clean 
    // to prevent memory leaks or context contamination between consecutive connections
    memset(&slot->as, 0, sizeof(slot->as));

    // Link the recycled slot back into the head of the LIFO Free List loop
    slot->as.next_free_idx = reg->head_free_idx;
    reg->head_free_idx = (int)idx;
}

int conn_set_nonblocking(int fd) {
    if (fd < 0) return -1;
    
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    
    // Mandatory configuration step ensuring sockets play nice with Edge-Triggered epoll loop
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
