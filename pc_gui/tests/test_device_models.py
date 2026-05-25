from pc_gui.device.device_models import DeviceConfig


def test_default_config_matches_firmware_defaults():
    config = DeviceConfig()
    assert config.rgb_enable is True
    assert config.rgb_mode == 2
    assert config.rgb_brightness == 30
    assert config.rgb_red == 255
    assert config.rgb_green == 80
    assert config.rgb_blue == 20
    assert config.numlock_policy == 1
    assert config.encoder_cw_action == 1
    assert config.encoder_ccw_action == 2
    assert config.encoder_press_action == 3


def test_config_dict_and_proto_round_trip():
    config = DeviceConfig(rgb_brightness=64, encoder_press_action=7)
    assert DeviceConfig.from_dict(config.to_dict()) == config
    assert DeviceConfig.from_proto(config.to_proto()) == config
