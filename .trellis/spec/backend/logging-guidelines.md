# Logging Guidelines

> How logging is done in this project.

## Overview

Use module-local `LOG_MODULE_REGISTER(...)` declarations and keep logs focused on state transitions, transport readiness, and failures that help board bring-up.

## Log Levels

### `LOG_INF`
Use for lifecycle and state changes that matter during bring-up:
- module initialization success
- USB/BLE mode changes
- BLE connection and disconnection
- BLE security level changes
- HID notification enable and disable events

### `LOG_WRN`
Use for recoverable or expected failures that affect behavior:
- transport send failures
- mode switch failures
- failed security requests
- ignored input events that do not map to HID usage
- non-ready transport attempts

### `LOG_ERR`
Use for initialization failures and unrecoverable configuration problems:
- transport init failures
- missing or not-ready hardware devices
- board or stack setup failures

### `LOG_DBG`
Use sparingly for low-level traces such as dropped input events or transport readiness checks.

## What to Log

- transport init and enable/disable events
- mode transitions
- BLE connection lifecycle and security results
- report send failures and report release failures
- input events that were ignored because they do not map to the product keyboard

## What NOT to Log

- raw HID report dumps in normal operation
- secrets, pairing keys, or bond data
- high-frequency scan spam from every matrix poll
- repetitive per-event traces that do not help bring-up

## Example

```c
LOG_MODULE_REGISTER(input_manager, LOG_LEVEL_INF);
LOG_INF("mode switched from %d to %d", current, next);
LOG_WRN("keyboard report send failed: %d", err);
```
