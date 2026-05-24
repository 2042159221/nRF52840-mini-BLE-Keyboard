cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(PRO_Ble_mini_keyboard)

target_include_directories(app PRIVATE include)

target_sources(app PRIVATE
	src/main.c
	src/hid/hid_consumer.c
	src/hid/hid_report.c
	src/input/encoder_manager.c
	src/input/input_manager.c
	src/keymap/keymap.c
	src/mode/mode_manager.c
	src/mode/mode_selector.c
	src/power/battery_monitor.c
	src/power/ip5306.c
	src/power/power_manager.c
	src/power/power_policy.c
	src/power/power_state.c
	src/transport/transport.c
	src/transport/transport_24g_stub.c
	src/transport/ble_bas_adapter.c
	src/transport/transport_ble.c
	src/transport/transport_ble_security.c
	src/transport/transport_usb.c
)
