#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/autoconf.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <host/host_comm.h>
#include <host/host_frame.h>
#include <host/host_protocol.h>

LOG_MODULE_REGISTER(host_comm, LOG_LEVEL_INF);

#if DT_HAS_COMPAT_STATUS_OKAY(zephyr_cdc_acm_uart)

static const struct device *const host_uart =
    DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);

#define HOST_COMM_RX_BUFFER_SIZE HOST_FRAME_MAX_SIZE
#define HOST_COMM_RESPONSE_PAYLOAD_SIZE HOST_FRAME_MAX_PAYLOAD
#define HOST_COMM_UART_CHUNK_SIZE 64u

static uint8_t rx_buffer[HOST_COMM_RX_BUFFER_SIZE];
static size_t rx_len;
static uint8_t response_payload[HOST_COMM_RESPONSE_PAYLOAD_SIZE];
static uint8_t response_frame[HOST_FRAME_MAX_SIZE];
static uint8_t processing_frame[HOST_FRAME_MAX_SIZE];
static struct k_work host_comm_work;
static struct k_spinlock rx_lock;

static uint16_t get_le16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static size_t expected_frame_len(const uint8_t *frame)
{
    return HOST_FRAME_HEADER_SIZE + (size_t)get_le16(&frame[3]) +
           HOST_FRAME_CRC_SIZE;
}

static void drop_rx_bytes(size_t count)
{
    if (count >= rx_len) {
        rx_len = 0;
        return;
    }

    for (size_t i = 0; i < rx_len - count; i++) {
        rx_buffer[i] = rx_buffer[count + i];
    }
    rx_len -= count;
}

static void send_frame(const uint8_t *frame, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        uart_poll_out(host_uart, frame[i]);
    }
}

static void handle_frame(const uint8_t *frame, size_t frame_len)
{
    const uint8_t *payload;
    size_t payload_len;
    size_t response_len;
    size_t response_frame_len;
    int err;

    err = host_frame_decode(frame, frame_len, &payload, &payload_len);
    if (err != HOST_FRAME_OK) {
        LOG_WRN("host frame decode failed: %d", err);
        return;
    }

    err = host_protocol_handle_payload(payload, payload_len, response_payload,
                       sizeof(response_payload), &response_len);
    if (err != 0) {
        LOG_WRN("host protocol failed: %d", err);
        return;
    }

    err = host_frame_encode(response_payload, response_len, response_frame,
                sizeof(response_frame), &response_frame_len);
    if (err != HOST_FRAME_OK) {
        LOG_WRN("host frame encode failed: %d", err);
        return;
    }

    send_frame(response_frame, response_frame_len);
}

static void process_rx_buffer(void)
{
    while (true) {
        k_spinlock_key_t key;
        size_t frame_len;

        key = k_spin_lock(&rx_lock);
        if (rx_len < HOST_FRAME_HEADER_SIZE) {
            k_spin_unlock(&rx_lock, key);
            return;
        }

        if (rx_buffer[0] != HOST_FRAME_MAGIC0 ||
            rx_buffer[1] != HOST_FRAME_MAGIC1) {
            drop_rx_bytes(1);
            k_spin_unlock(&rx_lock, key);
            continue;
        }

        if (rx_buffer[2] != HOST_FRAME_VERSION) {
            LOG_WRN("unsupported host frame version: %u", rx_buffer[2]);
            drop_rx_bytes(HOST_FRAME_HEADER_SIZE);
            k_spin_unlock(&rx_lock, key);
            continue;
        }

        frame_len = expected_frame_len(rx_buffer);
        if (frame_len > HOST_FRAME_MAX_SIZE) {
            LOG_WRN("host frame too large: %u", (unsigned int)frame_len);
            drop_rx_bytes(HOST_FRAME_HEADER_SIZE);
            k_spin_unlock(&rx_lock, key);
            continue;
        }

        if (rx_len < frame_len) {
            k_spin_unlock(&rx_lock, key);
            return;
        }

        for (size_t i = 0; i < frame_len; i++) {
            processing_frame[i] = rx_buffer[i];
        }
        drop_rx_bytes(frame_len);
        k_spin_unlock(&rx_lock, key);

        handle_frame(processing_frame, frame_len);
    }
}

static void host_uart_irq_cb(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
        if (!uart_irq_rx_ready(dev)) {
            continue;
        }

        k_spinlock_key_t key = k_spin_lock(&rx_lock);
        while (rx_len < sizeof(rx_buffer)) {
            uint8_t chunk[HOST_COMM_UART_CHUNK_SIZE];
            size_t room = sizeof(rx_buffer) - rx_len;
            size_t want = room < sizeof(chunk) ? room : sizeof(chunk);
            int got = uart_fifo_read(dev, chunk, want);

            if (got <= 0) {
                break;
            }

            for (int i = 0; i < got; i++) {
                rx_buffer[rx_len++] = chunk[i];
            }
        }
        k_spin_unlock(&rx_lock, key);

        uart_irq_rx_disable(dev);
        (void)k_work_submit(&host_comm_work);
    }
}

static void host_comm_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    uart_irq_rx_disable(host_uart);
    process_rx_buffer();
    uart_irq_rx_enable(host_uart);
}

int host_comm_init(void)
{
    if (!device_is_ready(host_uart)) {
        LOG_WRN("CDC ACM host config UART is not ready");
        return -ENODEV;
    }

    host_protocol_reset_session();
    rx_len = 0;
    k_work_init(&host_comm_work, host_comm_work_handler);
    uart_irq_callback_set(host_uart, host_uart_irq_cb);
    uart_irq_rx_enable(host_uart);
    LOG_INF("host config CDC ACM initialized");
    return 0;
}

#else

int host_comm_init(void)
{
    LOG_WRN("host config CDC ACM node is not available");
    return -ENODEV;
}

#endif
