from __future__ import annotations

from abc import ABC, abstractmethod


class TransportBase(ABC):
    @abstractmethod
    def open(self) -> None:
        ...

    @abstractmethod
    def close(self) -> None:
        ...

    @abstractmethod
    def write(self, data: bytes) -> None:
        ...

    @abstractmethod
    def read(self, size: int, timeout: float) -> bytes:
        ...
