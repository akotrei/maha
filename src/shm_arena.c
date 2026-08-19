#include "shm_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>


shm_arena_t* shm_arena_create(const char *name, int slot_count) {
    if (!name || slot_count <= 0) return NULL;

    shm_arena_t *arena = (shm_arena_t*)malloc(sizeof(shm_arena_t));
    if (!arena) return NULL;

    memset(arena, 0, sizeof(shm_arena_t));
    strncpy(arena->name, name, sizeof(arena->name) - 1);
    arena->slot_count = slot_count;
    arena->total_size = slot_count * CHUNK_SIZE;

    arena->shm_fd = shm_open(arena->name, O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (arena->shm_fd == -1) {
        free(arena);
        return NULL;
    }

    if (ftruncate(arena->shm_fd, arena->total_size) == -1) {
        close(arena->shm_fd);
        shm_unlink(arena->name);
        free(arena);
        return NULL;
    }

    arena->shm_ptr = mmap(NULL, arena->total_size, PROT_READ | PROT_WRITE, MAP_SHARED, arena->shm_fd, 0);
    if (arena->shm_ptr == MAP_FAILED) {
        close(arena->shm_fd);
        shm_unlink(arena->name);
        free(arena);
        return NULL;
    }

    arena->meta = (shm_slot_meta_t*)malloc(sizeof(shm_slot_meta_t) * slot_count);
    if (!arena->meta) {
        munmap(arena->shm_ptr, arena->total_size);
        close(arena->shm_fd);
        shm_unlink(arena->name);
        free(arena);
        return NULL;
    }

    for (int i = 0; i < slot_count; i++) {
        arena->meta[i].is_free = 1;
        arena->meta[i].next_free_slot_id = i + 1;
    }
    arena->meta[slot_count - 1].next_free_slot_id = -1;
    arena->head_free_slot_id = 0;
    return arena;
}

void shm_arena_destroy(shm_arena_t *arena) {
    if (!arena) return;

    if (arena->shm_ptr && arena->shm_ptr != MAP_FAILED) {
        munmap(arena->shm_ptr, arena->total_size);
    }
    if (arena->shm_fd != -1) {
        close(arena->shm_fd);
    }
    
    shm_unlink(arena->name);

    if (arena->meta) {
        free(arena->meta);
    }
    free(arena);
}

int shm_arena_pop_chunk(shm_arena_t *arena, void **out_slot_ptr) {
    if (!arena || !out_slot_ptr || arena->head_free_slot_id == -1) {
        return -1;
    }

    int allocated_id = arena->head_free_slot_id;
    shm_slot_meta_t *slot_meta = &arena->meta[allocated_id];

    arena->head_free_slot_id = slot_meta->next_free_slot_id;
    
    slot_meta->is_free = 0;
    slot_meta->next_free_slot_id = -1;

    *out_slot_ptr = (char*)arena->shm_ptr + (allocated_id * CHUNK_SIZE);

    return allocated_id;
}

void shm_arena_push_free_chunk(shm_arena_t *arena, int slot_id) {
    if (!arena || slot_id < 0 || slot_id >= arena->slot_count) return;
    
    shm_slot_meta_t *slot_meta = &arena->meta[slot_id];

    if (slot_meta->is_free) {
        return; 
    }

    slot_meta->next_free_slot_id = arena->head_free_slot_id;
    slot_meta->is_free = 1;
    arena->head_free_slot_id = slot_id;
}
