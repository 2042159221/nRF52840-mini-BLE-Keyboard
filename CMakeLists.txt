cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(PRO_Ble_mini_keyboard)

if(NOT c_std_11 IN_LIST CMAKE_C_COMPILE_FEATURES)
	list(APPEND CMAKE_C_COMPILE_FEATURES c_std_11)
endif()

list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
include(nanopb)

target_include_directories(app PRIVATE include)

zephyr_nanopb_sources(app proto/device_comm.proto)

target_sources(app PRIVATE
	src/config/app_config.c
	src/config/app_config_store.c
	src/display/status_screen.c
	src/host/host_frame.c
	src/host/host_protocol.c
	src/host/host_comm.c
	src/main.c
	src/display/status_screen_input.c
	src/display/status_screen_model.c
	src/hid/hid_consumer.c
	src/hid/hid_flowctrl.c
	src/hid/hid_report.c
	src/input/encoder_action.c
	src/input/encoder_manager.c
	src/input/input_manager.c
	src/keymap/keymap.c
	src/keyboard/keyboard_led_state.c
	src/mode/mode_manager.c
	src/mode/mode_selector.c
	src/power/battery_monitor.c
	src/power/ip5306.c
	src/power/power_manager.c
	src/power/power_policy.c
	src/power/power_state.c
	src/rgb/rgb_effect.c
	src/rgb/rgb_hw.c
	src/rgb/rgb_keymap.c
	src/rgb/rgb_manager.c
	src/rgb/rgb_policy.c
	src/transport/transport.c
	src/transport/transport_24g_stub.c
	src/transport/ble_bas_adapter.c
	src/transport/transport_ble.c
	src/transport/transport_ble_security.c
	src/transport/transport_usb.c
)

target_sources_ifdef(CONFIG_LVGL app PRIVATE
	src/display/status_screen_lvgl.c
)
