#include "shm_arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

shm_arena_t* shm_arena_create(const char *name, int slot_count) {
    if (!name || slot_count <= 0) return NULL;

    shm_arena_t *arena = malloc(sizeof(shm_arena_t));
    if (!arena) return NULL;

    arena->slot_count = slot_count;
    arena->total_size = (size_t)slot_count * CHUNK_SIZE;
    
    // Безопасно копируем имя (оно должно начинаться со слэша, например "/media_shm")
    strncpy(arena->name, name, sizeof(arena->name) - 1);

    // 1. На всякий случай удаляем старое имя из системы, если сервер прошлый раз упал аварийно
    shm_unlink(arena->name);

    // 2. Создаем или открываем объект разделяемой памяти POSIX
    // O_CREAT | O_RDWR — создать для чтения и записи
    // 0666 — права доступа (чтобы Python-процесс под любым юзером мог подключиться)
    arena->shm_fd = shm_open(arena->name, O_CREAT | O_RDWR, 0666);
    if (arena->shm_fd == -1) {
        perror("[C-SHM] Error: shm_open failed");
        free(arena);
        return NULL;
    }

    // 3. Жестко задаем размер выделяемой RAM памяти (ядро бронирует место)
    if (ftruncate(arena->shm_fd, arena->total_size) == -1) {
        perror("[C-SHM] Error: ftruncate failed");
        close(arena->shm_fd);
        shm_unlink(arena->name);
        free(arena);
        return NULL;
    }

    // 4. Проецируем созданный виртуальный файл в RAM нашего процесса
    arena->shm_ptr = mmap(NULL, arena->total_size, 
                          PROT_READ | PROT_WRITE, 
                          MAP_SHARED, 
                          arena->shm_fd, 0);

    if (arena->shm_ptr == MAP_FAILED) {
        perror("[C-SHM] Critical: mmap failed");
        close(arena->shm_fd);
        shm_unlink(arena->name);
        free(arena);
        return NULL;
    }

    printf("[C-SHM] Named Shared Memory Arena '%s' created successfully.\n", arena->name);
    printf("[C-SHM] Slots: %d, Total RAM: %zu bytes (mapped on /dev/shm%s).\n", 
           slot_count, arena->total_size, arena->name);

    return arena;
}

void shm_arena_destroy(shm_arena_t *arena) {
    if (!arena) return;

    // Сначала закрываем проекцию памяти в нашем процессе
    if (arena->shm_ptr && arena->shm_ptr != MAP_FAILED) {
        munmap(arena->shm_ptr, arena->total_size);
    }
    
    // Закрываем дескриптор виртуального файла
    if (arena->shm_fd >= 0) {
        close(arena->shm_fd);
    }

    // Полностью удаляем имя из системы (после этого /dev/shm/имя исчезает)
    shm_unlink(arena->name);
    
    printf("[C-SHM] Shared memory arena '%s' successfully destroyed.\n", arena->name);
    free(arena);
}

void* shm_arena_get_slot(shm_arena_t *arena, int slot_id) {
    if (!arena || slot_id < 0 || slot_id >= arena->slot_count) {
        return NULL;
    }
    char *base_ptr = (char *)arena->shm_ptr;
    return (void *)(base_ptr + (slot_id * CHUNK_SIZE));
}
