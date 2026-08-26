#ifndef SHM_CHUNKS_H
#define SHM_CHUNKS_H

#include "shm_base.h"
#include <stddef.h>

#define CHUNK_SIZE 65536 // 64 kB

// Slot metadata used only by the C-server for LIFO management
typedef struct {
    int next_free_slot_id;
    int is_free;
} shm_slot_meta_t;

// Context structure holding the pool state and inheriting from shm_base
typedef struct {
    shm_base_t *base;             // Underlying shared memory mapping module
    int slot_count;               // Total number of 64 kB chunks available
    shm_slot_meta_t *meta;        // Local metadata array (allocated in heap)
    int head_free_slot_id;        // Index of the first available slot in LIFO
} shm_chunks_t;

/**
 * Creates a chunk pool by automatically wrapping around a new shm_base instance.
 * @param name The file name inside /dev/shm
 * @param slot_count Total amount of 64 kB chunks to allocate
 */
shm_chunks_t* shm_chunks_create(const char *name, int slot_count);

/**
 * Destroys the chunk pool, freeing local metadata and closing the shm_base.
 */
void shm_chunks_destroy(shm_chunks_t *pool);

/**
 * Pops a free chunk from the LIFO pool in O(1) time.
 * @param out_slot_ptr Pointer where the absolute memory address of the chunk will be written
 * @return The allocated slot ID, or -1 if the pool is empty
 */
int shm_chunks_pop_chunk(shm_chunks_t *pool, void **out_slot_ptr);

/**
 * Pushes a chunk back into the free LIFO pool in O(1) time.
 * @param slot_id ID of the slot to release
 */
void shm_chunks_push_free_chunk(shm_chunks_t *pool, int slot_id);

#endif // SHM_CHUNKS_H
