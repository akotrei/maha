#include "shm_chunks.h"
#include <stdlib.h>
#include <string.h>

shm_chunks_t* shm_chunks_create(const char *name, int slot_count) {
    if (!name || slot_count <= 0) return NULL;

    shm_chunks_t *pool = malloc(sizeof(shm_chunks_t));
    if (!pool) return NULL;

    // Calculate the size strictly required for chunks: slot_count * 64 kB
    size_t required_shm_size = (size_t)slot_count * CHUNK_SIZE;

    // Initialize the base shm layout (creates the file and runs mmap)
    pool->base = shm_base_create(name, required_shm_size);
    if (!pool->base) {
        free(pool);
        return NULL;
    }

    // Allocate tracking metadata locally in the C-server process memory
    pool->meta = malloc(sizeof(shm_slot_meta_t) * slot_count);
    if (!pool->meta) {
        shm_base_destroy(pool->base);
        free(pool);
        return NULL;
    }

    pool->slot_count = slot_count;
    pool->head_free_slot_id = 0;

    // Build the initial LIFO free list structure
    for (int i = 0; i < slot_count; i++) {
        pool->meta[i].is_free = 1;
        pool->meta[i].next_free_slot_id = i + 1;
    }
    // Terminal element points to nowhere
    pool->meta[slot_count - 1].next_free_slot_id = -1;

    return pool;
}

void shm_chunks_destroy(shm_chunks_t *pool) {
    if (!pool) return;

    if (pool->meta) {
        free(pool->meta);
    }
    if (pool->base) {
        shm_base_destroy(pool->base);
    }
    free(pool);
}

int shm_chunks_pop_chunk(shm_chunks_t *pool, void **out_slot_ptr) {
    if (!pool || pool->head_free_slot_id == -1 || !out_slot_ptr) {
        return -1;
    }

    int allocated_id = pool->head_free_slot_id;
    shm_slot_meta_t *slot_meta = &pool->meta[allocated_id];

    // Move the head pointer to the next free slot
    pool->head_free_slot_id = slot_meta->next_free_slot_id;
    slot_meta->is_free = 0;
    slot_meta->next_free_slot_id = -1;

    // Compute the absolute RAM pointer inside the shared space for the C-server
    *out_slot_ptr = (char*)pool->base->ptr + ((size_t)allocated_id * CHUNK_SIZE);

    return allocated_id;
}

void shm_chunks_push_free_chunk(shm_chunks_t *pool, int slot_id) {
    if (!pool || slot_id < 0 || slot_id >= pool->slot_count) {
        return;
    }

    shm_slot_meta_t *slot_meta = &pool->meta[slot_id];
    if (slot_meta->is_free) {
        return; // Guard against double-free errors
    }

    // Insert back at the head of the LIFO stack
    slot_meta->next_free_slot_id = pool->head_free_slot_id;
    slot_meta->is_free = 1;
    pool->head_free_slot_id = slot_id;
}
