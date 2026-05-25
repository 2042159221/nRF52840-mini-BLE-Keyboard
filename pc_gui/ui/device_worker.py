from __future__ import annotations

from collections.abc import Callable
from queue import Empty, Queue
from typing import Any

from PySide6.QtCore import QObject, QThread, Signal, Slot

from pc_gui.device.device_client import DeviceClient

WorkerJob = Callable[[DeviceClient], Any]


class DeviceWorker(QObject):
    result = Signal(str, object)
    error = Signal(str, str)

    def __init__(self, client: DeviceClient) -> None:
        super().__init__()
        self.client = client
        self._jobs: Queue[tuple[str, WorkerJob]] = Queue()
        self._running = True

    def submit(self, name: str, job: WorkerJob) -> None:
        self._jobs.put((name, job))

    @Slot()
    def run(self) -> None:
        while self._running:
            try:
                name, job = self._jobs.get(timeout=0.1)
            except Empty:
                continue
            if name == "__stop__":
                break
            try:
                self.result.emit(name, job(self.client))
            except Exception as exc:
                self.error.emit(name, str(exc))

    def stop(self) -> None:
        self._running = False
        self._jobs.put(("__stop__", lambda client: None))


class DeviceWorkerThread:
    def __init__(self, client: DeviceClient) -> None:
        self.thread = QThread()
        self.worker = DeviceWorker(client)
        self.worker.moveToThread(self.thread)
        self.thread.started.connect(self.worker.run)
        self.thread.start()

    def stop(self) -> None:
        self.worker.stop()
        self.thread.quit()
        self.thread.wait(1000)
