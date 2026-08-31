from __future__ import annotations
import struct
from dataclasses import dataclass
from enum import IntEnum


class MediaType(IntEnum):
    UNKNOWN = 0
    JPG     = 1
    PNG     = 2
    MP4     = 3
    MOV     = 4


class IPCCmd(IntEnum):
    # --- RW_WORKER DISK COMMANDS ---
    RW_WRITE_CHUNK = 1
    RW_ACK         = 2
    RW_FINAL_CHUNK = 3
    RW_ABORT       = 4
    RW_DELETE_FILE = 5

    # --- AUTH_WORKER COMMANDS ---
    AUTH_REQUEST   = 6
    AUTH_RESPONSE  = 7

    # --- TRACKER_WORKER COMMANDS ---
    TRK_REQ_METADATA = 8
    TRK_META_UPDATED = 9


# '=' means Native Endianness (matches C packed struct)
# i = int32 (4 bytes), Q = uint64 (8 bytes)
IPC_MSG_FORMAT = "=iiiiQQ"  
IPC_MSG_SIZE = struct.calcsize(IPC_MSG_FORMAT)  # Exactly 32 bytes


@dataclass(frozen=True)
class IPCMessage:
    cmd: IPCCmd        # Matches the updated enum dispatch name
    slot_id: int    
    data_size: int  
    media_type: MediaType
    owner_id: int
    file_id: int

    @classmethod
    def from_bytes(cls, raw_bytes: bytes) -> IPCMessage:
        """
        Parse from C struct packed bytes
        """
        if len(raw_bytes) != IPC_MSG_SIZE:
            raise ValueError(
                f"Incorrect message length: got {len(raw_bytes)} "
                f"expected: {IPC_MSG_SIZE}"
            )
        
        raw_cmd, slot_id, data_size, m_type, owner_id, file_id = struct.unpack(
            IPC_MSG_FORMAT, raw_bytes
        )
        return cls(
            cmd=IPCCmd(raw_cmd), 
            slot_id=slot_id, 
            data_size=data_size,
            media_type=MediaType(m_type),
            owner_id=owner_id,
            file_id=file_id
        )

    def to_bytes(self) -> bytes:
        """
        Pack back to C struct packed bytes
        """
        return struct.pack(
            IPC_MSG_FORMAT, 
            self.cmd.value, # Explicitly extract integer value from IntEnum
            self.slot_id, 
            self.data_size, 
            self.media_type.value, 
            self.owner_id, 
            self.file_id
        )
