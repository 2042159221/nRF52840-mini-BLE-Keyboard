import pytest

from pc_gui.device.device_client import DeviceClient
from pc_gui.device.device_models import DeviceConfig
from pc_gui.device.error_codes import DeviceResponseError, ResponseCode
from pc_gui.device.mock_transport import MockTransport
from pc_gui.proto import device_comm_pb2


def test_device_client_mock_round_trip():
    events = []
    transport = MockTransport()
    client = DeviceClient(transport, log_handler=lambda event, fields: events.append((event, fields)))

    client.connect()
    hello = client.hello()
    assert hello.protocol_version == 1

    config = client.get_config()
    assert config.rgb_brightness == 30

    updated = client.set_config(DeviceConfig(rgb_brightness=68), apply=True, save=True)
    assert updated.rgb_brightness == 68
    assert transport.saved is True

    status = client.get_status()
    assert status.usb_present is True
    assert status.battery_percent == 95

    client.factory_reset()
    assert client.get_config().rgb_brightness == 30
    client.disconnect()
    assert any(event == "tx" for event, _ in events)
    assert any(event == "rx" for event, _ in events)


def test_device_client_raises_config_set_response_error():
    transport = MockTransport()
    client = DeviceClient(transport)

    client.connect()
    transport.next_config_set_code = device_comm_pb2.RESPONSE_CODE_INVALID_PARAM

    with pytest.raises(DeviceResponseError) as exc_info:
        client.set_config(DeviceConfig(rgb_brightness=101), apply=True, save=False)

    assert exc_info.value.code == ResponseCode.INVALID_PARAM
