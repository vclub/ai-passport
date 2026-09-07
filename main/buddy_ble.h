#pragma once

#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*buddy_ble_write_cb_t)(const uint8_t *data, size_t len);
typedef void (*buddy_ble_link_cb_t)(bool connected);

esp_err_t buddy_ble_init(buddy_ble_write_cb_t on_write);
void buddy_ble_set_link_cb(buddy_ble_link_cb_t cb);
bool buddy_ble_ready(void);
const char *buddy_ble_name(void);
bool buddy_ble_connected(void);
void buddy_ble_notify(uint8_t byte);
void buddy_ble_disconnect(void);

#ifdef __cplusplus
}
#endif
