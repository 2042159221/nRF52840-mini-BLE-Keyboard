from __future__ import annotations

from .device_models import DeviceConfig, DeviceStatus
from .device_protocol import DeviceProtocol, LogHandler
from .error_codes import DeviceResponseError, ResponseCode
from .transport_base import TransportBase
from pc_gui.proto import device_comm_pb2


class DeviceClient:
    def __init__(self, transport: TransportBase, log_handler: LogHandler | None = None) -> None:
        self.transport = transport
        self.protocol = DeviceProtocol(transport, log_handler=log_handler)

    def connect(self) -> None:
        self.transport.open()

    def disconnect(self) -> None:
        self.transport.close()

    def hello(self, app_name: str = "PRO Config") -> device_comm_pb2.HelloRsp:
        message = device_comm_pb2.DeviceMessage()
        message.hello_req.protocol_version = 1
        message.hello_req.app_name = app_name
        return self.protocol.request(message).hello_rsp

    def get_config(self) -> DeviceConfig:
        message = device_comm_pb2.DeviceMessage()
        message.config_get_req.SetInParent()
        response = self.protocol.request(message)
        return DeviceConfig.from_proto(response.config_get_rsp.config)

    def set_config(self, config: DeviceConfig, apply: bool = True, save: bool = False) -> DeviceConfig:
        message = device_comm_pb2.DeviceMessage()
        message.config_set_req.config.CopyFrom(config.to_proto())
        message.config_set_req.apply = apply
        message.config_set_req.save = save
        response = self.protocol.request(message)
        if response.config_set_rsp.code != device_comm_pb2.RESPONSE_CODE_OK:
            raise DeviceResponseError(
                ResponseCode(response.config_set_rsp.code),
                "config set failed",
            )
        return DeviceConfig.from_proto(response.config_set_rsp.effective_config)

    def save_config(self) -> None:
        message = device_comm_pb2.DeviceMessage()
        message.config_save_req.SetInParent()
        self.protocol.request(message)

    def factory_reset(self) -> None:
        message = device_comm_pb2.DeviceMessage()
        message.factory_reset_req.SetInParent()
        self.protocol.request(message)

    def get_status(self) -> DeviceStatus:
        message = device_comm_pb2.DeviceMessage()
        message.device_status_req.SetInParent()
        response = self.protocol.request(message).device_status_rsp
        return DeviceStatus(
            current_mode=response.current_mode,
            battery_percent=response.battery_percent,
            power_level=response.power_level,
            usb_present=response.usb_present,
            charging=response.charging,
        )
