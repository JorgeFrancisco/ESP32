/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "app_storage.h"

#include "nvs.h"

#define APP_STORAGE_NAMESPACE "app"

uint8_t app_storage_get_u8(const char *key, uint8_t default_value)
{
    nvs_handle_t handle;
    uint8_t value = default_value;
    uint8_t stored;

    if (nvs_open(APP_STORAGE_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        if (nvs_get_u8(handle, key, &stored) == ESP_OK) {
            value = stored;
        }
        nvs_close(handle);
    }

    return value;
}

void app_storage_set_u8(const char *key, uint8_t value)
{
    nvs_handle_t handle;

    /* A missing key reads back as ~value, so it is always written */
    if (app_storage_get_u8(key, (uint8_t)~value) == value) {
        return;
    }

    if (nvs_open(APP_STORAGE_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, key, value);
        nvs_commit(handle);
        nvs_close(handle);
    }
}
