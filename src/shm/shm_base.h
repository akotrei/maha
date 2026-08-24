#ifndef SHM_BASE_H
#define SHM_BASE_H

#include <stddef.h>

// Управляющая структура базового отображения разделяемой памяти
typedef struct {
    int fd;             // Системный дескриптор файла в /dev/shm
    void *ptr;          // Прямой указатель на начало выделенной RAM
    size_t size;        // Общий размер выделенной памяти в байтах
    char name[64];      // Имя объекта в системе
} shm_base_t;

/**
 * Создает или открывает объект разделяемой памяти и отображает его в RAM.
 * @param name Имя файла (например, "/media_acl_cache")
 * @param size Требуемый размер в байтах
 * @return Указатель на структуру управления или NULL при ошибке
 */
shm_base_t* shm_base_create(const char *name, size_t size);

/**
 * Полностью уничтожает отображение, закрывает дескриптор и удаляет файл из /dev/shm
 */
void shm_base_destroy(shm_base_t *shm);

#endif // SHM_BASE_H
