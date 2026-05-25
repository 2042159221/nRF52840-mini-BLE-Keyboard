from __future__ import annotations

from dataclasses import dataclass

from .error_codes import FrameDecodeError

MAGIC = b"\x55\xaa"
VERSION = 1
HEADER_SIZE = 5
CRC_SIZE = 2
MAX_PAYLOAD = 512


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for value in data:
        crc ^= value << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


class FrameCodec:
    @staticmethod
    def encode(payload: bytes) -> bytes:
        if len(payload) > MAX_PAYLOAD:
            raise ValueError("payload too large")
        header = MAGIC + bytes([VERSION]) + len(payload).to_bytes(2, "little")
        crc = crc16_ccitt(header[2:] + payload).to_bytes(2, "little")
        return header + payload + crc

    @staticmethod
    def decode(frame: bytes) -> bytes:
        payload, consumed = FrameCodec.decode_from_buffer(frame)
        if consumed != len(frame):
            raise FrameDecodeError("trailing frame bytes")
        return payload

    @staticmethod
    def decode_from_buffer(buffer: bytes) -> tuple[bytes, int]:
        if len(buffer) < HEADER_SIZE:
            raise FrameDecodeError("incomplete frame")
        if not buffer.startswith(MAGIC):
            raise FrameDecodeError("bad magic")
        if buffer[2] != VERSION:
            raise FrameDecodeError("bad version")
        length = int.from_bytes(buffer[3:5], "little")
        if length > MAX_PAYLOAD:
            raise FrameDecodeError("payload too large")
        frame_len = HEADER_SIZE + length + CRC_SIZE
        if len(buffer) < frame_len:
            raise FrameDecodeError("incomplete frame")
        expected = int.from_bytes(buffer[HEADER_SIZE + length:frame_len], "little")
        actual = crc16_ccitt(buffer[2:HEADER_SIZE + length])
        if expected != actual:
            raise FrameDecodeError("bad crc")
        return buffer[HEADER_SIZE:HEADER_SIZE + length], frame_len


@dataclass
class FrameStreamDecoder:
    buffer: bytearray

    def __init__(self) -> None:
        self.buffer = bytearray()

    def feed(self, data: bytes) -> list[bytes]:
        self.buffer.extend(data)
        frames: list[bytes] = []
        while True:
            if len(self.buffer) < HEADER_SIZE:
                break
            if self.buffer[0:2] != MAGIC:
                del self.buffer[0]
                continue
            try:
                payload, consumed = FrameCodec.decode_from_buffer(bytes(self.buffer))
            except FrameDecodeError as exc:
                if "incomplete" in str(exc):
                    break
                del self.buffer[0]
                continue
            frames.append(payload)
            del self.buffer[:consumed]
        return frames
