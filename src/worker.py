import socket
import os
import sys
import time
import argparse
from protocol import IPCMessage, IPCCmd, IPC_MSG_SIZE


def run_worker(socket_path: str):
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
                # Get a message
                msg = IPCMessage.from_bytes(raw_bytes)
                print(f"\n[PYTHON] Commad from c is received: {msg.type.name} (slot_id: {msg.slot_id}, size: {msg.data_size} bytes)")
                
                match msg.type:
                    case IPCCmd.WRITE_CHUNK:
                        print(f"[PYTHON] [DISK] Write a batch a from slot {msg.slot_id} to a disk...")
                        time.sleep(0.05) # Immitate disk latency (50 мс)
                        
                    case IPCCmd.FINAL_CHUNK:
                        print(f"[PYTHON] [DISK] Last chunk is received! Start to build a file...")
                        time.sleep(0.2) # Assebmling of a file is hevier (200 мс)
                        print("[PYTHON] [Disk] The file is successfully created!")
                        
                    case IPCCmd.ABORT:
                        print(f"[PYTHON] [DISK] WARNING: Cancle command for slot {msg.slot_id} is received!")
                        print("[PYTHON] [DISK] Remove temp garbage from a disk...")
                        time.sleep(0.01)
                        
                    case _:
                        # Unsupported case
                        print(f"[PYTHON] Unknown command is received: {msg.type}")

                # 5. Send back to C a coomand about result
                response = IPCMessage(type=IPCCmd.ACK, slot_id=msg.slot_id, data_size=0)
                client_socket.sendall(response.to_bytes())
                print(f"[PYTHON] Result is sent to C: ACK for a socket {msg.slot_id}")

            except ValueError as e:
                print(f"[PYTHON] Protocol validation error: {e}. Ignoring a pocket.")
                
    except KeyboardInterrupt:
        print("\n[PYTHON] The worker is stopped (Ctrl+C).")
    finally:
        # Clean resources
        client_socket.close()
        print("[PYTHON] The socket is closed. By!")


def parse_args():
    """Parse command line args"""
    parser = argparse.ArgumentParser(description="Silent Python-worker for writing media to a disk.")
    
    # Flag -s / --socket with 
    parser.add_argument(
        "-s", "--socket",
        type=str,
        default="/tmp/gallery.sock",
        help="Path to a Unix Domain file Socket (default: /tmp/gallery.sock)"
    )
    
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()
    run_worker(args.socket)
