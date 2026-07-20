#include <stdio.h>
#include "config.h"


int main(int argc, char** argv) {
    setbuf(stdout, NULL);

    server_config_t config;
    parse_arguments(argc, argv, &config);

    printf("[СИ-СЕРВЕР] Инициализация успешна!\n");
    printf("[НАСТРОЙКА] Порт для Nginx: %d\n", config.port);
    printf("[НАСТРОЙКА] Лимит клиентов: %d\n", config.max_clients);
    printf("[НАСТРОЙКА] Python-воркеров: %d\n", config.max_workers);

    printf("[СИ-СЕРВЕР] Подготовка завершена. Готов к созданию IPC каналов.\n");

    return 0;
}
