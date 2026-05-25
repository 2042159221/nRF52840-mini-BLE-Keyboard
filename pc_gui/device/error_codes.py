from enum import IntEnum


class ResponseCode(IntEnum):
    OK = 0
    UNKNOWN_TYPE = 1
    INVALID_LENGTH = 2
    INVALID_PARAM = 3
    NOT_READY = 4
    INTERNAL_ERROR = 5
    STORAGE_ERROR = 6


class DeviceProtocolError(RuntimeError):
    pass


class FrameDecodeError(DeviceProtocolError):
    pass


class DeviceResponseError(DeviceProtocolError):
    def __init__(self, code: int, message: str = "") -> None:
        super().__init__(f"device response {code}: {message}")
        self.code = code
        self.message = message
