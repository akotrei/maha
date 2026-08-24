#include "shm_base.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

shm_base_t* shm_base_create(const char *name, size_t size) {
    if (!name || size == 0) return NULL;

    // Выделяем память в приватной куче управляющего процесса под структуру
    shm_base_t *shm = (shm_base_t*)malloc(sizeof(shm_base_t));
    if (!shm) return NULL;

    memset(shm, 0, sizeof(shm_base_t));
    strncpy(shm->name, name, sizeof(shm->name) - 1);
    shm->size = size;

    // 1. Открываем/создаем файл разделяемой памяти POSIX.
    // O_TRUNC очищает старый мусор, если сервер аварийно перезапустился.
    shm->fd = shm_open(shm->name, O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (shm->fd == -1) {
        free(shm);
        return NULL;
    }

    // 2. Жестко резервируем размер файла в оперативной памяти против SIGBUS ошибок.
    if (ftruncate(shm->fd, shm->size) == -1) {
        close(shm->fd);
        shm_unlink(shm->name);
        free(shm);
        return NULL;
    }

    // 3. Отображаем физическую RAM в адресное пространство процесса.
    // MAP_SHARED гарантирует реалтайм синхронизацию изменений между Си и Python.
    shm->ptr = mmap(NULL, shm->size, PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0);
    if (shm->ptr == MAP_FAILED) {
        close(shm->fd);
        shm_unlink(shm->name);
        free(shm);
        return NULL;
    }

    return shm;
}

void shm_base_destroy(shm_base_t *shm) {
    if (!shm) return;

    // Освобождаем mmap
    if (shm->ptr && shm->ptr != MAP_FAILED) {
        munmap(shm->ptr, shm->size);
    }
    
    // Закрываем дескриптор файла
    if (shm->fd != -1) {
        close(shm->fd);
    }
    
    // Удаляем объект из виртуальной файловой системы /dev/shm
    shm_unlink(shm->name);

    // Удаляем саму структуру из кучи управляющего процесса Си
    free(shm);
}
