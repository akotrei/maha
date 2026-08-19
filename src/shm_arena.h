#ifndef SHM_ARENA_H
#define SHM_ARENA_H

#include <stddef.h>

#define CHUNK_SIZE 65536 // 64 kB


typedef struct {
    int next_free_slot_id;
    int is_free;
} shm_slot_meta_t;

typedef struct {
    int shm_fd;               // /dev/shm
    void *shm_ptr;            // RAM
    int slot_count;           
    int total_size;        
    char name[64];            // name on a disk
    shm_slot_meta_t *meta;
    int head_free_slot_id;
} shm_arena_t;

shm_arena_t* shm_arena_create(const char *name, int slot_count);
void shm_arena_destroy(shm_arena_t *arena);
int shm_arena_pop_chunk(shm_arena_t *arena, void **out_slot_ptr);
void shm_arena_push_free_chunk(shm_arena_t *arena, int slot_id);

#endif // SHM_ARENA_H
