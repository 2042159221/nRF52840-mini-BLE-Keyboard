from __future__ import annotations

from dataclasses import asdict, dataclass

from pc_gui.proto import device_comm_pb2


@dataclass(slots=True)
class DeviceConfig:
    version: int = 1
    rgb_enable: bool = True
    rgb_mode: int = 2
    rgb_brightness: int = 30
    rgb_red: int = 255
    rgb_green: int = 80
    rgb_blue: int = 20
    numlock_policy: int = 1
    encoder_cw_action: int = 1
    encoder_ccw_action: int = 2
    encoder_press_action: int = 3
    mode_policy: int = 0
    default_mode: int = 0

    def to_dict(self) -> dict[str, int | bool]:
        return asdict(self)

    @classmethod
    def from_dict(cls, data: dict[str, int | bool]) -> "DeviceConfig":
        values = cls().to_dict()
        values.update(data)
        return cls(**values)

    def to_proto(self) -> device_comm_pb2.DeviceConfig:
        proto = device_comm_pb2.DeviceConfig()
        for key, value in self.to_dict().items():
            setattr(proto, key, value)
        return proto

    @classmethod
    def from_proto(cls, proto: device_comm_pb2.DeviceConfig) -> "DeviceConfig":
        return cls(
            version=proto.version,
            rgb_enable=proto.rgb_enable,
            rgb_mode=proto.rgb_mode,
            rgb_brightness=proto.rgb_brightness,
            rgb_red=proto.rgb_red,
            rgb_green=proto.rgb_green,
            rgb_blue=proto.rgb_blue,
            numlock_policy=proto.numlock_policy,
            encoder_cw_action=proto.encoder_cw_action,
            encoder_ccw_action=proto.encoder_ccw_action,
            encoder_press_action=proto.encoder_press_action,
            mode_policy=proto.mode_policy,
            default_mode=proto.default_mode,
        )


@dataclass(slots=True)
class DeviceStatus:
    current_mode: int = 0
    battery_percent: int = 0
    power_level: int = 0
    usb_present: bool = False
    charging: bool = False
