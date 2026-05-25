from __future__ import annotations

import argparse
import sys

from pc_gui.device.device_client import DeviceClient
from pc_gui.device.mock_transport import MockTransport
from pc_gui.device.serial_transport import SerialTransport


def build_client(port: str | None) -> DeviceClient:
    transport = SerialTransport(port) if port else MockTransport()
    return DeviceClient(transport)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", help="CDC ACM serial port")
    args = parser.parse_args()

    from PySide6.QtWidgets import QApplication
    from pc_gui.ui.main_window import MainWindow

    app = QApplication(sys.argv)
    window = MainWindow(build_client(args.port))
    window.resize(980, 680)
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
