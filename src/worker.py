import socket
import os
import sys
import time
from protocol import IPCMessage, IPCCmd, IPC_MSG_SIZE


SOCKET_PATH = "/tmp/gallery.sock"


def run_worker():
    print("[PYTHON] Background worker has started. Searching a c-server...")
    
    # 1. Check if the c-server created a socket
    if not os.path.exists(SOCKET_PATH):
        print(f"[PYTHON] Error: socket file {SOCKET_PATH} has not been found.")
        print("[PYTHON] Please, run c-server before, to create the ipc socket!")
        sys.exit(1)

    # 2. Create Unix Domain socket (AF_UNIX)
    client_socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

    try:
        # 3. Connecting to the ipc socket
        client_socket.connect(SOCKET_PATH)
        print(f"[PYTHON] Successfully connected to the c-server socket: {SOCKET_PATH}!")
        
    except socket.error as e:
        print(f"[PYTHON] Connetin to c-server socket failed: {e}")
        sys.exit(1)

    # 4. Handle c-server messages
    try:
        while True:
            # recv() blocks as and we are slipping until c send as a message
            raw_bytes = client_socket.recv(IPC_MSG_SIZE)
            
            # if recv returns 0 c-server finished or failed.
            if not raw_bytes:
                print("[PYTHON] C-server finished his work. Exiting...")
                break

            try:
                # get a message
                msg = IPCMessage.from_bytes(raw_bytes)
                print(f"\n[PYTHON] Получена команда от Си: {msg.type.name} (slot_id: {msg.slot_id}, size: {msg.data_size} байт)")
                
                # Использование современного паттерн-матчинга (доступен с Python 3.10+)
                match msg.type:
                    case IPCCmd.WRITE_CHUNK:
                        print(f"[PYTHON] [ДИСК] Пишу батч из слота {msg.slot_id} на флешку...")
                        time.sleep(0.05) # Имитируем задержку диска (50 мс)
                        
                    case IPCCmd.FINAL_CHUNK:
                        print(f"[PYTHON] [ДИСК] Получен финальный чанк! Начинаю финальную сборку файла...")
                        time.sleep(0.2) # Сборка файла тяжелее (200 мс)
                        print("[PYTHON] [ДИСК] Видео успешно собрано и сохранено!")
                        
                    case IPCCmd.ABORT:
                        print(f"[PYTHON] [ДИСК] ВНИМАНИЕ: Получена команда ОТМЕНЫ для слота {msg.slot_id}!")
                        print("[PYTHON] [ДИСК] Удаляю временный мусор с диска...")
                        time.sleep(0.01)
                        
                    case _:
                        # Ветка по умолчанию (wildcard), если прилетит что-то совсем странное
                        print(f"[PYTHON] Получена неизвестная команда: {msg.type}")

                # 5. Отправляем обратно в Си сигнал подтверждения (ACK)
                # Си ждет эту весточку в своем неблокирующем epoll, чтобы освободить слот памяти!
                response = IPCMessage(type=IPCCmd.ACK, slot_id=msg.slot_id, data_size=0)
                client_socket.sendall(response.to_bytes())
                print(f"[PYTHON] Отправил подтверждение в Си: ACK для слота {msg.slot_id}")

            except ValueError as e:
                print(f"[PYTHON] Ошибка валидации протокола: {e}. Игнорирую пакет.")
                
    except KeyboardInterrupt:
        print("\n[PYTHON] Воркер остановлен пользователем (Ctrl+C).")
    finally:
        # Чистим за собой ресурсы
        client_socket.close()
        print("[PYTHON] Сокет закрыт. Пока!")


if __name__ == "__main__":
    run_worker()
