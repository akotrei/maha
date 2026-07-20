#include "ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/gallery.sock"


int create_ipc_socket(void) {
    int server_fd;
    struct sockaddr_un address;

    // 1. Принудительно удаляем старый файл сокета, если он остался от прошлых запусков.
    // Без этого bind() выдаст ошибку "Address already in use".
    unlink(SOCKET_PATH);

    // 2. Создаем локальный сокет. AF_UNIX — локальный IPC, SOCK_STREAM — надежный потоковый
    if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("[СИ-IPC] Критическая ошибка: Не удалось создать Unix-сокет");
        exit(EXIT_FAILURE);
    }

    // 3. Зануляем структуру адреса и прописываем путь к файлу
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    // Безопасно копируем строку пути, оставляя один байт под финальный нуль-терминатор
    strncpy(address.sun_path, SOCKET_PATH, sizeof(address.sun_path) - 1);

    // 4. Связываем дескриптор сокета с файлом на диске
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("[СИ-IPC] Критическая ошибка: Не удалось выполнить bind сокета к файлу");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 5. Включаем режим прослушивания двери.
    // Число 5 — размер очереди «входящих стуков» от подключающихся Питонов
    if (listen(server_fd, 5) < 0) {
        perror("[СИ-IPC] Критическая ошибка: Не удалось перевести сокет в режим listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("[СИ-IPC] Unix-сокет успешно создан по адресу: %s\n", SOCKET_PATH);
    printf("[СИ-IPC] Сервер готов принимать подключения от воркеров.\n");

    return server_fd;
}
