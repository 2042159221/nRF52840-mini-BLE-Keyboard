from __future__ import annotations

import argparse
import sys

from pc_gui.device.device_client import DeviceClient
from pc_gui.device.mock_transport import MockTransport
from pc_gui.device.serial_transport import SerialTransport


def build_client(port: str | None) -> DeviceClient:
    transport = SerialTransport(port) if port else MockTransport()
    return DeviceClient(transport)


def missing_dependency_message(module_name: str, executable: str | None = None) -> str:
    python_executable = executable or sys.executable
    return (
        f"缺少 GUI 运行依赖：{module_name}\n"
        "请在项目根目录执行：\n"
        f"{python_executable} -m pip install -r pc_gui/requirements.txt"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", help="CDC ACM serial port")
    args = parser.parse_args()

    try:
        from PySide6.QtWidgets import QApplication
        from pc_gui.ui.main_window import MainWindow
    except ModuleNotFoundError as exc:
        if exc.name == "PySide6":
            print(missing_dependency_message("PySide6"), file=sys.stderr)
            return 1
        raise

    app = QApplication(sys.argv)
    window = MainWindow(build_client(args.port))
    window.resize(980, 680)
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
