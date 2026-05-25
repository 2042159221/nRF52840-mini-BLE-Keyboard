from __future__ import annotations

from PySide6.QtWidgets import (
    QFormLayout,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QVBoxLayout,
    QWidget,
)


class DevicePage(QWidget):
    def __init__(self, window) -> None:
        super().__init__()
        self.window = window
        self.identity = QLabel("Disconnected")
        self.status = QLabel("No status")
        connect_button = QPushButton("Connect")
        hello_button = QPushButton("Hello")
        refresh_button = QPushButton("Refresh")
        connect_button.clicked.connect(self.connect_and_refresh)
        hello_button.clicked.connect(self.refresh_identity)
        refresh_button.clicked.connect(self.refresh_status)

        buttons = QHBoxLayout()
        buttons.addWidget(connect_button)
        buttons.addWidget(hello_button)
        buttons.addWidget(refresh_button)
        buttons.addStretch(1)

        form = QFormLayout()
        form.addRow("Device", self.identity)
        form.addRow("Status", self.status)

        layout = QVBoxLayout(self)
        layout.addLayout(buttons)
        layout.addLayout(form)
        layout.addStretch(1)

    def connect_and_refresh(self) -> None:
        def connect_and_read(client):
            client.connect()
            hello = client.hello()
            config = client.get_config()
            status = client.get_status()
            return {"hello": hello, "config": config, "status": status}

        self.window.run_device_task(
            "connect",
            connect_and_read,
            self._show_connect_result,
        )

    def refresh_identity(self) -> None:
        self.window.run_device_task(
            "hello",
            lambda client: client.hello(),
            self._show_identity,
        )

    def refresh_status(self) -> None:
        self.window.run_device_task(
            "status",
            lambda client: client.get_status(),
            self._show_status,
        )

    def _show_connect_result(self, result) -> None:
        self.window.cached_config = result["config"]
        self._show_identity(result["hello"])
        self._show_status(result["status"])

    def _show_identity(self, hello) -> None:
        self.identity.setText(
            f"VID {hello.vendor_id:04x} PID {hello.product_id:04x} "
            f"FW {hello.firmware_major}.{hello.firmware_minor}.{hello.firmware_patch}"
        )

    def _show_status(self, status) -> None:
        charging = "charging" if status.charging else "not charging"
        self.status.setText(
            f"mode={status.current_mode} battery={status.battery_percent}% "
            f"power={status.power_level} usb={status.usb_present} {charging}"
        )
