from __future__ import annotations

from PySide6.QtCore import QDateTime
from PySide6.QtWidgets import QPlainTextEdit, QVBoxLayout, QWidget


class DeveloperLogPage(QWidget):
    def __init__(self) -> None:
        super().__init__()
        self.log = QPlainTextEdit()
        self.log.setReadOnly(True)
        layout = QVBoxLayout(self)
        layout.addWidget(self.log)

    def append_event(self, event: str, fields: dict[str, int | str]) -> None:
        timestamp = QDateTime.currentDateTime().toString("HH:mm:ss.zzz")
        detail = " ".join(f"{key}={value}" for key, value in fields.items())
        self.log.appendPlainText(f"{timestamp} {event} {detail}".rstrip())
