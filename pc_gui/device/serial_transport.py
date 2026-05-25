from __future__ import annotations

from .transport_base import TransportBase


class SerialTransport(TransportBase):
    def __init__(self, port: str, baudrate: int = 115200) -> None:
        self.port = port
        self.baudrate = baudrate
        self._serial = None

    def open(self) -> None:
        import serial

        self._serial = serial.Serial(self.port, self.baudrate, timeout=0)

    def close(self) -> None:
        if self._serial is not None:
            self._serial.close()
            self._serial = None

    def write(self, data: bytes) -> None:
        if self._serial is None:
            raise RuntimeError("serial transport is not open")
        self._serial.write(data)

    def read(self, size: int, timeout: float) -> bytes:
        if self._serial is None:
            raise RuntimeError("serial transport is not open")
        previous_timeout = self._serial.timeout
        self._serial.timeout = timeout
        try:
            return self._serial.read(size)
        finally:
            self._serial.timeout = previous_timeout
