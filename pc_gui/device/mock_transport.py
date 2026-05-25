from __future__ import annotations

from .device_models import DeviceConfig
from .frame_codec import FrameCodec
from .transport_base import TransportBase
from pc_gui.proto import device_comm_pb2


class MockTransport(TransportBase):
    def __init__(self) -> None:
        self.is_open = False
        self.read_buffer = bytearray()
        self.config = DeviceConfig()
        self.saved = False
        self.next_config_set_code = device_comm_pb2.RESPONSE_CODE_OK

    def open(self) -> None:
        self.is_open = True

    def close(self) -> None:
        self.is_open = False

    def write(self, data: bytes) -> None:
        if not self.is_open:
            raise RuntimeError("mock transport is not open")
        request = device_comm_pb2.DeviceMessage()
        request.ParseFromString(FrameCodec.decode(data))
        response = self._handle(request)
        self.read_buffer.extend(FrameCodec.encode(response.SerializeToString()))

    def read(self, size: int, timeout: float) -> bytes:
        del timeout
        if not self.read_buffer:
            return b""
        chunk = bytes(self.read_buffer[:size])
        del self.read_buffer[:size]
        return chunk

    def _handle(self, request: device_comm_pb2.DeviceMessage) -> device_comm_pb2.DeviceMessage:
        response = device_comm_pb2.DeviceMessage(reply_to=request.msg_id)
        body = request.WhichOneof("body")
        if body == "hello_req":
            response.hello_rsp.protocol_version = 1
            response.hello_rsp.vendor_id = 0x1915
            response.hello_rsp.product_id = 0x52F0
            response.hello_rsp.firmware_major = 0
            response.hello_rsp.firmware_minor = 1
            response.hello_rsp.firmware_patch = 0
            response.hello_rsp.capability_flags = 0x07
        elif body == "config_get_req":
            response.config_get_rsp.config.CopyFrom(self.config.to_proto())
        elif body == "config_set_req":
            response.config_set_rsp.code = self.next_config_set_code
            self.next_config_set_code = device_comm_pb2.RESPONSE_CODE_OK
            if response.config_set_rsp.code == device_comm_pb2.RESPONSE_CODE_OK:
                self.config = DeviceConfig.from_proto(request.config_set_req.config)
            if request.config_set_req.save and response.config_set_rsp.code == device_comm_pb2.RESPONSE_CODE_OK:
                self.saved = True
            response.config_set_rsp.effective_config.CopyFrom(self.config.to_proto())
        elif body == "config_save_req":
            self.saved = True
            response.response.code = device_comm_pb2.RESPONSE_CODE_OK
            response.response.message = "saved"
        elif body == "factory_reset_req":
            self.config = DeviceConfig()
            self.saved = True
            response.response.code = device_comm_pb2.RESPONSE_CODE_OK
            response.response.message = "factory reset"
        elif body == "device_status_req":
            response.device_status_rsp.current_mode = 0
            response.device_status_rsp.battery_percent = 95
            response.device_status_rsp.power_level = 0
            response.device_status_rsp.usb_present = True
            response.device_status_rsp.charging = False
        else:
            response.response.code = device_comm_pb2.RESPONSE_CODE_UNKNOWN_TYPE
            response.response.message = "unknown"
        return response
