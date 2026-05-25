from __future__ import annotations

from PySide6.QtWidgets import QHBoxLayout, QLabel, QPushButton, QVBoxLayout, QWidget


class SystemPage(QWidget):
    def __init__(self, window) -> None:
        super().__init__()
        self.window = window
        self.result = QLabel("")
        save_button = QPushButton("Save")
        reset_button = QPushButton("Factory reset")
        status_button = QPushButton("Status")
        save_button.clicked.connect(self.save)
        reset_button.clicked.connect(self.reset)
        status_button.clicked.connect(self.status)

        buttons = QHBoxLayout()
        buttons.addWidget(save_button)
        buttons.addWidget(reset_button)
        buttons.addWidget(status_button)
        buttons.addStretch(1)

        layout = QVBoxLayout(self)
        layout.addLayout(buttons)
        layout.addWidget(self.result)
        layout.addStretch(1)

    def save(self) -> None:
        self.window.run_device_task(
            "save_config",
            lambda client: client.save_config(),
            lambda result: self.result.setText("Saved"),
        )

    def reset(self) -> None:
        self.window.run_device_task(
            "factory_reset",
            lambda client: client.factory_reset(),
            lambda result: self.result.setText("Factory defaults restored"),
        )

    def status(self) -> None:
        self.window.run_device_task(
            "status_system",
            lambda client: client.get_status(),
            self._show_status,
        )

    def _show_status(self, status) -> None:
        self.result.setText(
            f"mode={status.current_mode} battery={status.battery_percent}% "
            f"usb={status.usb_present}"
        )
