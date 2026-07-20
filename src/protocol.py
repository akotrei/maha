from __future__ import annotations
import struct
from dataclasses import dataclass
from enum import IntEnum


class IPCCmd(IntEnum):
    WRITE_CHUNK = 1
    ACK         = 2
    FINAL_CHUNK = 3
    ABORT       = 4


IPC_MSG_FORMAT = "!iii"  # '!' Network Byte Order (Big-Endian)
IPC_MSG_SIZE = struct.calcsize(IPC_MSG_FORMAT)  # 12 bytes

@dataclass(frozen=True)
class IPCMessage:
    type: IPCCmd
    slot_id: int    
    data_size: int  

    @classmethod
    def from_bytes(cls, raw_bytes: bytes) -> IPCMessage:
        """
        Parse from c
        """
        if len(raw_bytes) != IPC_MSG_SIZE:
            raise ValueError(
                f"Incorrect message length: got {len(raw_bytes)} "
                f"expected: {IPC_MSG_SIZE}"
            )
        
        cmd_type, slot_id, data_size = struct.unpack(IPC_MSG_FORMAT, raw_bytes)
        return cls(type=IPCCmd(cmd_type), slot_id=slot_id, data_size=data_size)

    def to_bytes(self) -> bytes:
        """
        Put back to c-struct
        """
        return struct.pack(IPC_MSG_FORMAT, self.type, self.slot_id, self.data_size)
        