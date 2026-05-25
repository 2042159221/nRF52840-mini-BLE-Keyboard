import sys
import time
import types

import pytest

from pc_gui.device.serial_transport import SerialTransport


def test_serial_transport_open_adds_port_diagnostic(monkeypatch):
    class SerialException(Exception):
        pass

    def open_serial(port, baudrate, timeout):
        raise SerialException("PermissionError(13, 'Access is denied.', None, 5)")

    serial_module = types.SimpleNamespace(Serial=open_serial, SerialException=SerialException)
    monkeypatch.setitem(sys.modules, "serial", serial_module)

    transport = SerialTransport("COM19")

    with pytest.raises(RuntimeError) as exc_info:
        transport.open()

    message = str(exc_info.value)
    assert "COM19" in message
    assert "USB CDC ACM" in message
    assert "serial monitor" in message


def test_serial_transport_open_times_out_when_driver_blocks(monkeypatch):
    def open_serial(port, baudrate, timeout):
        time.sleep(0.2)
        return types.SimpleNamespace(close=lambda: None)

    serial_module = types.SimpleNamespace(Serial=open_serial, SerialException=Exception)
    monkeypatch.setitem(sys.modules, "serial", serial_module)

    transport = SerialTransport("COM19", open_timeout=0.01)

    started = time.monotonic()
    with pytest.raises(RuntimeError) as exc_info:
        transport.open()

    assert time.monotonic() - started < 0.15
    assert "Timed out opening USB CDC ACM port COM19" in str(exc_info.value)


def test_serial_transport_closes_late_open_after_timeout(monkeypatch):
    closed = []

    class LateSerial:
        def __init__(self) -> None:
            self.timeout = 0

        def close(self) -> None:
            closed.append(True)

    def open_serial(port, baudrate, timeout):
        time.sleep(0.05)
        return LateSerial()

    serial_module = types.SimpleNamespace(Serial=open_serial, SerialException=Exception)
    monkeypatch.setitem(sys.modules, "serial", serial_module)

    transport = SerialTransport("COM19", open_timeout=0.01)

    with pytest.raises(RuntimeError, match="Timed out opening USB CDC ACM port COM19"):
        transport.open()

    time.sleep(0.1)
    assert transport._serial is None
    assert closed == [True]
