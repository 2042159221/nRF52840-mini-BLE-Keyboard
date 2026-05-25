from __future__ import annotations

import time
from collections.abc import Callable

from .error_codes import DeviceResponseError, FrameDecodeError, ResponseCode
from .frame_codec import FrameCodec, FrameStreamDecoder
from .transport_base import TransportBase
from pc_gui.proto import device_comm_pb2

LogHandler = Callable[[str, dict[str, int | str]], None]


class DeviceProtocol:
    def __init__(
        self,
        transport: TransportBase,
        timeout: float = 1.0,
        log_handler: LogHandler | None = None,
    ) -> None:
        self.transport = transport
        self.timeout = timeout
        self.log_handler = log_handler
        self.next_msg_id = 1

    def request(self, message: device_comm_pb2.DeviceMessage) -> device_comm_pb2.DeviceMessage:
        message.msg_id = self.next_msg_id
        self.next_msg_id += 1
        body = message.WhichOneof("body") or "empty"
        payload = message.SerializeToString()
        frame = FrameCodec.encode(payload)
        self._log("tx", msg_id=message.msg_id, body=body, length=len(payload))
        self.transport.write(frame)
        response = self._read_message()
        rsp_body = response.WhichOneof("body") or "empty"
        self._log(
            "rx",
            msg_id=response.msg_id,
            reply_to=response.reply_to,
            body=rsp_body,
            length=response.ByteSize(),
        )
        if response.reply_to != message.msg_id:
            raise DeviceResponseError(ResponseCode.INTERNAL_ERROR, "reply_to mismatch")
        if rsp_body == "response" and response.response.code != ResponseCode.OK:
            raise DeviceResponseError(response.response.code, response.response.message)
        return response

    def _read_message(self) -> device_comm_pb2.DeviceMessage:
        decoder = FrameStreamDecoder()
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            remaining = max(0.0, deadline - time.monotonic())
            data = self.transport.read(64, remaining)
            if not data:
                continue
            try:
                frames = decoder.feed(data)
            except FrameDecodeError as exc:
                self._log("frame_error", error=str(exc), length=len(data))
                raise
            if not frames:
                continue
            message = device_comm_pb2.DeviceMessage()
            message.ParseFromString(frames[0])
            return message
        self._log("timeout", length=0)
        raise TimeoutError("device response timed out")

    def _log(self, event: str, **fields: int | str) -> None:
        if self.log_handler is not None:
            self.log_handler(event, fields)
