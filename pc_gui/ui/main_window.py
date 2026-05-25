from __future__ import annotations

from collections.abc import Callable
from typing import Any

from PySide6.QtWidgets import QMainWindow, QMessageBox, QTabWidget

from pc_gui.device.device_client import DeviceClient
from pc_gui.device.device_models import DeviceConfig

from .developer_log_page import DeveloperLogPage
from .device_page import DevicePage
from .device_worker import DeviceWorkerThread
from .encoder_page import EncoderPage
from .rgb_page import RgbPage
from .system_page import SystemPage


class MainWindow(QMainWindow):
    def __init__(self, client: DeviceClient) -> None:
        super().__init__()
        self.setWindowTitle("PRO BLE Mini Keyboard")
        self.client = client
        self.cached_config: DeviceConfig | None = None
        self._pending: dict[str, Callable[[object], None]] = {}
        self.log_page = DeveloperLogPage()
        self.client.protocol.log_handler = self.log_page.append_event
        self.worker_thread = DeviceWorkerThread(self.client)
        self.worker_thread.worker.result.connect(self._handle_worker_result)
        self.worker_thread.worker.error.connect(self._handle_worker_error)

        tabs = QTabWidget()
        tabs.addTab(DevicePage(self), "Device")
        tabs.addTab(RgbPage(self), "RGB")
        tabs.addTab(EncoderPage(self), "Encoder")
        tabs.addTab(SystemPage(self), "System")
        tabs.addTab(self.log_page, "Developer Log")
        self.setCentralWidget(tabs)

    def run_device_task(
        self,
        name: str,
        job: Callable[[DeviceClient], Any],
        on_result: Callable[[object], None] | None = None,
    ) -> None:
        if on_result is not None:
            self._pending[name] = on_result
        self.worker_thread.worker.submit(name, job)

    def current_config_copy(self) -> DeviceConfig | None:
        if self.cached_config is None:
            return None
        return DeviceConfig.from_dict(self.cached_config.to_dict())

    def apply_config(self, config: DeviceConfig) -> None:
        self.run_device_task(
            "apply_config",
            lambda client: client.set_config(config, apply=True, save=False),
        )

    def _handle_worker_result(self, name: str, result: object) -> None:
        if name == "connect" and isinstance(result, dict):
            config = result.get("config")
            if isinstance(config, DeviceConfig):
                self.cached_config = config
        elif name == "get_config" and isinstance(result, DeviceConfig):
            self.cached_config = result
        elif name == "apply_config" and isinstance(result, DeviceConfig):
            self.cached_config = result
        elif name == "factory_reset":
            self.cached_config = None

        callback = self._pending.pop(name, None)
        if callback is not None:
            callback(result)

    def _handle_worker_error(self, name: str, message: str) -> None:
        self._pending.pop(name, None)
        QMessageBox.warning(self, name.replace("_", " ").title(), message)

    def closeEvent(self, event) -> None:
        self.worker_thread.stop()
        super().closeEvent(event)
