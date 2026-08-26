#ifndef SHM_BASE_H
#define SHM_BASE_H

#include <stddef.h>

typedef struct {
    int fd;
    void *ptr;
    size_t size;
    char name[64];
} shm_base_t;

shm_base_t* shm_base_create(const char *name, size_t size);
void shm_base_destroy(shm_base_t *shm);

#endif // SHM_BASE_H
