#ifndef SHM_ARENA_H
#define SHM_ARENA_H

#include <stddef.h>

#define CHUNK_SIZE 65536 // Размер одного чанка (64 КБ)

// Структура одного служебного слота управления на стороне Си.
// Она живет в обычной RAM Си-сервера и управляет состоянием чанка в mmap.
typedef struct {
    int next_free_slot_id; // Индекс следующего свободного чанка в цепочке Free List
    int is_free;           // Флаг: 1 — чанк свободен, 0 — занят клиентом/воркером
} shm_slot_meta_t;

// Единый объект Арены памяти
typedef struct {
    int shm_fd;               // Системный дескриптор /dev/shm
    void *shm_ptr;            // Прямой указатель на начало всего бинарного блока в RAM
    int slot_count;           // Общее количество чанков в пуле
    size_t total_size;        // Полный размер памяти в байтах
    char name[64];            // Системное имя объекта (например, "/media_shm")
    
    // Менеджер свободных мест (наш скрытый Free List)
    shm_slot_meta_t *meta;    // Статический массив метаданных для каждого чанка
    int head_free_slot_id;    // Индекс самого первого свободного чанка в Арене
} shm_arena_t;

// Создает именованную Арену и полностью настраивает Free List для чанков
shm_arena_t* shm_arena_create(const char *name, int slot_count);

// Уничтожает Арену, закрывает mmap, удаляет имя из системы и чистит метаданные
void shm_arena_destroy(shm_arena_t *arena);

// МГНОВЕННО забирает свободный чанк из Арены за O(1).
// Записывает в out_slot_ptr прямой указатель на память в mmap, куда Си будет лить данные.
// Возвращает slot_id (индекс чанка) или -1, если вся Арена забита.
int shm_arena_pop_chunk(shm_arena_t *arena, void **out_slot_ptr);

// МГНОВЕННО возвращает чанк обратно в пул свободных мест за O(1) (вызывается при получении ACK от воркера)
void shm_arena_push_free_chunk(shm_arena_t *arena, int slot_id);

#endif // SHM_ARENA_H
