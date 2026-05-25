from __future__ import annotations

from threading import Condition, Thread
from typing import Any

from .transport_base import TransportBase


class SerialTransport(TransportBase):
    def __init__(self, port: str, baudrate: int = 115200, open_timeout: float = 2.0) -> None:
        self.port = port
        self.baudrate = baudrate
        self.open_timeout = open_timeout
        self._serial = None

    def open(self) -> None:
        import serial

        condition = Condition()
        state: dict[str, Any] = {
            "cancelled": False,
            "done": False,
            "kind": None,
            "value": None,
        }

        def open_port() -> None:
            try:
                serial_obj = serial.Serial(self.port, self.baudrate, timeout=0)
            except serial.SerialException as exc:
                with condition:
                    if state["cancelled"]:
                        return
                    state["done"] = True
                    state["kind"] = "error"
                    state["value"] = exc
                    condition.notify()
                return
            with condition:
                if state["cancelled"]:
                    serial_obj.close()
                    return
                state["done"] = True
                state["kind"] = "serial"
                state["value"] = serial_obj
                condition.notify()

        Thread(target=open_port, daemon=True).start()
        with condition:
            if not condition.wait_for(lambda: state["done"], timeout=self.open_timeout):
                state["cancelled"] = True
                self._serial = None
                raise RuntimeError(
                    f"Timed out opening USB CDC ACM port {self.port} after {self.open_timeout:.2f}s. "
                    "Close any serial monitor, nRF Connect, or VS Code terminal using the port, "
                    "unplug/replug the device, then retry."
                )
            kind = state["kind"]
            value = state["value"]

        if kind == "serial":
            self._serial = value
            return

        try:
            raise value
        except serial.SerialException as exc:
            self._serial = None
            raise RuntimeError(
                f"Cannot open USB CDC ACM port {self.port}: {exc}. "
                "Close any serial monitor, nRF Connect, or VS Code terminal using the port, "
                "unplug/replug the device, then retry."
            ) from exc

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
